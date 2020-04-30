//! Implementation of the linear scan allocator algorithm.
//!
//! This tries to follow the implementation as suggested by:
//!   Optimized Interval Splitting in a Linear Scan Register Allocator,
//!     by Wimmer et al., 2005

// TODO brain dump:
// - (perf) in try_allocate_reg, try to implement the fixed blocked heuristics, and see
// if it improves perf.
// - (perf) try to handle different register classes in different passes.
// - (correctness) use sanitized reg uses in lieu of reg uses.

use log::{debug, info, log_enabled, trace, Level};
use rustc_hash::FxHashMap as HashMap;
use smallvec::{Array, SmallVec};

use std::fmt;

use crate::analysis_data_flow::add_raw_reg_vecs_for_insn;
use crate::analysis_main::run_analysis;
use crate::data_structures::*;
use crate::inst_stream::{edit_inst_stream, InstToInsertAndPoint};
use crate::{Function, RegAllocError, RegAllocResult};

mod assign_registers;
mod resolve_moves;

// Helpers for SmallVec
fn smallvec_append<A: Array>(dst: &mut SmallVec<A>, src: &mut SmallVec<A>)
where
    A::Item: Copy,
{
    for e in src.iter() {
        dst.push(*e);
    }
    src.clear();
}

// Local shorthands.
type Fragments = TypedIxVec<RangeFragIx, RangeFrag>;
type VirtualRanges = TypedIxVec<VirtualRangeIx, VirtualRange>;
type RealRanges = TypedIxVec<RealRangeIx, RealRange>;
type RegUses = RegVecsAndBounds;

/// A unique identifier for an interval.
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) struct IntId(usize);

impl fmt::Debug for IntId {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "int{}", self.0)
    }
}

enum LiveIntervalKind {
    Fixed(RealRangeIx),
    Virtual(VirtualRangeIx),
}

impl fmt::Debug for LiveIntervalKind {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match self {
            LiveIntervalKind::Fixed(range) => write!(fmt, "fixed({:?})", range),
            LiveIntervalKind::Virtual(range) => write!(fmt, "virtual({:?})", range),
        }
    }
}

#[derive(Clone, PartialOrd, Ord, PartialEq, Eq)]
pub(crate) struct Mention(u8);

impl fmt::Debug for Mention {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        let mut comma = false;
        if self.0 & 1 == 1 {
            write!(fmt, "use")?;
            comma = true;
        }
        if (self.0 >> 1) & 1 == 1 {
            if comma {
                write!(fmt, ",")?;
            }
            write!(fmt, "mod")?;
            comma = true;
        }
        if (self.0 >> 2) & 1 == 1 {
            if comma {
                write!(fmt, ",")?;
            }
            write!(fmt, "def")?;
        }
        Ok(())
    }
}

impl Mention {
    // Setters.
    fn new() -> Self {
        Self(0)
    }
    fn add_def(&mut self) {
        self.0 |= 1 << 2;
    }
    fn add_mod(&mut self) {
        self.0 |= 1 << 1;
    }
    fn add_use(&mut self) {
        self.0 |= 1 << 0;
    }

    // Getters.
    fn is_use(&self) -> bool {
        (self.0 & 0b1) != 0
    }
    fn is_use_or_mod(&self) -> bool {
        (self.0 & 0b11) != 0
    }
    fn is_mod_or_def(&self) -> bool {
        (self.0 & 0b110) != 0
    }
}

pub(crate) type MentionMap = Vec<(InstIx, Mention)>;

#[derive(Debug, Clone, Copy)]
enum Location {
    None,
    Reg(RealReg),
    Stack(SpillSlot),
}

impl Location {
    fn reg(&self) -> Option<RealReg> {
        match self {
            Location::Reg(reg) => Some(*reg),
            _ => None,
        }
    }
    fn unwrap_reg(&self) -> RealReg {
        match self {
            Location::Reg(reg) => *reg,
            _ => panic!("unwrap_reg called on non-reg location"),
        }
    }
    fn spill(&self) -> Option<SpillSlot> {
        match self {
            Location::Stack(slot) => Some(*slot),
            _ => None,
        }
    }
    fn is_none(&self) -> bool {
        match self {
            Location::None => true,
            _ => false,
        }
    }
}

