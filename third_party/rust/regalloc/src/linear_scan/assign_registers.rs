use super::{
    last_use, next_use, Fragments, IntId, Intervals, LiveInterval, LiveIntervalKind, Location,
    Mention, MentionMap, RegUses,
};
use crate::{
    avl_tree::{AVLTree, AVL_NULL},
    data_structures::{
        InstPoint, Point, RangeFrag, RangeFragIx, RegVecsAndBounds, SortedRangeFragIxs, SpillCost,
        VirtualRange, VirtualRangeIx,
    },
    Function, InstIx, RealReg, RealRegUniverse, Reg, RegAllocError, RegClass, SpillSlot,
    VirtualReg, NUM_REG_CLASSES,
};

use log::{debug, info, log_enabled, trace, Level};
use rustc_hash::FxHashMap as HashMap;
use smallvec::{Array, SmallVec};
use std::cmp::Ordering;
use std::env;
use std::mem;

pub(crate) fn run<F: Function>(
    func: &F,
    reg_uses: &RegVecsAndBounds,
    reg_universe: &RealRegUniverse,
    scratches_by_rc: &Vec<Option<RealReg>>,
    intervals: Intervals,
    fragments: Fragments,
) -> Result<(HashMap<Reg, MentionMap>, Fragments, Intervals, u32), RegAllocError> {
    // Subset of fixed intervals.
    let mut fixed_intervals = intervals
        .data
        .iter()
        .filter_map(|int| if int.is_fixed() { Some(int.id) } else { None })
        .collect::<Vec<_>>();
    fixed_intervals.sort_by_key(|&id| intervals.get(id).start);

    let mut state = State::new(func, &reg_uses, fragments, intervals);
    let mut reusable = ReusableState::new(reg_universe, &scratches_by_rc);

    #[cfg(debug_assertions)]
    let mut prev_start = None;

    let mut last_fixed = 0;

    while let Some(id) = state.next_unhandled() {
        info!("main loop: allocating {:?}", id);

        #[cfg(debug_assertions)]
        {
            let start = state.intervals.get(id).start;
            if let Some(ref prev) = prev_start {
                debug_assert!(*prev <= start, "main loop must make progress");
            };
            prev_start = Some(start);
        }

        if state.intervals.get(id).location.is_none() {
            // Lazily push all the fixed intervals that might interfere with the
            // current interval to the inactive list.
            let int = state.intervals.get(id);
            while last_fixed < fixed_intervals.len()
                && state.intervals.get(fixed_intervals[last_fixed]).start <= int.end
            {
                // Maintain active/inactive state for match_previous_update_state.
                #[cfg(debug_assertions)]
                state.inactive.push(fixed_intervals[last_fixed]);

                {
                    let intervals = &state.intervals;
                    let fragments = &state.fragments;
                    state.interval_tree.insert(
                        (fixed_intervals[last_fixed], 0),
                        Some(&|left, right| cmp_interval_tree(left, right, intervals, fragments)),
                    );
                }

                last_fixed += 1;
            }

            update_state(&mut reusable, id, &mut state);

            let (allocated, inactive_intersecting) =
                try_allocate_reg(&mut reusable, id, &mut state);
            if !allocated {
                allocate_blocked_reg(
                    &mut reusable,
                    id,
                    &mut state,
                    inactive_intersecting.unwrap(),
                )?;
            }

            {
                // Maintain active/inactive state for match_previous_update_state.
                if state.intervals.get(id).location.reg().is_some() {
                    // Add the current interval to the interval tree, if it's been
                    // allocated.
                    let fragments = &state.fragments;
                    let intervals = &state.intervals;
                    state.interval_tree.insert(
                        (id, 0),
                        Some(&|left, right| cmp_interval_tree(left, right, intervals, fragments)),
                    );

                    #[cfg(debug_assertions)]
                    state.active.push(id);
                }
            }
        }

        debug!("");
    }

    if log_enabled!(Level::Debug) {
        debug!("allocation results (in order):");
        for id in 0..state.intervals.data.len() {
            debug!("{}", state.intervals.display(IntId(id), &state.fragments));
        }
        debug!("");
    }

    Ok((
        state.mention_map,
        state.fragments,
        state.intervals,
        state.next_spill_slot.get(),
    ))
}

/// A mapping from real reg to some T.
#[derive(Clone)]
struct RegisterMapping<T> {
    offset: usize,
    regs: Vec<(RealReg, T)>,
    scratch: Option<RealReg>,
    initial_value: T,
    reg_class_index: usize,
}

impl<T: Copy> RegisterMapping<T> {
    fn with_default(
        reg_class_index: usize,
        reg_universe: &RealRegUniverse,
        scratch: Option<RealReg>,
        initial_value: T,
    ) -> Self {
        let mut regs = Vec::new();
        let mut offset = 0;
        // Collect all the registers for the current class.
        if let Some(ref info) = reg_universe.allocable_by_class[reg_class_index] {
            debug_assert!(info.first <= info.last);
            offset = info.first;
            for reg in &reg_universe.regs[info.first..=info.last] {
                debug_assert!(regs.len() == reg.0.get_index() - offset);
                regs.push((reg.0, initial_value));
            }
        };
        Self {
            offset,
            regs,
            scratch,
            initial_value,
            reg_class_index,
        }
    }

    fn clear(&mut self) {
        for reg in self.regs.iter_mut() {
            reg.1 = self.initial_value;
        }
    }

    fn iter<'a>(&'a self) -> RegisterMappingIter<T> {
        RegisterMappingIter {
            iter: self.regs.iter(),
            scratch: self.scratch,
        }
    }
}

struct RegisterMappingIter<'a, T: Copy> {
    iter: std::slice::Iter<'a, (RealReg, T)>,
    scratch: Option<RealReg>,
}

