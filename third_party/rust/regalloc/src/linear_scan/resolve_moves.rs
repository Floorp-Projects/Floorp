use super::{next_use, Fragments, IntId, Intervals, Location, MentionMap, RegUses};
use crate::{
    data_structures::{BlockIx, InstPoint, Point},
    inst_stream::{InstToInsert, InstToInsertAndPoint},
    sparse_set::SparseSet,
    Function, RealReg, Reg, SpillSlot, TypedIxVec, VirtualReg, Writable,
};

use log::{debug, info, trace};
use rustc_hash::{FxHashMap as HashMap, FxHashSet as HashSet};
use std::env;
use std::fmt;

#[inline(never)]
pub(crate) fn run<F: Function>(
    func: &F,
    reg_uses: &RegUses,
    mention_map: &HashMap<Reg, MentionMap>,
    intervals: &Intervals,
    virtual_intervals: &Vec<IntId>,
    fragments: &Fragments,
    liveouts: &TypedIxVec<BlockIx, SparseSet<Reg>>,
    spill_slot: &mut u32,
    scratches_by_rc: &[Option<RealReg>],
) -> Vec<InstToInsertAndPoint> {
    let mut memory_moves = HashMap::default();

    let mut parallel_reloads = HashMap::default();
    let mut spills = HashMap::default();

    info!("resolve_moves");

    let mut block_ends = HashSet::default();
    let mut block_starts = HashSet::default();
    for bix in func.blocks() {
        let insts = func.block_insns(bix);
        block_ends.insert(insts.last());
        block_starts.insert(insts.first());
    }

    for &int_id in virtual_intervals {
        let (parent_end, parent_loc, loc) = {
            let interval = intervals.get(int_id);
            let loc = interval.location;

            let parent_id = match interval.parent {
                Some(pid) => pid,
                None => {
                    // In unreachable code, it's possible that a given interval has no
                    // parents and is assigned to a stack location for its whole lifetime.
                    //
                    // In reachable code, the analysis only create intervals for virtual
                    // registers with at least one register use, so a parentless interval (=
                    // hasn't ever been split) can't live in a stack slot.
                    debug_assert!(
                        loc.spill().is_none()
                            || (next_use(
                                mention_map,
                                intervals,
                                int_id,
                                InstPoint::min_value(),
                                reg_uses,
                                fragments
                            )
                            .is_none())
                    );
                    continue;
                }
            };

            let parent = intervals.get(parent_id);

            // If this is a move between blocks, handle it as such.
            if parent.end.pt == Point::Def
                && interval.start.pt == Point::Use
                && block_ends.contains(&parent.end.iix)
                && block_starts.contains(&interval.start.iix)
            {
                continue;
            }

            (parent.end, parent.location, loc)
        };

        let child_start = intervals.get(int_id).start;
        let vreg = intervals.vreg(int_id);

        match loc {
            Location::None => panic!("interval has no location after regalloc!"),

            Location::Reg(rreg) => {
                // Reconnect with the parent location, by adding a move if needed.
                match next_use(
                    mention_map,
                    intervals,
                    int_id,
                    child_start,
                    reg_uses,
                    fragments,
                ) {
                    Some(next_use) => {
                        // No need to reload before a new definition.
                        if next_use.pt == Point::Def {
                            continue;
                        }
                    }
                    None => {}
                };

                let mut at_inst = child_start;
                match at_inst.pt {
                    Point::Use => {
                        at_inst.pt = Point::Reload;
                    }
                    Point::Def => {
                        at_inst.pt = Point::Spill;
                    }
                    _ => unreachable!(),
                }
                let entry = parallel_reloads.entry(at_inst).or_insert(Vec::new());

                match parent_loc {
                    Location::None => unreachable!(),

                    Location::Reg(from_rreg) => {
                        if from_rreg != rreg {
                            debug!(
                                "inblock fixup: {:?} move {:?} -> {:?} at {:?}",
                                int_id, from_rreg, rreg, at_inst
                            );
                            entry.push(MoveOp::new_move(from_rreg, rreg, vreg));
                        }
                    }

                    Location::Stack(spill) => {
                        debug!(
                            "inblock fixup: {:?} reload {:?} -> {:?} at {:?}",
                            int_id, spill, rreg, at_inst
                        );
                        entry.push(MoveOp::new_reload(spill, rreg, vreg));
                    }
                }
            }

            Location::Stack(spill) => {
                // This interval has been spilled (i.e. split). Spill after the last def
                // or before the last use.
                let mut at_inst = parent_end;
                at_inst.pt = if at_inst.pt == Point::Use {
                    Point::Reload
                } else {
                    debug_assert!(at_inst.pt == Point::Def);
                    Point::Spill
                };

                match parent_loc {
                    Location::None => unreachable!(),

                    Location::Reg(rreg) => {
                        debug!(
                            "inblock fixup: {:?} spill {:?} -> {:?} at {:?}",
                            int_id, rreg, spill, at_inst
                        );
                        spills
                            .entry(at_inst)
                            .or_insert(Vec::new())
                            .push(InstToInsert::Spill {
                                to_slot: spill,
                                from_reg: rreg,
                                for_vreg: vreg,
                            });
                    }

                    Location::Stack(parent_spill) => {
                        debug_assert_eq!(parent_spill, spill);
                    }
                }
            }
        }
    }

    // Flush the memory moves caused by in-block fixups. Conceptually, the spills
    // must happen after the right locations have been set, that is, after the
    // reloads. Reloads may include several moves that must happen in parallel
    // (e.g. if two real regs must be swapped), so process them first. Once all
    // the parallel assignments have been done, push forward all the spills.
    for (at_inst, mut parallel_moves) in parallel_reloads {
        let ordered_moves = schedule_moves(&mut parallel_moves);
        let insts = emit_moves(ordered_moves, spill_slot, scratches_by_rc);
        memory_moves.insert(at_inst, insts);
    }
    for (at_inst, mut spills) in spills {
        memory_moves
            .entry(at_inst)
            .or_insert(Vec::new())
            .append(&mut spills);
    }

    let mut parallel_move_map = HashMap::default();
    enum BlockPos {
        Start,
        End,
    }

    // Figure the sequence of parallel moves to insert at block boundaries:
    // - for each block
    //  - for each liveout vreg in this block
    //    - for each successor of this block
    //      - if the locations allocated in the block and its successor don't
    //      match, insert a pending move from one location to the other.
    //
    // Once that's done:
    // - resolve cycles in the pending moves
    // - generate real moves from the pending moves.
    let mut seen_successors = HashSet::default();
    for block in func.blocks() {
        let successors = func.block_succs(block);
        seen_successors.clear();

        // Where to insert the fixup move, if needed? If there's more than one
        // successor to the current block, inserting in the current block will
        // impact all the successors.
        //
        // We assume critical edges have been split, so
        // if the current block has more than one successor, then its successors
        // have at most one predecessor.
        let cur_has_one_succ = successors.len() == 1;

        for succ in successors {
            if !seen_successors.insert(succ) {
                continue;
            }

            for &reg in liveouts[block].iter() {
                let vreg = if let Some(vreg) = reg.as_virtual_reg() {
                    vreg
                } else {
                    continue;
                };

                // Find the interval for this (vreg, inst) pair.
                let (succ_first_inst, succ_id) = {
                    let first_inst = InstPoint::new_use(func.block_insns(succ).first());
                    let found = match find_enclosing_interval(
                        vreg,
                        first_inst,
                        intervals,
                        &virtual_intervals,
                    ) {
                        Some(found) => found,
                        // The vreg is unused in this successor, no need to update its
                        // location.
                        None => continue,
                    };
                    (first_inst, found)
                };

                let (cur_last_inst, cur_id) = {
                    let last_inst = func.block_insns(block).last();
                    // see XXX above
                    let last_inst = InstPoint::new_def(last_inst);
                    let cur_id =
                        find_enclosing_interval(vreg, last_inst, intervals, &virtual_intervals)
                            .expect(&format!(
                                "no interval for given {:?}:{:?} pair in current {:?}",
                                vreg, last_inst, block
                            ));
                    (last_inst, cur_id)
                };

                if succ_id == cur_id {
                    continue;
                }

                let (at_inst, block_pos) = if cur_has_one_succ {
                    let mut pos = cur_last_inst;
                    // Before the control flow instruction.
                    pos.pt = Point::Reload;
                    (pos, BlockPos::End)
                } else {
                    let mut pos = succ_first_inst;
                    pos.pt = Point::Reload;
                    (pos, BlockPos::Start)
                };

                let pending_moves = parallel_move_map
                    .entry(at_inst)
                    .or_insert((Vec::new(), block_pos));

                match (
                    intervals.get(cur_id).location,
                    intervals.get(succ_id).location,
                ) {
                    (Location::Reg(cur_rreg), Location::Reg(succ_rreg)) => {
                        if cur_rreg == succ_rreg {
                            continue;
                        }
                        debug!(
              "boundary fixup: move {:?} -> {:?} at {:?} for {:?} between {:?} and {:?}",
              cur_rreg,
              succ_rreg,
              at_inst,
              vreg,
              block,
              succ
            );
                        pending_moves
                            .0
                            .push(MoveOp::new_move(cur_rreg, succ_rreg, vreg));
                    }

                    (Location::Reg(cur_rreg), Location::Stack(spillslot)) => {
                        debug!(
              "boundary fixup: spill {:?} -> {:?} at {:?} for {:?} between {:?} and {:?}",
              cur_rreg,
              spillslot,
              at_inst,
              vreg,
              block,
              succ
            );
                        pending_moves
                            .0
                            .push(MoveOp::new_spill(cur_rreg, spillslot, vreg));
                    }

                    (Location::Stack(spillslot), Location::Reg(rreg)) => {
                        debug!(
              "boundary fixup: reload {:?} -> {:?} at {:?} for {:?} between {:?} and {:?}",
              spillslot,
              rreg,
              at_inst,
              vreg,
              block,
              succ
            );
                        pending_moves
                            .0
                            .push(MoveOp::new_reload(spillslot, rreg, vreg));
                    }

                    (Location::Stack(left_spill_slot), Location::Stack(right_spill_slot)) => {
                        // Stack to stack should not happen here, since two ranges for the
                        // same vreg can't be intersecting, so the same stack slot ought to
                        // be reused in this case.
                        debug_assert_eq!(
              left_spill_slot, right_spill_slot,
              "Moves from stack to stack only happen on the same vreg, thus the same stack slot"
            );
                        continue;
                    }

                    (_, _) => {
                        panic!("register or stack slots must have been allocated.");
                    }
                };
            }
        }

        // Flush the memory moves caused by block fixups for this block.
        for (at_inst, parallel_moves) in parallel_move_map.iter_mut() {
            let ordered_moves = schedule_moves(&mut parallel_moves.0);
            let mut insts = emit_moves(ordered_moves, spill_slot, scratches_by_rc);

            // If at_inst pointed to a block start, then insert block fixups *before*
            // inblock fixups;
            // otherwise it pointed to a block end, then insert block fixups *after*
            // inblock fixups.
            let mut entry = memory_moves.entry(*at_inst).or_insert(Vec::new());
            match parallel_moves.1 {
                BlockPos::Start => {
                    insts.append(&mut entry);
                    *entry = insts;
                }
                BlockPos::End => {
                    entry.append(&mut insts);
                }
            }
        }
        parallel_move_map.clear();
    }
    debug!("");

    let mut insts_and_points = Vec::<InstToInsertAndPoint>::new();
    for (at, insts) in memory_moves {
        for inst in insts {
            insts_and_points.push(InstToInsertAndPoint::new(inst, at));
        }
    }

    insts_and_points
}