impl fmt::Display for Location {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Location::None => write!(fmt, "none"),
            Location::Reg(reg) => write!(fmt, "{:?}", reg),
            Location::Stack(slot) => write!(fmt, "{:?}", slot),
        }
    }
}

struct LiveInterval {
    /// A unique identifier in the live interval graph.
    id: IntId,
    /// Is it fixed or virtual?
    kind: LiveIntervalKind,
    /// Parent interval in the split tree.
    parent: Option<IntId>,
    child: Option<IntId>,
    /// Location assigned to this live interval.
    location: Location,

    // Cached fields
    reg_class: RegClass,
    start: InstPoint,
    end: InstPoint,
    last_frag: usize,
}

impl LiveInterval {
    fn is_fixed(&self) -> bool {
        match &self.kind {
            LiveIntervalKind::Fixed(_) => true,
            LiveIntervalKind::Virtual(_) => false,
        }
    }
    fn unwrap_virtual(&self) -> VirtualRangeIx {
        if let LiveIntervalKind::Virtual(r) = &self.kind {
            *r
        } else {
            unreachable!();
        }
    }
}

/// A group of live intervals.
pub(crate) struct Intervals {
    real_ranges: RealRanges,
    virtual_ranges: VirtualRanges,
    data: Vec<LiveInterval>,
}

impl Intervals {
    fn new(real_ranges: RealRanges, virtual_ranges: VirtualRanges, fragments: &Fragments) -> Self {
        let mut data =
            Vec::with_capacity(real_ranges.len() as usize + virtual_ranges.len() as usize);

        for rlr in 0..real_ranges.len() {
            data.push(LiveIntervalKind::Fixed(RealRangeIx::new(rlr)));
        }
        for vlr in 0..virtual_ranges.len() {
            data.push(LiveIntervalKind::Virtual(VirtualRangeIx::new(vlr)));
        }

        let data = data
            .into_iter()
            .enumerate()
            .map(|(index, kind)| {
                let (location, start, end, reg_class) = match kind {
                    LiveIntervalKind::Fixed(ix) => {
                        let range = &real_ranges[ix];
                        let start = fragments[range.sorted_frags.frag_ixs[0]].first;
                        let end = fragments[*range.sorted_frags.frag_ixs.last().unwrap()].last;
                        let reg_class = range.rreg.get_class();
                        let location = Location::Reg(range.rreg);
                        (location, start, end, reg_class)
                    }
                    LiveIntervalKind::Virtual(ix) => {
                        let range = &virtual_ranges[ix];
                        let start = fragments[range.sorted_frags.frag_ixs[0]].first;
                        let end = fragments[*range.sorted_frags.frag_ixs.last().unwrap()].last;
                        let reg_class = range.vreg.get_class();
                        let location = Location::None;
                        (location, start, end, reg_class)
                    }
                };

                LiveInterval {
                    id: IntId(index),
                    kind,
                    parent: None,
                    child: None,
                    location,
                    reg_class,
                    start,
                    end,
                    last_frag: 0,
                }
            })
            .collect();

        Self {
            real_ranges,
            virtual_ranges,
            data,
        }
    }

    fn get(&self, int_id: IntId) -> &LiveInterval {
        &self.data[int_id.0]
    }
    fn get_mut(&mut self, int_id: IntId) -> &mut LiveInterval {
        &mut self.data[int_id.0]
    }

    fn fragments(&self, int_id: IntId) -> &SmallVec<[RangeFragIx; 4]> {
        match &self.data[int_id.0].kind {
            LiveIntervalKind::Fixed(r) => &self.real_ranges[*r].sorted_frags.frag_ixs,
            LiveIntervalKind::Virtual(r) => &self.virtual_ranges[*r].sorted_frags.frag_ixs,
        }
    }
    fn fragments_mut(&mut self, int_id: IntId) -> &mut SortedRangeFragIxs {
        match &mut self.data[int_id.0].kind {
            LiveIntervalKind::Fixed(r) => &mut self.real_ranges[*r].sorted_frags,
            LiveIntervalKind::Virtual(r) => &mut self.virtual_ranges[*r].sorted_frags,
        }
    }