impl<'a, T: Copy> std::iter::Iterator for RegisterMappingIter<'a, T> {
    type Item = &'a (RealReg, T);
    fn next(&mut self) -> Option<Self::Item> {
        match self.iter.next() {
            Some(pair) => {
                if Some(pair.0) == self.scratch {
                    // Skip to the next one.
                    self.iter.next()
                } else {
                    Some(pair)
                }
            }
            None => None,
        }
    }
}

impl<T> std::ops::Index<RealReg> for RegisterMapping<T> {
    type Output = T;
    fn index(&self, rreg: RealReg) -> &Self::Output {
        debug_assert!(
            rreg.get_class() as usize == self.reg_class_index,
            "trying to index a reg from the wrong class"
        );
        debug_assert!(Some(rreg) != self.scratch, "trying to use the scratch");
        &self.regs[rreg.get_index() - self.offset].1
    }
}

impl<T> std::ops::IndexMut<RealReg> for RegisterMapping<T> {
    fn index_mut(&mut self, rreg: RealReg) -> &mut Self::Output {
        debug_assert!(
            rreg.get_class() as usize == self.reg_class_index,
            "trying to index a reg from the wrong class"
        );
        debug_assert!(Some(rreg) != self.scratch, "trying to use the scratch");
        &mut self.regs[rreg.get_index() - self.offset].1
    }
}

// State management.

/// Parts of state just reused for recycling memory.
struct ReusableState {
    reg_to_instpoint_1: Vec<RegisterMapping<InstPoint>>,
    reg_to_instpoint_2: Vec<RegisterMapping<InstPoint>>,
    vec_u32: Vec<u32>,
    interval_tree_updates: Vec<(IntId, usize, usize)>,
    interval_tree_deletes: Vec<(IntId, usize)>,
}

impl ReusableState {
    fn new(reg_universe: &RealRegUniverse, scratches: &[Option<RealReg>]) -> Self {
        let mut reg_to_instpoint_1 = Vec::with_capacity(NUM_REG_CLASSES);

        for i in 0..NUM_REG_CLASSES {
            let scratch = scratches[i];
            reg_to_instpoint_1.push(RegisterMapping::with_default(
                i,
                reg_universe,
                scratch,
                InstPoint::max_value(),
            ));
        }

        let reg_to_instpoint_2 = reg_to_instpoint_1.clone();

        Self {
            reg_to_instpoint_1,
            reg_to_instpoint_2,
            vec_u32: Vec::new(),
            interval_tree_updates: Vec::new(),
            interval_tree_deletes: Vec::new(),
        }
    }
}

/// State structure, which can be cleared between different calls to register allocation.
/// TODO: split this into clearable fields and non-clearable fields.
struct State<'a, F: Function> {
    func: &'a F,
    reg_uses: &'a RegUses,

    optimal_split_strategy: OptimalSplitStrategy,

    fragments: Fragments,
    intervals: Intervals,

    interval_tree: AVLTree<(IntId, usize)>,

    /// Intervals that are starting after the current interval's start position.
    unhandled: AVLTree<IntId>,

    /// Intervals that are covering the current interval's start position.
    active: Vec<IntId>,

    /// Intervals that are not covering but end after the current interval's start
    /// position.
    inactive: Vec<IntId>,

    /// Next available spill slot.
    next_spill_slot: SpillSlot,

    /// Maps given virtual registers to the spill slots they should be assigned
    /// to.
    spill_map: HashMap<VirtualReg, SpillSlot>,

    mention_map: HashMap<Reg, MentionMap>,
}

impl<'a, F: Function> State<'a, F> {
    fn new(func: &'a F, reg_uses: &'a RegUses, fragments: Fragments, intervals: Intervals) -> Self {
        // Trick! Keep unhandled in reverse sorted order, so we can just pop
        // unhandled ids instead of shifting the first element.
        let mut unhandled = AVLTree::new(IntId(usize::max_value()));
        for int in intervals.data.iter() {
            unhandled.insert(
                int.id,
                Some(&|left: IntId, right: IntId| {
                    (intervals.data[left.0].start, left)
                        .partial_cmp(&(intervals.data[right.0].start, right))
                }),
            );
        }

        // Useful for debugging.
        let optimal_split_strategy = match env::var("SPLIT") {
            Ok(s) => match s.as_str() {
                "t" | "to" => OptimalSplitStrategy::To,
                "n" => OptimalSplitStrategy::NextFrom,
                "nn" => OptimalSplitStrategy::NextNextFrom,
                "p" => OptimalSplitStrategy::PrevTo,
                "pp" => OptimalSplitStrategy::PrevPrevTo,
                "m" | "mid" => OptimalSplitStrategy::Mid,
                _ => OptimalSplitStrategy::From,
            },
            Err(_) => OptimalSplitStrategy::From,
        };

        Self {
            func,
            reg_uses,
            optimal_split_strategy,
            fragments,
            intervals,
            unhandled,
            active: Vec::new(),
            inactive: Vec::new(),
            next_spill_slot: SpillSlot::new(0),
            spill_map: HashMap::default(),
            interval_tree: AVLTree::new((IntId(usize::max_value()), usize::max_value())),
            mention_map: build_mention_map(reg_uses),
        }
    }

    fn next_unhandled(&mut self) -> Option<IntId> {
        // Left-most entry in tree is the next interval to handle.
        let mut pool_id = self.unhandled.root;
        if pool_id == AVL_NULL {
            return None;
        }

        loop {
            let left = self.unhandled.pool[pool_id as usize].left;
            if left == AVL_NULL {
                break;
            }
            pool_id = left;
        }
        let id = self.unhandled.pool[pool_id as usize].item;

        let intervals = &self.intervals;
        let deleted = self.unhandled.delete(
            id,
            Some(&|left: IntId, right: IntId| {
                (intervals.data[left.0].start, left)
                    .partial_cmp(&(intervals.data[right.0].start, right))
            }),
        );
        debug_assert!(deleted);

        Some(id)
    }