#[inline(never)]
fn find_enclosing_interval(
    vreg: VirtualReg,
    inst: InstPoint,
    intervals: &Intervals,
    virtual_intervals: &Vec<IntId>,
) -> Option<IntId> {
    // The list of virtual intervals is sorted by vreg; find one interval for this
    // vreg.
    let index = virtual_intervals
        .binary_search_by_key(&vreg, |&int_id| intervals.vreg(int_id))
        .expect("should find at least one virtual interval for this vreg");

    // Rewind back to the first interval for this vreg, since there might be
    // several ones.
    let mut index = index;
    while index > 0 && intervals.vreg(virtual_intervals[index - 1]) == vreg {
        index -= 1;
    }

    // Now iterates on all the intervals for this virtual register, until one
    // works.
    let mut int_id = virtual_intervals[index];
    loop {
        let int = intervals.get(int_id);
        if int.start <= inst && inst <= int.end {
            return Some(int_id);
        }
        // TODO reenable this if there are several fragments per interval again.
        //if intervals.covers(int_id, inst, fragments) {
        //return Some(int_id);
        //}

        index += 1;
        if index == virtual_intervals.len() {
            return None;
        }

        int_id = virtual_intervals[index];
        if intervals.vreg(int_id) != vreg {
            return None;
        }
    }
}