    fn vreg(&self, int_id: IntId) -> VirtualReg {
        match &self.data[int_id.0].kind {
            LiveIntervalKind::Fixed(_) => panic!("asking for vreg of fixed interval"),
            LiveIntervalKind::Virtual(r) => self.virtual_ranges[*r].vreg,
        }
    }

    fn reg(&self, int_id: IntId) -> Reg {
        match &self.data[int_id.0].kind {
            LiveIntervalKind::Fixed(r) => self.real_ranges[*r].rreg.to_reg(),
            LiveIntervalKind::Virtual(r) => self.virtual_ranges[*r].vreg.to_reg(),
        }
    }

    #[inline(never)]
    fn covers(&self, int_id: IntId, pos: InstPoint, fragments: &Fragments) -> bool {
        // Fragments are sorted by start.
        let frag_ixs = self.fragments(int_id);

        // The binary search is useful only after some threshold number of elements;
        // This value has been determined after benchmarking a large program.
        if frag_ixs.len() <= 4 {
            for &frag_ix in frag_ixs {
                let frag = &fragments[frag_ix];
                if frag.first <= pos && pos <= frag.last {
                    return true;
                }
            }
            return false;
        }

        match frag_ixs.binary_search_by_key(&pos, |&index| fragments[index].first) {
            // Either we find a precise match...
            Ok(_) => true,
            // ... or we're just after an interval that could contain it.
            Err(index) => {
                // There's at least one fragment, by construction, so no need to check
                // against fragments.len().
                index > 0 && pos <= fragments[frag_ixs[index - 1]].last
            }
        }
    }

    #[inline(never)]
    fn intersects_with(
        &self,
        left_id: IntId,
        right_id: IntId,
        fragments: &Fragments,
    ) -> Option<InstPoint> {
        let left = self.get(left_id);
        let right = self.get(right_id);

        if left.start == right.start {
            return Some(left.start);
        }

        let left_frags = &self.fragments(left_id);
        let right_frags = &self.fragments(right_id);

        let mut left_i = left.last_frag;
        let mut right_i = right.last_frag;
        let mut left_max_i = left_frags.len() - 1;
        let mut right_max_i = right_frags.len() - 1;

        if left.end < right.end {
            right_max_i = match right_frags
                .binary_search_by_key(&left.end, |&frag_ix| fragments[frag_ix].first)
            {
                Ok(index) => index,
                Err(index) => {
                    if index == 0 {
                        index
                    } else {
                        index - 1
                    }
                }
            };
        } else {
            left_max_i = match left_frags
                .binary_search_by_key(&right.end, |&frag_ix| fragments[frag_ix].first)
            {
                Ok(index) => index,
                Err(index) => {
                    if index == 0 {
                        index
                    } else {
                        index - 1
                    }
                }
            };
        }

        let mut left_frag = &fragments[left_frags[left_i]];
        let mut right_frag = &fragments[right_frags[right_i]];
        loop {
            if left_frag.first == right_frag.first {
                return Some(left_frag.first);
            }
            if left_frag.last < right_frag.first {
                // left_frag < right_frag, go to the range following left_frag.
                left_i += 1;
                if left_i > left_max_i {
                    break;
                }
                left_frag = &fragments[left_frags[left_i]];
            } else if right_frag.last < left_frag.first {
                // left_frag > right_frag, go to the range following right_frag.
                right_i += 1;
                if right_i > right_max_i {
                    break;
                }
                right_frag = &fragments[right_frags[right_i]];
            } else {
                // They intersect!
                return Some(if left_frag.first < right_frag.first {
                    right_frag.first
                } else {
                    left_frag.first
                });
            }
        }

        None
    }

    fn num_intervals(&self) -> usize {
        self.data.len()
    }