    fn insert_unhandled(&mut self, id: IntId) {
        let intervals = &self.intervals;
        let inserted = self.unhandled.insert(
            id,
            Some(&|left: IntId, right: IntId| {
                (intervals.data[left.0].start, left)
                    .partial_cmp(&(intervals.data[right.0].start, right))
            }),
        );
        debug_assert!(inserted);
    }

    fn spill(&mut self, id: IntId) {
        let int = self.intervals.get(id);
        debug_assert!(!int.is_fixed(), "can't split fixed interval");
        debug_assert!(int.location.spill().is_none(), "already spilled");
        debug!("spilling {:?}", id);

        let vreg = self.intervals.vreg(id);
        let spill_slot = if let Some(spill_slot) = self.spill_map.get(&vreg) {
            *spill_slot
        } else {
            let size_slot = self.func.get_spillslot_size(int.reg_class, vreg);
            let spill_slot = self.next_spill_slot.round_up(size_slot);
            self.next_spill_slot = self.next_spill_slot.inc(1);
            self.spill_map.insert(vreg, spill_slot);
            spill_slot
        };

        self.intervals.set_spill(id, spill_slot);
    }
}

/// Checks that the update_state algorithm matches the naive way to perform the
/// update.
#[cfg(debug_assertions)]
fn match_previous_update_state(
    start_point: InstPoint,
    active: &Vec<IntId>,
    inactive: &Vec<IntId>,
    expired: &Vec<IntId>,
    prev_active: Vec<IntId>,
    prev_inactive: Vec<IntId>,
    intervals: &Intervals,
    fragments: &Fragments,
) -> Result<bool, &'static str> {
    // Make local mutable copies.
    let mut active = active.clone();
    let mut inactive = inactive.clone();
    let mut expired = expired.clone();

    for &int_id in &active {
        if start_point > intervals.get(int_id).end {
            return Err("active should have expired");
        }
        if !intervals.covers(int_id, start_point, fragments) {
            return Err("active should contain start pos");
        }
    }

    for &int_id in &inactive {
        if intervals.covers(int_id, start_point, fragments) {
            return Err("inactive should not contain start pos");
        }
        if start_point > intervals.get(int_id).end {
            return Err("inactive should have expired");
        }
    }

    for &int_id in &expired {
        if intervals.covers(int_id, start_point, fragments) {
            return Err("expired shouldn't cover target");
        }
        if intervals.get(int_id).end >= start_point {
            return Err("expired shouldn't have expired");
        }
    }

    let mut other_active = Vec::new();
    let mut other_inactive = Vec::new();
    let mut other_expired = Vec::new();
    for &id in &prev_active {
        if intervals.get(id).location.spill().is_some() {
            continue;
        }
        if intervals.get(id).end < start_point {
            // It's expired, forget about it.
            other_expired.push(id);
        } else if intervals.covers(id, start_point, fragments) {
            other_active.push(id);
        } else {
            other_inactive.push(id);
        }
    }
    for &id in &prev_inactive {
        if intervals.get(id).location.spill().is_some() {
            continue;
        }
        if intervals.get(id).end < start_point {
            // It's expired, forget about it.
            other_expired.push(id);
        } else if intervals.covers(id, start_point, fragments) {
            other_active.push(id);
        } else {
            other_inactive.push(id);
        }
    }

    other_active.sort_by_key(|&id| (intervals.get(id).start, id));
    active.sort_by_key(|&id| (intervals.get(id).start, id));
    other_inactive.sort_by_key(|&id| (intervals.get(id).start, id));
    inactive.sort_by_key(|&id| (intervals.get(id).start, id));
    other_expired.sort_by_key(|&id| (intervals.get(id).start, id));
    expired.sort_by_key(|&id| (intervals.get(id).start, id));

    trace!("active: reference/fast algo");
    trace!("{:?}", other_active);
    trace!("{:?}", active);
    trace!("inactive: reference/fast algo");
    trace!("{:?}", other_inactive);
    trace!("{:?}", inactive);
    trace!("expired: reference/fast algo");
    trace!("{:?}", other_expired);
    trace!("{:?}", expired);

    if other_active.len() != active.len() {
        return Err("diff in active.len()");
    }
    for (&other, &next) in other_active.iter().zip(active.iter()) {
        if other != next {
            return Err("diff in active");
        }
    }

    if other_inactive.len() != inactive.len() {
        return Err("diff in inactive.len()");
    };
    for (&other, &next) in other_inactive.iter().zip(inactive.iter()) {
        if other != next {
            return Err("diff in inactive");
        }
    }

    if other_expired.len() != expired.len() {
        return Err("diff in expired.len()");
    };
    for (&other, &next) in other_expired.iter().zip(expired.iter()) {
        if other != next {
            return Err("diff in expired");
        }
    }

    Ok(true)
}

#[inline(never)]
fn lazy_compute_inactive(
    reusable_vec_u32: &mut Vec<u32>,
    intervals: &Intervals,
    interval_tree: &AVLTree<(IntId, usize)>,
    active: &[IntId],
    _prev_inactive: &[IntId],
    fragments: &Fragments,
    cur_id: IntId,
    inactive_intersecting: &mut Vec<(IntId, InstPoint)>,
) {
    debug_assert_eq!(intervals.fragments(cur_id).len(), 1);
    let int = intervals.get(cur_id);
    let reg_class = int.reg_class;
    //let cur_start = int.start;
    let cur_end = int.end;

    for (id, _last_frag) in interval_tree.iter(reusable_vec_u32).skip(active.len()) {
        let other_int = intervals.get(id);
        debug_assert!(other_int.is_fixed() || intervals.fragments(id).len() == 1);

        if cur_end < other_int.start {
            break;
        }

        if other_int.reg_class != reg_class {
            continue;
        }

        debug_assert!(other_int.location.reg().is_some());
        if other_int.is_fixed() {
            if let Some(intersect_at) = intervals.intersects_with(id, cur_id, fragments) {
                inactive_intersecting.push((id, intersect_at));
            }
        } else {
            // cur_start < frag.start, otherwise the interval would be active.
            debug_assert!(other_int.start <= cur_end);
            debug_assert!(
                intervals.intersects_with(id, cur_id, fragments) == Some(other_int.start)
            );
            inactive_intersecting.push((id, other_int.start));
        }
    }

    #[cfg(debug_assertions)]
    {
        let former_inactive = {
            let mut inactive = Vec::new();
            for &id in _prev_inactive {
                if intervals.get(id).reg_class != reg_class {
                    continue;
                }
                if let Some(pos) = intervals.intersects_with(id, cur_id, fragments) {
                    inactive.push((id, pos));
                }
            }
            inactive.sort();
            inactive
        };
        inactive_intersecting.sort();
        trace!("inactive: reference/faster");
        trace!("{:?}", former_inactive,);
        trace!("{:?}", inactive_intersecting,);
        debug_assert_eq!(former_inactive.len(), inactive_intersecting.len());
        debug_assert_eq!(former_inactive, *inactive_intersecting);
    }
}