#[derive(PartialEq, Debug)]
enum MoveOperand {
    Reg(RealReg),
    Stack(SpillSlot),
}

impl MoveOperand {
    fn aliases(&self, other: &Self) -> bool {
        self == other
    }
}

struct MoveOp {
    from: MoveOperand,
    to: MoveOperand,
    vreg: VirtualReg,
    cycle_begin: Option<usize>,
    cycle_end: Option<usize>,
}

impl fmt::Debug for MoveOp {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "{:?}: {:?} -> {:?}", self.vreg, self.from, self.to)?;
        if let Some(ref begin) = self.cycle_begin {
            write!(fmt, ", start of cycle #{}", begin)?;
        }
        if let Some(ref end) = self.cycle_end {
            write!(fmt, ", end of cycle #{}", end)?;
        }
        Ok(())
    }
}

impl MoveOp {
    fn new_move(from: RealReg, to: RealReg, vreg: VirtualReg) -> Self {
        Self {
            from: MoveOperand::Reg(from),
            to: MoveOperand::Reg(to),
            vreg,
            cycle_begin: None,
            cycle_end: None,
        }
    }

    fn new_spill(from: RealReg, to: SpillSlot, vreg: VirtualReg) -> Self {
        Self {
            from: MoveOperand::Reg(from),
            to: MoveOperand::Stack(to),
            vreg,
            cycle_begin: None,
            cycle_end: None,
        }
    }