    fn display(&self, int_id: IntId, fragments: &Fragments) -> String {
        let int = &self.data[int_id.0];
        let vreg = if int.is_fixed() {
            "fixed".to_string()
        } else {
            format!("{:?}", self.vreg(int_id))
        };
        let frag_ixs = &self.fragments(int_id);
        let fragments = frag_ixs
            .iter()
            .map(|&ix| {
                let frag = fragments[ix];
                (ix, frag.first, frag.last)
            })
            .collect::<Vec<_>>();
        format!(
            "{:?}{}: {} {} {:?}",
            int.id,
            if let Some(ref p) = int.parent {
                format!(" (parent={:?}) ", p)
            } else {
                "".to_string()
            },
            vreg,
            int.location,
            fragments
        )
    }

    // Mutators.
    fn set_reg(&mut self, int_id: IntId, reg: RealReg) {
        let int = self.get_mut(int_id);
        debug_assert!(int.location.is_none());
        debug_assert!(!int.is_fixed());
        int.location = Location::Reg(reg);
    }
    fn set_spill(&mut self, int_id: IntId, slot: SpillSlot) {
        let int = self.get_mut(int_id);
        debug_assert!(int.location.spill().is_none());
        debug_assert!(!int.is_fixed());
        int.location = Location::Stack(slot);
    }
    fn push_interval(&mut self, int: LiveInterval) {
        debug_assert!(int.id.0 == self.data.len());
        self.data.push(int);
    }
    fn set_child(&mut self, int_id: IntId, child_id: IntId) {
        if let Some(prev_child) = self.data[int_id.0].child.clone() {
            self.data[child_id.0].child = Some(prev_child);
            self.data[prev_child.0].parent = Some(child_id);
        }
        self.data[int_id.0].child = Some(child_id);
    }
}

/// Finds the first use for the current interval that's located after the given
/// `pos` (included), in a broad sense of use (any of use, def or mod).
///
/// Extends to the left, that is, "modified" means "used".
#[inline(never)]
fn next_use(
    mentions: &HashMap<Reg, MentionMap>,
    intervals: &Intervals,
    id: IntId,
    pos: InstPoint,
    _reg_uses: &RegUses,
    fragments: &Fragments,
) -> Option<InstPoint> {
    if log_enabled!(Level::Trace) {
        trace!(
            "find next use of {} after {:?}",
            intervals.display(id, fragments),
            pos
        );
    }

    let mentions = &mentions[&intervals.reg(id)];

    let target = InstPoint::max(pos, intervals.get(id).start);

    let ret = match mentions.binary_search_by_key(&target.iix, |mention| mention.0) {
        Ok(index) => {
            // Either the selected index is a perfect match, or the next mention is
            // the correct answer.
            let mention = &mentions[index];
            if target.pt == Point::Use {
                if mention.1.is_use_or_mod() {
                    Some(InstPoint::new_use(mention.0))
                } else {
                    Some(InstPoint::new_def(mention.0))
                }
            } else if target.pt == Point::Def && mention.1.is_mod_or_def() {
                Some(target)
            } else if index == mentions.len() - 1 {
                None
            } else {
                let mention = &mentions[index + 1];
                if mention.1.is_use_or_mod() {
                    Some(InstPoint::new_use(mention.0))
                } else {
                    Some(InstPoint::new_def(mention.0))
                }
            }
        }

        Err(index) => {
            if index == mentions.len() {
                None
            } else {
                let mention = &mentions[index];
                if mention.1.is_use_or_mod() {
                    Some(InstPoint::new_use(mention.0))
                } else {
                    Some(InstPoint::new_def(mention.0))
                }
            }
        }
    };

    // TODO once the mentions are properly split, this could be removed, in
    // theory.
    let ret = match ret {
        Some(pos) => {
            if pos <= intervals.get(id).end {
                Some(pos)
            } else {
                None
            }
        }
        None => None,
    };

    #[cfg(debug_assertions)]
    debug_assert_eq!(ref_next_use(intervals, id, pos, _reg_uses, fragments), ret);

    ret
}