/// Transitions intervals from active/inactive into active/inactive/handled.
///
/// An interval tree is stored in the state, containing all the active and
/// inactive intervals. The comparison key is the interval's start point.
///
/// A state update consists in the following. We consider the next interval to
/// allocate, and in particular its start point S.
///
/// 1. remove all the active/inactive intervals that have expired, i.e. their
///    end point is before S.
/// 2. reconsider active/inactive intervals:
///   - if they contain S, they become (or stay) active.
///   - otherwise, they become (or stay) inactive.
///
/// Item 1 is easy to implement, and fast enough.
///
/// Item 2 is a bit trickier. While we could just call `Intervals::covers` for
/// each interval on S, this is quite expensive. In addition to this, it happens
/// that most intervals are inactive. This is explained by the fact that linear
/// scan can create large intervals, if a value is used much later after it's
/// been created, *according to the block ordering*.
///
/// For each interval, we remember the last active fragment, or the first
/// inactive fragment that starts after S. This makes search really fast:
///
/// - if the considered (active or inactive) interval start is before S, then we
/// should look more precisely if it's active or inactive. This might include
/// seeking to the next fragment that contains S.
/// - otherwise, if the considered interval start is *after* S, then it means
/// this interval, as well as all the remaining ones in the interval tree (since
/// they're sorted by starting position) are inactive, and we can escape the
/// loop eagerly.
///
/// The escape for inactive intervals make this function overall cheap.
#[inline(never)]
fn update_state<'a, F: Function>(
    reusable: &mut ReusableState,
    cur_id: IntId,
    state: &mut State<'a, F>,
) {
    let intervals = &mut state.intervals;

    let start_point = intervals.get(cur_id).start;

    #[cfg(debug_assertions)]
    let prev_active = state.active.clone();
    #[cfg(debug_assertions)]
    let prev_inactive = state.inactive.clone();

    let mut next_active = Vec::new();
    mem::swap(&mut state.active, &mut next_active);
    next_active.clear();

    let mut next_inactive = Vec::new();
    mem::swap(&mut state.inactive, &mut next_inactive);
    next_inactive.clear();

    let fragments = &state.fragments;
    let comparator = |left, right| cmp_interval_tree(left, right, intervals, fragments);

    #[cfg(debug_assertions)]
    let mut expired = Vec::new();

    let mut next_are_all_inactive = false;

    for (int_id, last_frag_idx) in state.interval_tree.iter(&mut reusable.vec_u32) {
        if next_are_all_inactive {
            next_inactive.push(int_id);
            continue;
        }

        let int = intervals.get(int_id);

        // Skip expired intervals.
        if int.end < start_point {
            #[cfg(debug_assertions)]
            expired.push(int.id);
            reusable.interval_tree_deletes.push((int.id, last_frag_idx));
            continue;
        }

        // From this point, start <= int.end.
        let frag_ixs = &intervals.fragments(int_id);
        let mut cur_frag = &state.fragments[frag_ixs[last_frag_idx]];

        // If the current fragment still contains start, it is still active.
        if cur_frag.contains(&start_point) {
            next_active.push(int_id);
            continue;
        }

        if start_point < cur_frag.first {
            // This is the root of the optimization: all the remaining intervals,
            // including this one, are now inactive, so we can skip them.
            next_inactive.push(int_id);
            next_are_all_inactive = true;
            continue;
        }

        // Otherwise, fast-forward to the next fragment that starts after start.
        // It exists, because start <= int.end.
        let mut new_frag_idx = last_frag_idx + 1;

        while new_frag_idx < frag_ixs.len() {
            cur_frag = &state.fragments[frag_ixs[new_frag_idx]];
            if start_point <= cur_frag.last {
                break;
            }
            new_frag_idx += 1;
        }

        debug_assert!(new_frag_idx != frag_ixs.len());

        // In all the cases, update the interval so its last fragment is now the
        // one we'd expect.
        reusable
            .interval_tree_updates
            .push((int_id, last_frag_idx, new_frag_idx));

        if start_point >= cur_frag.first {
            // Now active.
            next_active.push(int_id);
        } else {
            // Now inactive.
            next_inactive.push(int_id);
        }
    }

    for &(int_id, from_idx, to_idx) in &reusable.interval_tree_updates {
        let deleted_2 = state
            .interval_tree
            .delete((int_id, from_idx), Some(&comparator));
        debug_assert!(deleted_2);
        let inserted_1 = state
            .interval_tree
            .insert((int_id, to_idx), Some(&comparator));
        debug_assert!(inserted_1);
    }

    for &(int_id, from_idx) in &reusable.interval_tree_deletes {
        let deleted_1 = state
            .interval_tree
            .delete((int_id, from_idx), Some(&comparator));
        debug_assert!(deleted_1);
    }

    for &(int_id, _from_idx, to_idx) in &reusable.interval_tree_updates {
        intervals.get_mut(int_id).last_frag = to_idx;
    }

    reusable.interval_tree_updates.clear();
    reusable.interval_tree_deletes.clear();

    #[cfg(debug_assertions)]
    debug_assert!(match_previous_update_state(
        start_point,
        &next_active,
        &next_inactive,
        &expired,
        prev_active,
        prev_inactive,
        &state.intervals,
        &state.fragments
    )
    .unwrap());

    state.active = next_active;
    state.inactive = next_inactive;

    trace!("state active: {:?}", state.active);
    trace!("state inactive: {:?}", state.inactive);
}

