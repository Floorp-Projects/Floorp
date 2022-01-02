/// For tracking bytecode offsets in jumps
#[derive(Clone, Copy, PartialEq, Debug)]
#[must_use]
pub struct BytecodeOffset {
    pub offset: usize,
}

impl BytecodeOffset {
    fn new(offset: usize) -> Self {
        Self { offset }
    }

    /// diff_from is useful for finding the offset between two bytecodes. This is usually
    /// used for jumps and jump_targets.
    ///
    /// For a forward jump, self will be a larger number, and start will be smaller
    /// the output will be a positive number.  for a backward jump, the reverse
    /// will be true, and the number will be negative. So it is important to use this api
    /// consistently in both cases.
    ///
    /// Examples:
    /// let offset_diff: BytecodeOffsetDiff = forward_jump_target_offset.diff_from(jump)
    /// let offset_diff: BytecodeOffsetDiff = backward_jump_target_offset.diff_from(jump)
    pub fn diff_from(self, start: BytecodeOffset) -> BytecodeOffsetDiff {
        BytecodeOffsetDiff::new(self, start)
    }
}

impl From<BytecodeOffset> for usize {
    fn from(offset: BytecodeOffset) -> usize {
        offset.offset
    }
}

impl From<usize> for BytecodeOffset {
    fn from(offset: usize) -> BytecodeOffset {
        BytecodeOffset::new(offset)
    }
}

pub struct BytecodeOffsetDiff {
    diff: i32,
}

impl BytecodeOffsetDiff {
    fn new(end: BytecodeOffset, start: BytecodeOffset) -> Self {
        let diff = (end.offset as i128 - start.offset as i128) as i32;
        Self { diff }
    }

    pub fn uninitialized() -> Self {
        Self { diff: 0i32 }
    }
}

impl From<BytecodeOffsetDiff> for i32 {
    fn from(offset: BytecodeOffsetDiff) -> i32 {
        offset.diff
    }
}