#[cfg(debug_assertions)]
fn ref_next_use(
    intervals: &Intervals,
    id: IntId,
    pos: InstPoint,
    reg_uses: &RegUses,
    fragments: &Fragments,
) -> Option<InstPoint> {
    let int = intervals.get(id);
    if int.end < pos {
        return None;
    }

    let reg = if int.is_fixed() {
        int.location.reg().unwrap().to_reg()
    } else {
        intervals.vreg(id).to_reg()
    };

    for &frag_id in intervals.fragments(id) {
        let frag = &fragments[frag_id];
        if frag.last < pos {
            continue;
        }
        for inst_id in frag.first.iix.dotdot(frag.last.iix.plus_n(1)) {
            if inst_id < pos.iix {
                continue;
            }

            let regsets = &reg_uses.get_reg_sets_for_iix(inst_id);
            debug_assert!(regsets.is_sanitized());

            let at_use = InstPoint::new_use(inst_id);
            if pos <= at_use && frag.contains(&at_use) {
                if regsets.uses.contains(reg) || regsets.mods.contains(reg) {
                    #[cfg(debug_assertions)]
                    debug_assert!(intervals.covers(id, at_use, fragments));
                    trace!(
                        "ref next_use: found next use of {:?} after {:?} at {:?}",
                        id,
                        pos,
                        at_use
                    );
                    return Some(at_use);
                }
            }

            let at_def = InstPoint::new_def(inst_id);
            if pos <= at_def && frag.contains(&at_def) {
                if regsets.defs.contains(reg) || regsets.mods.contains(reg) {
                    #[cfg(debug_assertions)]
                    debug_assert!(intervals.covers(id, at_def, fragments));
                    trace!(
                        "ref next_use: found next use of {:?} after {:?} at {:?}",
                        id,
                        pos,
                        at_def
                    );
                    return Some(at_def);
                }
            }
        }
    }

    trace!("ref next_use: no next use");
    None
}

/// Finds the last use of a vreg before a given target, including it in possible
/// return values.
/// Extends to the right, that is, modified means "def".
fn last_use(
    mention_map: &HashMap<Reg, MentionMap>,
    intervals: &Intervals,
    id: IntId,
    pos: InstPoint,
    _reg_uses: &RegUses,
    fragments: &Fragments,
) -> Option<InstPoint> {
    if log_enabled!(Level::Trace) {
        trace!(
            "searching last use of {} before {:?}",
            intervals.display(id, fragments),
            pos,
        );
    }

    let mentions = &mention_map[&intervals.reg(id)];

    let target = InstPoint::min(pos, intervals.get(id).end);

    let ret = match mentions.binary_search_by_key(&target.iix, |mention| mention.0) {
        Ok(index) => {
            // Either the selected index is a perfect match, or the previous mention
            // is the correct answer.
            let mention = &mentions[index];
            if target.pt == Point::Def {
                if mention.1.is_mod_or_def() {
                    Some(InstPoint::new_def(mention.0))
                } else {
                    Some(InstPoint::new_use(mention.0))
                }
            } else if target.pt == Point::Use && mention.1.is_use() {
                Some(target)
            } else if index == 0 {
                None
            } else {
                let mention = &mentions[index - 1];
                if mention.1.is_mod_or_def() {
                    Some(InstPoint::new_def(mention.0))
                } else {
                    Some(InstPoint::new_use(mention.0))
                }
            }
        }

        Err(index) => {
            if index == 0 {
                None
            } else {
                let mention = &mentions[index - 1];
                if mention.1.is_mod_or_def() {
                    Some(InstPoint::new_def(mention.0))
                } else {
                    Some(InstPoint::new_use(mention.0))
                }
            }
        }
    };

    // TODO once the mentions are properly split, this could be removed, in
    // theory.
    let ret = match ret {
        Some(pos) => {
            if pos >= intervals.get(id).start {
                Some(pos)
            } else {
                None
            }
        }
        None => None,
    };

    trace!("mentions: {:?}", mentions);
    trace!("new algo: {:?}", ret);

    #[cfg(debug_assertions)]
    debug_assert_eq!(ref_last_use(intervals, id, pos, _reg_uses, fragments), ret);

    ret
}