/// Naive heuristic to select a register when we're not aware of any conflict.
/// Currently, it chooses the register with the furthest next use.
#[inline(never)]
fn select_naive_reg<F: Function>(
    reusable: &mut ReusableState,
    state: &mut State<F>,
    id: IntId,
    reg_class: RegClass,
    inactive_intersecting: &mut Vec<(IntId, InstPoint)>,
) -> Option<(RealReg, InstPoint)> {
    let free_until_pos = &mut reusable.reg_to_instpoint_1[reg_class as usize];
    free_until_pos.clear();

    let mut num_free = usize::max(1, free_until_pos.regs.len()) - 1;

    // All registers currently in use are blocked.
    for &id in &state.active {
        if let Some(reg) = state.intervals.get(id).location.reg() {
            if reg.get_class() == reg_class {
                free_until_pos[reg] = InstPoint::min_value();
                num_free -= 1;
            }
        }
    }

    // Shortcut: if all the registers are taken, don't even bother.
    if num_free == 0 {
        return None;
    }

    // All registers that would be used at the same time as the current interval
    // are partially blocked, up to the point when they start being used.
    lazy_compute_inactive(
        &mut reusable.vec_u32,
        &state.intervals,
        &state.interval_tree,
        &state.active,
        &state.inactive,
        &state.fragments,
        id,
        inactive_intersecting,
    );

    for &(id, intersect_at) in inactive_intersecting.iter() {
        let reg = state.intervals.get(id).location.unwrap_reg();
        if intersect_at < free_until_pos[reg] {
            free_until_pos[reg] = intersect_at;
        }
    }

    // Find the register with the furthest next use, if there's any.
    let mut best_reg = None;
    let mut best_pos = InstPoint::min_value();
    for &(reg, pos) in free_until_pos.iter() {
        if pos > best_pos {
            best_pos = pos;
            best_reg = Some(reg);
        }
    }

    best_reg.and_then(|reg| Some((reg, best_pos)))
}

#[inline(never)]
fn try_allocate_reg<F: Function>(
    reusable: &mut ReusableState,
    id: IntId,
    state: &mut State<F>,
) -> (bool, Option<Vec<(IntId, InstPoint)>>) {
    let reg_class = state.intervals.get(id).reg_class;

    let mut inactive_intersecting = Vec::new();
    let (best_reg, best_pos) = if let Some(solution) =
        select_naive_reg(reusable, state, id, reg_class, &mut inactive_intersecting)
    {
        solution
    } else {
        debug!("try_allocate_reg: all registers taken, need to spill.");
        return (false, Some(inactive_intersecting));
    };
    debug!(
        "try_allocate_reg: best register {:?} has next use at {:?}",
        best_reg, best_pos
    );

    if best_pos <= state.intervals.get(id).end {
        // TODO Here, it should be possible to split the interval between the start
        // (or more precisely, the last use before best_pos) and the best_pos value.
        // See also issue #32.
        return (false, Some(inactive_intersecting));
    }

    // At least a partial match: allocate.
    debug!("{:?}: {:?} <- {:?}", id, state.intervals.vreg(id), best_reg);
    state.intervals.set_reg(id, best_reg);

    (true, None)
}