    fn new_reload(from: SpillSlot, to: RealReg, vreg: VirtualReg) -> Self {
        Self {
            from: MoveOperand::Stack(from),
            to: MoveOperand::Reg(to),
            vreg,
            cycle_begin: None,
            cycle_end: None,
        }
    }

    fn gen_inst(&self) -> InstToInsert {
        match self.from {
            MoveOperand::Reg(from) => match self.to {
                MoveOperand::Reg(to) => InstToInsert::Move {
                    to_reg: Writable::from_reg(to),
                    from_reg: from,
                    for_vreg: self.vreg,
                },
                MoveOperand::Stack(to) => InstToInsert::Spill {
                    to_slot: to,
                    from_reg: from,
                    for_vreg: self.vreg,
                },
            },
            MoveOperand::Stack(from) => match self.to {
                MoveOperand::Reg(to) => InstToInsert::Reload {
                    to_reg: Writable::from_reg(to),
                    from_slot: from,
                    for_vreg: self.vreg,
                },
                MoveOperand::Stack(_to) => unreachable!("stack to stack move"),
            },
        }
    }
}

fn find_blocking_move<'a>(
    pending: &'a mut Vec<MoveOp>,
    last: &MoveOp,
) -> Option<(usize, &'a mut MoveOp)> {
    for (i, other) in pending.iter_mut().enumerate() {
        if other.from.aliases(&last.to) {
            return Some((i, other));
        }
    }
    None
}

fn find_cycled_move<'a>(
    stack: &'a mut Vec<MoveOp>,
    from: &mut usize,
    last: &MoveOp,
) -> Option<&'a mut MoveOp> {
    for i in *from..stack.len() {
        *from += 1;
        let other = &stack[i];
        if other.from.aliases(&last.to) {
            return Some(&mut stack[i]);
        }
    }
    None
}

/// Given a pending list of moves, returns a list of moves ordered in a correct
/// way, i.e., no move clobbers another one.
#[inline(never)]
fn schedule_moves(pending: &mut Vec<MoveOp>) -> Vec<MoveOp> {
    let mut ordered_moves = Vec::new();

    let mut num_cycles = 0;
    let mut cur_cycles = 0;

    let show_debug = env::var("MOVES").is_ok();
    if show_debug {
        trace!("pending moves: {:#?}", pending);
    }

    while let Some(pm) = pending.pop() {
        if show_debug {
            trace!("handling pending move {:?}", pm);
        }
        debug_assert!(
            pm.from != pm.to,
            "spurious moves should not have been inserted"
        );

        let mut stack = vec![pm];

        while !stack.is_empty() {
            let blocking_pair = find_blocking_move(pending, stack.last().unwrap());

            if let Some((blocking_idx, blocking)) = blocking_pair {
                if show_debug {
                    trace!("found blocker: {:?}", blocking);
                }
                let mut stack_cur = 0;

                let has_cycles = if let Some(mut cycled) =
                    find_cycled_move(&mut stack, &mut stack_cur, blocking)
                {
                    if show_debug {
                        trace!("found cycle: {:?}", cycled);
                    }
                    debug_assert!(cycled.cycle_end.is_none());
                    cycled.cycle_end = Some(cur_cycles);
                    true
                } else {
                    false
                };

                if has_cycles {
                    loop {
                        match find_cycled_move(&mut stack, &mut stack_cur, blocking) {
                            Some(ref mut cycled) => {
                                if show_debug {
                                    trace!("found more cycles ending on blocker: {:?}", cycled);
                                }
                                debug_assert!(cycled.cycle_end.is_none());
                                cycled.cycle_end = Some(cur_cycles);
                            }
                            None => break,
                        }
                    }

                    debug_assert!(blocking.cycle_begin.is_none());
                    blocking.cycle_begin = Some(cur_cycles);
                    cur_cycles += 1;
                }

                let blocking = pending.remove(blocking_idx);
                stack.push(blocking);
            } else {
                // There's no blocking move! We can push this in the ordered list of
                // moves.
                // TODO IonMonkey has more optimizations for this case.
                let last = stack.pop().unwrap();
                ordered_moves.push(last);
            }
        }

        if num_cycles < cur_cycles {
            num_cycles = cur_cycles;
        }
        cur_cycles = 0;
    }

    ordered_moves
}