#[allow(dead_code)]
#[inline(never)]
fn ref_last_use(
    intervals: &Intervals,
    id: IntId,
    pos: InstPoint,
    reg_uses: &RegUses,
    fragments: &Fragments,
) -> Option<InstPoint> {
    let int = intervals.get(id);
    debug_assert!(int.start <= pos);

    let reg = intervals.vreg(id).to_reg();

    for &i in intervals.fragments(id).iter().rev() {
        let frag = fragments[i];
        if frag.first > pos {
            continue;
        }

        let mut inst = frag.last.iix;
        while inst >= frag.first.iix {
            let regsets = &reg_uses.get_reg_sets_for_iix(inst);
            debug_assert!(regsets.is_sanitized());

            let at_def = InstPoint::new_def(inst);
            if at_def <= pos && at_def <= frag.last {
                if regsets.defs.contains(reg) || regsets.mods.contains(reg) {
                    #[cfg(debug_assertions)]
                    debug_assert!(
                        intervals.covers(id, at_def, fragments),
                        "last use must be in interval"
                    );
                    trace!(
                        "last use of {:?} before {:?} found at {:?}",
                        id,
                        pos,
                        at_def,
                    );
                    return Some(at_def);
                }
            }

            let at_use = InstPoint::new_use(inst);
            if at_use <= pos && at_use <= frag.last {
                if regsets.uses.contains(reg) || regsets.mods.contains(reg) {
                    #[cfg(debug_assertions)]
                    debug_assert!(
                        intervals.covers(id, at_use, fragments),
                        "last use must be in interval"
                    );
                    trace!(
                        "last use of {:?} before {:?} found at {:?}",
                        id,
                        pos,
                        at_use,
                    );
                    return Some(at_use);
                }
            }

            if inst.get() == 0 {
                break;
            }
            inst = inst.minus(1);
        }
    }

    None
}

fn try_compress_ranges<F: Function>(
    func: &F,
    rlrs: &mut RealRanges,
    vlrs: &mut VirtualRanges,
    fragments: &mut Fragments,
) {
    fn compress<F: Function>(
        func: &F,
        frag_ixs: &mut SmallVec<[RangeFragIx; 4]>,
        fragments: &mut Fragments,
    ) {
        if frag_ixs.len() == 1 {
            return;
        }

        let last_frag_end = fragments[*frag_ixs.last().unwrap()].last;
        let first_frag = &mut fragments[frag_ixs[0]];

        let new_range =
            RangeFrag::new_multi_block(func, first_frag.bix, first_frag.first, last_frag_end, 1);

        let new_range_ix = RangeFragIx::new(fragments.len());
        fragments.push(new_range);
        frag_ixs.clear();
        frag_ixs.push(new_range_ix);

        //let old_size = frag_ixs.len();
        //let mut i = frag_ixs.len() - 1;
        //while i > 0 {
        //let cur_frag = &fragments[frag_ixs[i]];
        //let prev_frag = &fragments[frag_ixs[i - 1]];
        //if prev_frag.last.iix.get() + 1 == cur_frag.first.iix.get()
        //&& prev_frag.last.pt == Point::Def
        //&& cur_frag.first.pt == Point::Use
        //{
        //let new_range = RangeFrag::new_multi_block(
        //func,
        //prev_frag.bix,
        //prev_frag.first,
        //cur_frag.last,
        //prev_frag.count + cur_frag.count,
        //);

        //let new_range_ix = RangeFragIx::new(fragments.len());
        //fragments.push(new_range);
        //frag_ixs[i - 1] = new_range_ix;

        //let _ = frag_ixs.remove(i);
        //}
        //i -= 1;
        //}

        //let new_size = frag_ixs.len();
        //info!(
        //"compress: {} -> {}; {}",
        //old_size,
        //new_size,
        //100. * (old_size as f64 - new_size as f64) / (old_size as f64)
        //);
    }

    let mut by_vreg: HashMap<VirtualReg, VirtualRange> = HashMap::default();

    for vlr in vlrs.iter_mut() {
        if let Some(vrange) = by_vreg.get(&vlr.vreg) {
            let vlr_start = fragments[vlr.sorted_frags.frag_ixs[0]].first;
            let vlr_last = fragments[*vlr.sorted_frags.frag_ixs.last().unwrap()].last;
            let common_frags = &vrange.sorted_frags.frag_ixs;
            if vlr_start < fragments[common_frags[0]].first {
                fragments[vrange.sorted_frags.frag_ixs[0]].first = vlr_start;
            }
            if vlr_last > fragments[*common_frags.last().unwrap()].last {
                fragments[*common_frags.last().unwrap()].last = vlr_last;
            }
        } else {
            // First time we see this vreg, compress and insert it.
            compress(func, &mut vlr.sorted_frags.frag_ixs, fragments);
            // TODO try to avoid the clone?
            by_vreg.insert(vlr.vreg, vlr.clone());
        }
    }

    vlrs.clear();
    for (_, vlr) in by_vreg {
        vlrs.push(vlr);
    }

    let mut reg_map: HashMap<RealReg, SmallVec<[RangeFragIx; 4]>> = HashMap::default();
    for rlr in rlrs.iter_mut() {
        let reg = rlr.rreg;
        if let Some(ref mut vec) = reg_map.get_mut(&reg) {
            smallvec_append(vec, &mut rlr.sorted_frags.frag_ixs);
        } else {
            // TODO clone can be avoided with an into_iter methods.
            reg_map.insert(reg, rlr.sorted_frags.frag_ixs.clone());
        }
    }

    rlrs.clear();
    for (rreg, mut sorted_frags) in reg_map {
        sorted_frags.sort_by_key(|frag_ix| fragments[*frag_ix].first);

        //compress(func, &mut sorted_frags, fragments);

        rlrs.push(RealRange {
            rreg,
            sorted_frags: SortedRangeFragIxs {
                frag_ixs: sorted_frags,
            },
        });
    }
}