#[inline(never)]
fn allocate_blocked_reg<F: Function>(
    reusable: &mut ReusableState,
    cur_id: IntId,
    state: &mut State<F>,
    mut inactive_intersecting: Vec<(IntId, InstPoint)>,
) -> Result<(), RegAllocError> {
    // If the current interval has no uses, spill it directly.
    let first_use = match next_use(
        &state.mention_map,
        &state.intervals,
        cur_id,
        InstPoint::min_value(),
        &state.reg_uses,
        &state.fragments,
    ) {
        Some(u) => u,
        None => {
            state.spill(cur_id);
            return Ok(());
        }
    };

    let (start_pos, reg_class) = {
        let int = state.intervals.get(cur_id);
        (int.start, int.reg_class)
    };

    // Note: in this function, "use" isn't just a use as in use-def; it really
    // means a mention, so either a use or a definition.
    //
    // 1. Compute all the positions of next uses for registers of active intervals
    // and inactive intervals that might intersect with the current one.
    // 2. Then use this to select the interval with the further next use.
    // 3. Spill either the current interval or active/inactive intervals with the
    //    selected register.
    // 4. Make sure that the current interval doesn't intersect with the fixed
    //    interval for the selected register.

    // Step 1: compute all the next use positions.
    let next_use_pos = &mut reusable.reg_to_instpoint_1[reg_class as usize];
    next_use_pos.clear();

    let block_pos = &mut reusable.reg_to_instpoint_2[reg_class as usize];
    block_pos.clear();

    trace!(
        "allocate_blocked_reg: searching reg with next use after {:?}",
        start_pos
    );

    for &id in &state.active {
        let int = state.intervals.get(id);
        if int.reg_class != reg_class {
            continue;
        }
        if let Some(reg) = state.intervals.get(id).location.reg() {
            if int.is_fixed() {
                block_pos[reg] = InstPoint::min_value();
                next_use_pos[reg] = InstPoint::min_value();
            } else if next_use_pos[reg] != InstPoint::min_value() {
                if let Some(reg) = state.intervals.get(id).location.reg() {
                    if let Some(next_use) = next_use(
                        &state.mention_map,
                        &state.intervals,
                        id,
                        start_pos,
                        &state.reg_uses,
                        &state.fragments,
                    ) {
                        next_use_pos[reg] = InstPoint::min(next_use_pos[reg], next_use);
                    }
                }
            }
        }
    }

    if inactive_intersecting.len() == 0 {
        lazy_compute_inactive(
            &mut reusable.vec_u32,
            &state.intervals,
            &state.interval_tree,
            &state.active,
            &state.inactive,
            &state.fragments,
            cur_id,
            &mut inactive_intersecting,
        );
    }

    for &(id, intersect_pos) in &inactive_intersecting {
        debug_assert!(!state.active.iter().any(|active_id| *active_id == id));
        debug_assert!(state.intervals.get(id).reg_class == reg_class);

        let reg = state.intervals.get(id).location.unwrap_reg();
        if block_pos[reg] == InstPoint::min_value() {
            // This register is already blocked.
            debug_assert!(next_use_pos[reg] == InstPoint::min_value());
            continue;
        }

        if state.intervals.get(id).is_fixed() {
            block_pos[reg] = InstPoint::min(block_pos[reg], intersect_pos);
            next_use_pos[reg] = InstPoint::min(next_use_pos[reg], intersect_pos);
        } else if let Some(reg) = state.intervals.get(id).location.reg() {
            if let Some(next_use) = next_use(
                &state.mention_map,
                &state.intervals,
                id,
                intersect_pos,
                &state.reg_uses,
                &state.fragments,
            ) {
                next_use_pos[reg] = InstPoint::min(next_use_pos[reg], next_use);
            }
        }
    }

    // Step 2: find the register with the furthest next use.
    let best_reg = {
        let mut best = None;
        for (reg, pos) in next_use_pos.iter() {
            trace!("allocate_blocked_reg: {:?} has next use at {:?}", reg, pos);
            match best {
                None => best = Some((reg, pos)),
                Some((ref mut best_reg, ref mut best_pos)) => {
                    if *best_pos < pos {
                        *best_pos = pos;
                        *best_reg = reg;
                    }
                }
            }
        }
        match best {
            Some(best) => *best.0,
            None => {
                return Err(RegAllocError::Other(format!(
                    "the {:?} register class has no registers!",
                    reg_class
                )));
            }
        }
    };
    debug!(
        "selecting blocked register {:?} with furthest next use at {:?}",
        best_reg, next_use_pos[best_reg]
    );

    // Step 3: if the next use of the current interval is after the furthest use
    // of the selected register, then we should spill the current interval.
    // Otherwise, spill other intervals.
    debug!(
        "current first used at {:?}, next use of best reg at {:?}",
        first_use, next_use_pos[best_reg]
    );

    if first_use >= next_use_pos[best_reg] {
        if first_use == start_pos {
            return Err(RegAllocError::OutOfRegisters(reg_class));
        }
        debug!("spill current interval");
        let new_int = split(state, cur_id, first_use);
        state.insert_unhandled(new_int);
        state.spill(cur_id);
    } else {
        debug!("taking over register, spilling intersecting intervals");

        // Spill intervals that currently block the selected register.
        state.intervals.set_reg(cur_id, best_reg);

        // If there's an interference with a fixed interval, split at the
        // intersection.
        let int_end = state.intervals.get(cur_id).end;
        if block_pos[best_reg] <= int_end {
            debug!(
                "allocate_blocked_reg: fixed conflict! blocked at {:?}, while ending at {:?}",
                block_pos[best_reg], int_end
            );
            // TODO Here, it should be possible to only split the interval, and not
            // spill it. See also issue #32.
            split_and_spill(state, cur_id, block_pos[best_reg]);
        }

        for &id in &state.active {
            let int = state.intervals.get(id);
            if int.reg_class != reg_class {
                continue;
            }
            if let Some(reg) = int.location.reg() {
                if reg == best_reg {
                    // spill it!
                    debug!("allocate_blocked_reg: split and spill active stolen reg");
                    split_and_spill(state, id, start_pos);
                    break;
                }
            }
        }

        for (id, _intersect_pos) in inactive_intersecting {
            let int = state.intervals.get(id);
            if int.is_fixed() {
                continue;
            }
            let reg = int.location.unwrap_reg();
            debug_assert_eq!(reg.get_class(), reg_class);
            if reg == best_reg {
                debug!("allocate_blocked_reg: split and spill inactive stolen reg");
                // start_pos is in the middle of a hole in the split interval
                // (otherwise it'd be active), so it's a great split position.
                split_and_spill(state, id, start_pos);
            }
        }
    }

    Ok(())
}

/// Which strategy should we use when trying to find the best split position?
/// TODO Consider loop depth to avoid splitting in the middle of a loop
/// whenever possible.
enum OptimalSplitStrategy {
    From,
    To,
    NextFrom,
    NextNextFrom,
    PrevTo,
    PrevPrevTo,
    Mid,
}

/// Finds an optimal split position, whenever we're given a range of possible
/// positions where to split.
fn find_optimal_split_pos<F: Function>(
    state: &State<F>,
    id: IntId,
    from: InstPoint,
    to: InstPoint,
) -> InstPoint {
    trace!("find_optimal_split_pos between {:?} and {:?}", from, to);

    debug_assert!(from <= to, "split between positions are inconsistent");
    let int = state.intervals.get(id);
    debug_assert!(from >= int.start, "split should happen after the start");
    debug_assert!(to <= int.end, "split should happen before the end");

    if from == to {
        return from;
    }

    let candidate = match state.optimal_split_strategy {
        OptimalSplitStrategy::To => Some(to),
        OptimalSplitStrategy::NextFrom => Some(next_pos(from)),
        OptimalSplitStrategy::NextNextFrom => Some(next_pos(next_pos(from))),
        OptimalSplitStrategy::From => {
            // This is the general setting, so win some time and eagerly return here.
            return from;
        }
        OptimalSplitStrategy::PrevTo => Some(prev_pos(to)),
        OptimalSplitStrategy::PrevPrevTo => Some(prev_pos(prev_pos(to))),
        OptimalSplitStrategy::Mid => Some(InstPoint::new_use(InstIx::new(
            (from.iix.get() + to.iix.get()) / 2,
        ))),
    };

    if let Some(pos) = candidate {
        if pos >= from && pos <= to && state.intervals.covers(id, pos, &state.fragments) {
            return pos;
        }
    }

    from
}