#[inline(never)]
fn emit_moves(
    ordered_moves: Vec<MoveOp>,
    num_spill_slots: &mut u32,
    scratches_by_rc: &[Option<RealReg>],
) -> Vec<InstToInsert> {
    let mut spill_slot = None;
    let mut in_cycle = false;

    let mut insts = Vec::new();

    let show_debug = env::var("MOVES").is_ok();
    if show_debug {
        trace!("emit_moves");
    }

    for mov in ordered_moves {
        if let Some(_) = &mov.cycle_end {
            debug_assert!(in_cycle);

            // There is some pattern:
            //   (A -> B)
            //   (B -> A)
            // This case handles (B -> A), which we reach last. We emit a move from
            // the saved value of B, to A.
            match mov.to {
                MoveOperand::Reg(dst_reg) => {
                    let inst = InstToInsert::Reload {
                        to_reg: Writable::from_reg(dst_reg),
                        from_slot: spill_slot.expect("should have a cycle spill slot"),
                        for_vreg: mov.vreg,
                    };
                    insts.push(inst);
                    if show_debug {
                        trace!(
                            "finishing cycle: {:?} -> {:?}",
                            spill_slot.unwrap(),
                            dst_reg
                        );
                    }
                }
                MoveOperand::Stack(dst_spill) => {
                    let scratch = scratches_by_rc[mov.vreg.get_class() as usize]
                        .expect("missing scratch reg");
                    let inst = InstToInsert::Reload {
                        to_reg: Writable::from_reg(scratch),
                        from_slot: spill_slot.expect("should have a cycle spill slot"),
                        for_vreg: mov.vreg,
                    };
                    insts.push(inst);
                    let inst = InstToInsert::Spill {
                        to_slot: dst_spill,
                        from_reg: scratch,
                        for_vreg: mov.vreg,
                    };
                    insts.push(inst);
                    if show_debug {
                        trace!(
                            "finishing cycle: {:?} -> {:?} -> {:?}",
                            spill_slot.unwrap(),
                            scratch,
                            dst_spill
                        );
                    }
                }
            };

            in_cycle = false;
            continue;
        }

        if let Some(_) = &mov.cycle_begin {
            debug_assert!(!in_cycle);

            // There is some pattern:
            //   (A -> B)
            //   (B -> A)
            // This case handles (A -> B), which we reach first. We save B, then allow
            // the original move to continue.
            match spill_slot {
                Some(_) => {}
                None => {
                    spill_slot = Some(SpillSlot::new(*num_spill_slots));
                    *num_spill_slots += 1;
                }
            }

            match mov.to {
                MoveOperand::Reg(src_reg) => {
                    let inst = InstToInsert::Spill {
                        to_slot: spill_slot.unwrap(),
                        from_reg: src_reg,
                        for_vreg: mov.vreg,
                    };
                    insts.push(inst);
                    if show_debug {
                        trace!("starting cycle: {:?} -> {:?}", src_reg, spill_slot.unwrap());
                    }
                }
                MoveOperand::Stack(src_spill) => {
                    let scratch = scratches_by_rc[mov.vreg.get_class() as usize]
                        .expect("missing scratch reg");
                    let inst = InstToInsert::Reload {
                        to_reg: Writable::from_reg(scratch),
                        from_slot: src_spill,
                        for_vreg: mov.vreg,
                    };
                    insts.push(inst);
                    let inst = InstToInsert::Spill {
                        to_slot: spill_slot.expect("should have a cycle spill slot"),
                        from_reg: scratch,
                        for_vreg: mov.vreg,
                    };
                    insts.push(inst);
                    if show_debug {
                        trace!(
                            "starting cycle: {:?} -> {:?} -> {:?}",
                            src_spill,
                            scratch,
                            spill_slot.unwrap()
                        );
                    }
                }
            };

            in_cycle = true;
        }

        // A normal move which is not part of a cycle.
        insts.push(mov.gen_inst());
        if show_debug {
            trace!("moving {:?} -> {:?}", mov.from, mov.to);
        }
    }

    insts
}