// Allocator top level.  `func` is modified so that, when this function
// returns, it will contain no VirtualReg uses.  Allocation can fail if there
// are insufficient registers to even generate spill/reload code, or if the
// function appears to have any undefined VirtualReg/RealReg uses.
#[inline(never)]
pub(crate) fn run<F: Function>(
    func: &mut F,
    reg_universe: &RealRegUniverse,
    use_checker: bool,
) -> Result<RegAllocResult<F>, RegAllocError> {
    let (reg_uses, mut rlrs, mut vlrs, mut fragments, liveouts, _est_freqs, _inst_to_block_map) =
        run_analysis(func, reg_universe).map_err(|err| RegAllocError::Analysis(err))?;

    let scratches_by_rc = {
        let mut scratches_by_rc = vec![None; NUM_REG_CLASSES];
        for i in 0..NUM_REG_CLASSES {
            if let Some(info) = &reg_universe.allocable_by_class[i] {
                if info.first == info.last {
                    return Err(RegAllocError::Other(
                        "at least 2 registers required for linear scan".into(),
                    ));
                }
                let scratch = if let Some(suggested_reg) = info.suggested_scratch {
                    reg_universe.regs[suggested_reg].0
                } else {
                    return Err(RegAllocError::MissingSuggestedScratchReg(
                        RegClass::rc_from_u32(i as u32),
                    ));
                };
                scratches_by_rc[i] = Some(scratch);
            }
        }
        scratches_by_rc
    };

    try_compress_ranges(func, &mut rlrs, &mut vlrs, &mut fragments);

    let intervals = Intervals::new(rlrs, vlrs, &fragments);

    if log_enabled!(Level::Trace) {
        trace!("unassigned intervals:");
        for int in &intervals.data {
            trace!("{}", intervals.display(int.id, &fragments));
        }
        trace!("");
    }

    let (mention_map, fragments, intervals, mut num_spill_slots) = assign_registers::run(
        func,
        &reg_uses,
        reg_universe,
        &scratches_by_rc,
        intervals,
        fragments,
    )?;

    // Filter fixed intervals, they're already in the right place.
    let mut virtual_intervals = intervals
        .data
        .iter()
        .filter_map(|int| {
            if let LiveIntervalKind::Fixed(_) = &int.kind {
                None
            } else {
                Some(int.id)
            }
        })
        .collect::<Vec<_>>();

    // Sort by vreg and starting point, so we can plug all the different intervals
    // together.
    virtual_intervals.sort_by_key(|&int_id| {
        let int = intervals.get(int_id);
        let vreg = &intervals.virtual_ranges[int.unwrap_virtual()].vreg;
        (vreg, int.start)
    });

    if log_enabled!(Level::Debug) {
        debug!("allocation results (by vreg)");
        for &int_id in &virtual_intervals {
            debug!("{}", intervals.display(int_id, &fragments));
        }
        debug!("");
    }

    let memory_moves = resolve_moves::run(
        func,
        &reg_uses,
        &mention_map,
        &intervals,
        &virtual_intervals,
        &fragments,
        &liveouts,
        &mut num_spill_slots,
        &scratches_by_rc,
    );

    apply_registers(
        func,
        &intervals,
        virtual_intervals,
        &fragments,
        memory_moves,
        reg_universe,
        num_spill_slots,
        use_checker,
    )
}