fn prev_pos(mut pos: InstPoint) -> InstPoint {
    match pos.pt {
        Point::Def => {
            pos.pt = Point::Use;
            pos
        }
        Point::Use => {
            pos.iix = pos.iix.minus(1);
            pos.pt = Point::Def;
            pos
        }
        _ => unreachable!(),
    }
}

fn next_pos(mut pos: InstPoint) -> InstPoint {
    match pos.pt {
        Point::Use => pos.pt = Point::Def,
        Point::Def => {
            pos.pt = Point::Use;
            pos.iix = pos.iix.plus(1);
        }
        _ => unreachable!(),
    };
    pos
}

/// Splits the given interval between the last use before `split_pos` and
/// `split_pos`.
///
/// In case of two-ways split (i.e. only place to split is precisely split_pos),
/// returns the live interval id for the middle child, to be added back to the
/// list of active/inactive intervals after iterating on these.
fn split_and_spill<F: Function>(state: &mut State<F>, id: IntId, split_pos: InstPoint) {
    let child = match last_use(
        &state.mention_map,
        &state.intervals,
        id,
        split_pos,
        &state.reg_uses,
        &state.fragments,
    ) {
        Some(last_use) => {
            debug!(
                "split_and_spill {:?}: spill between {:?} and {:?}",
                id, last_use, split_pos
            );

            // Maintain ascending order between the min and max positions.
            let min_pos = InstPoint::min(next_pos(last_use), split_pos);

            // Make sure that if the two positions are the same, we'll be splitting in
            // a position that's in the current interval.
            let optimal_pos = find_optimal_split_pos(state, id, min_pos, split_pos);

            let child = split(state, id, optimal_pos);
            state.spill(child);
            child
        }

        None => {
            // The current interval has no uses before the split position, it can
            // safely be spilled.
            debug!(
                "split_and_spill {:?}: spilling it since no uses before split position",
                id
            );
            state.spill(id);
            id
        }
    };

    // Split until the next register use.
    match next_use(
        &state.mention_map,
        &state.intervals,
        child,
        split_pos,
        &state.reg_uses,
        &state.fragments,
    ) {
        Some(next_use_pos) => {
            debug!(
                "split spilled interval before next use @ {:?}",
                next_use_pos
            );
            let child = split(state, child, next_use_pos);
            state.insert_unhandled(child);
        }
        None => {
            // Let it be spilled for the rest of its lifetime.
        }
    }

    // In both cases, the spilled child interval can remain on the stack.
    debug!("spilled split child {:?} silently expires", child);
}

