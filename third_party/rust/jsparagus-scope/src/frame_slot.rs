/// Slot in the stack frame.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Ord, PartialOrd)]
pub struct FrameSlot {
    slot: u32,
}

impl FrameSlot {
    pub fn new(slot: u32) -> Self {
        Self { slot }
    }

    pub fn next(&mut self) {
        self.slot += 1;
    }
}

impl From<FrameSlot> for u32 {
    fn from(slot: FrameSlot) -> u32 {
        slot.slot
    }
}