/// Fills in the register assignments into instructions.
#[inline(never)]
fn apply_registers<F: Function>(
    func: &mut F,
    intervals: &Intervals,
    virtual_intervals: Vec<IntId>,
    fragments: &Fragments,
    memory_moves: Vec<InstToInsertAndPoint>,
    reg_universe: &RealRegUniverse,
    num_spill_slots: u32,
    use_checker: bool,
) -> Result<RegAllocResult<F>, RegAllocError> {
    info!("apply_registers");

    let mut frag_map = Vec::<(RangeFragIx, VirtualReg, RealReg)>::new();
    for int_id in virtual_intervals {
        if let Some(rreg) = intervals.get(int_id).location.reg() {
            let vreg = intervals.vreg(int_id);
            for &range_ix in intervals.fragments(int_id) {
                let range = &fragments[range_ix];
                trace!("in {:?}, {:?} lives in {:?}", range, vreg, rreg);
                frag_map.push((range_ix, vreg, rreg));
            }
        }
    }

    trace!("frag_map: {:?}", frag_map);

    let (final_insns, target_map, orig_insn_map) = edit_inst_stream(
        func,
        memory_moves,
        &vec![],
        frag_map,
        fragments,
        reg_universe,
        use_checker,
    )?;

    // Compute clobbered registers with one final, quick pass.
    //
    // FIXME: derive this information directly from the allocation data
    // structures used above.
    //
    // NB at this point, the `san_reg_uses` that was computed in the analysis
    // phase is no longer valid, because we've added and removed instructions to
    // the function relative to the one that `san_reg_uses` was computed from,
    // so we have to re-visit all insns with `add_raw_reg_vecs_for_insn`.
    // That's inefficient, but we don't care .. this should only be a temporary
    // fix.

    let mut clobbered_registers: Set<RealReg> = Set::empty();

    // We'll dump all the reg uses in here.  We don't care the bounds, so just
    // pass a dummy one in the loop.
    let mut reg_vecs = RegVecs::new(/*sanitized=*/ false);
    let mut dummy_bounds = RegVecBounds::new();
    for insn in &final_insns {
        add_raw_reg_vecs_for_insn::<F>(insn, &mut reg_vecs, &mut dummy_bounds);
    }
    for reg in reg_vecs.defs.iter().chain(reg_vecs.mods.iter()) {
        debug_assert!(reg.is_real());
        clobbered_registers.insert(reg.to_real_reg());
    }

    // And now remove from the set, all those not available to the allocator.
    // But not removing the reserved regs, since we might have modified those.
    clobbered_registers.filter_map(|&reg| {
        if reg.get_index() >= reg_universe.allocable {
            None
        } else {
            Some(reg)
        }
    });

    Ok(RegAllocResult {
        insns: final_insns,
        target_map,
        orig_insn_map,
        clobbered_registers,
        num_spill_slots,
        block_annotations: None,
    })
}