/// Splits the interval at the given position.
///
/// The split position must either be a Def of the current vreg, or it must be
/// at a Use position (otherwise there's no place to put the moves created by
/// the split).
///
/// The id of the new interval is returned, while the parent interval is mutated
/// in place. The child interval starts after (including) at_pos.
fn split<F: Function>(state: &mut State<F>, id: IntId, at_pos: InstPoint) -> IntId {
    debug!("split {:?} at {:?}", id, at_pos);
    if log_enabled!(Level::Trace) {
        trace!(
            "interval: {}",
            state.intervals.display(id, &state.fragments),
        );
    }

    let parent_start = state.intervals.get(id).start;
    debug_assert!(parent_start <= at_pos, "must split after the start");
    debug_assert!(
        at_pos <= state.intervals.get(id).end,
        "must split before the end"
    );

    {
        // Remove the parent from the interval tree, if it was there.
        let intervals = &state.intervals;
        let fragments = &state.fragments;
        state.interval_tree.delete(
            (id, state.intervals.get(id).last_frag),
            Some(&|left, right| cmp_interval_tree(left, right, intervals, fragments)),
        );
    }
    if state.intervals.get(id).location.reg().is_some() {
        // If the interval was set to a register, reset it to the first fragment.
        state.intervals.get_mut(id).last_frag = 0;
        let intervals = &state.intervals;
        let fragments = &state.fragments;
        state.interval_tree.insert(
            (id, 0),
            Some(&|left, right| cmp_interval_tree(left, right, intervals, fragments)),
        );
    }

    let vreg = state.intervals.vreg(id);
    let fragments = &state.fragments;
    let frags = state.intervals.fragments_mut(id);

    // We need to split at the first range that's before or contains the "at"
    // position, reading from the end to the start.
    let split_ranges_at = frags
        .frag_ixs
        .iter()
        .position(|&frag_id| {
            let frag = fragments[frag_id];
            frag.first >= at_pos || frag.contains(&at_pos)
        })
        .expect("split would create an empty child");

    let mut child_frag_ixs = smallvec_split_off(&mut frags.frag_ixs, split_ranges_at);

    // The split position is either in the middle of a lifetime hole, in which
    // case we don't need to do anything. Otherwise, we might need to split a
    // range fragment into two parts.
    if let Some(&frag_ix) = child_frag_ixs.first() {
        let frag = &fragments[frag_ix];
        if frag.first != at_pos && frag.contains(&at_pos) {
            // We're splitting in the middle of a fragment: [L, R].
            // Split it into two fragments: parent [L, pos[ + child [pos, R].
            debug_assert!(frag.first < frag.last, "trying to split unit fragment");
            debug_assert!(frag.first <= at_pos, "no space to split fragment");

            let parent_first = frag.first;
            let parent_last = prev_pos(at_pos);
            let child_first = at_pos;
            let child_last = frag.last;

            trace!(
                "split fragment [{:?}; {:?}] into two parts: [{:?}; {:?}] to [{:?}; {:?}]",
                frag.first,
                frag.last,
                parent_first,
                parent_last,
                child_first,
                child_last
            );

            debug_assert!(parent_first <= parent_last);
            debug_assert!(parent_last <= child_first);
            debug_assert!(child_first <= child_last);

            let bix = frag.bix;

            // Parent range.
            let count = 1; // unused by LSRA.
            let parent_frag =
                RangeFrag::new_multi_block(state.func, bix, parent_first, parent_last, count);

            let parent_frag_ix = RangeFragIx::new(state.fragments.len());
            state.fragments.push(parent_frag);

            // Child range.
            let child_frag =
                RangeFrag::new_multi_block(state.func, bix, child_first, child_last, count);
            let child_frag_ix = RangeFragIx::new(state.fragments.len());
            state.fragments.push(child_frag);

            // Note the sorted order is maintained, by construction.
            frags.frag_ixs.push(parent_frag_ix);
            child_frag_ixs[0] = child_frag_ix;
        }
    }

    if frags.frag_ixs.is_empty() {
        // The only possible way is that we're trying to split [(A;B),...] at A, so
        // creating a unit [A, A] fragment. Otherwise, it's a bug and this assert
        // should catch it.
        debug_assert!(
            split_ranges_at == 0 && parent_start == at_pos,
            "no fragments in the parent interval"
        );

        let frag = &state.fragments[child_frag_ixs[0]];
        let parent_frag =
            RangeFrag::new_multi_block(state.func, frag.bix, at_pos, at_pos, /* count */ 1);

        let parent_frag_ix = RangeFragIx::new(state.fragments.len());
        state.fragments.push(parent_frag);

        frags.frag_ixs.push(parent_frag_ix);
    }

    debug_assert!(!child_frag_ixs.is_empty(), "no fragments in child interval");

    let child_sorted_frags = SortedRangeFragIxs {
        frag_ixs: child_frag_ixs,
    };

    let child_int = VirtualRange {
        vreg,
        rreg: None,
        sorted_frags: child_sorted_frags,
        // These three fields are not used by linear scan.
        size: 0,
        total_cost: 0xFFFF_FFFFu32, // Our best approximation to "infinity".
        spill_cost: SpillCost::infinite(),
    };

    let child_start = state.fragments[child_int.sorted_frags.frag_ixs[0]].first;
    let child_end = state.fragments[*child_int.sorted_frags.frag_ixs.last().unwrap()].last;
    let parent_end = state.fragments[*frags.frag_ixs.last().unwrap()].last;

    // Insert child in virtual ranges and live intervals.
    let vreg_ix = VirtualRangeIx::new(state.intervals.virtual_ranges.len());
    state.intervals.virtual_ranges.push(child_int);

    // TODO make a better interface out of this.
    let child_id = IntId(state.intervals.num_intervals());
    let child_int = LiveInterval {
        id: child_id,
        kind: LiveIntervalKind::Virtual(vreg_ix),
        parent: Some(id),
        child: None,
        location: Location::None,
        reg_class: state.intervals.get(id).reg_class,
        start: child_start,
        end: child_end,
        last_frag: 0,
    };
    state.intervals.push_interval(child_int);

    state.intervals.data[id.0].end = parent_end;
    state.intervals.set_child(id, child_id);

    if log_enabled!(Level::Trace) {
        trace!("split results:");
        trace!("- {}", state.intervals.display(id, &state.fragments));
        trace!("- {}", state.intervals.display(child_id, &state.fragments));
    }

    child_id
}

fn build_mention_map(reg_uses: &RegUses) -> HashMap<Reg, MentionMap> {
    // Maps reg to its mentions.
    let mut reg_mentions: HashMap<Reg, MentionMap> = HashMap::default();

    // Collect all the mentions.
    for i in 0..reg_uses.num_insns() {
        let iix = InstIx::new(i as u32);
        let regsets = reg_uses.get_reg_sets_for_iix(iix);
        debug_assert!(regsets.is_sanitized());

        for reg in regsets.uses.iter() {
            let mentions = reg_mentions.entry(*reg).or_default();
            if mentions.is_empty() || mentions.last().unwrap().0 != iix {
                mentions.push((iix, Mention::new()));
            }
            mentions.last_mut().unwrap().1.add_use();
        }

        for reg in regsets.mods.iter() {
            let mentions = reg_mentions.entry(*reg).or_default();
            if mentions.is_empty() || mentions.last().unwrap().0 != iix {
                mentions.push((iix, Mention::new()));
            }
            mentions.last_mut().unwrap().1.add_mod();
        }

        for reg in regsets.defs.iter() {
            let mentions = reg_mentions.entry(*reg).or_default();
            if mentions.is_empty() || mentions.last().unwrap().0 != iix {
                mentions.push((iix, Mention::new()));
            }
            mentions.last_mut().unwrap().1.add_def();
        }
    }

    reg_mentions
}

fn cmp_interval_tree(
    left_id: (IntId, usize),
    right_id: (IntId, usize),
    intervals: &Intervals,
    fragments: &Fragments,
) -> Option<Ordering> {
    let left_frags = &intervals.fragments(left_id.0);
    let left = fragments[left_frags[left_id.1]].first;
    let right_frags = &intervals.fragments(right_id.0);
    let right = fragments[right_frags[right_id.1]].first;
    (left, left_id).partial_cmp(&(right, right_id))
}

fn smallvec_split_off<A: Array>(arr: &mut SmallVec<A>, at: usize) -> SmallVec<A>
where
    A::Item: Copy,
{
    let orig_size = arr.len();
    let mut res = SmallVec::<A>::new();
    for i in at..arr.len() {
        res.push(arr[i]);
    }
    arr.truncate(at);
    debug_assert!(arr.len() + res.len() == orig_size);
    res
}
