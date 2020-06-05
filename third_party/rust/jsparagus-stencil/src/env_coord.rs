//! (EnvironmentSlot,EnvironmentHops) pair represent the reference to a slot in
//! an environment object in the environment chain.
//!
//! See the SMDOC comment in m-c/js/src/vm/EnvironmentObject.h for the details
//! about environment object and environment chain.

/// Slot in the environmentslot object.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Ord, PartialOrd)]
pub struct EnvironmentSlot {
    slot: u32,
}

impl EnvironmentSlot {
    pub fn new(slot: u32) -> Self {
        Self { slot }
    }

    pub fn next(&mut self) {
        self.slot += 1;
    }
}

impl From<EnvironmentSlot> for u32 {
    fn from(slot: EnvironmentSlot) -> u32 {
        slot.slot
    }
}

/// The number of environment objects to skip (hop) on the environment chain in
/// order to find the associated environment object.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Ord, PartialOrd)]
pub struct EnvironmentHops {
    hops: u8,
}

impl EnvironmentHops {
    pub fn new(hops: u8) -> Self {
        Self { hops }
    }

    pub fn next(&mut self) {
        self.hops += 1;
    }
}

impl From<EnvironmentHops> for u8 {
    fn from(hops: EnvironmentHops) -> u8 {
        hops.hops
    }
}
