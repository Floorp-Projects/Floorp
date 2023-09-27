//! `ioctl` opcode behavior for Linux platforms.

use super::{Direction, RawOpcode};
use consts::*;

/// Compose an opcode from its component parts.
pub(super) const fn compose_opcode(
    dir: Direction,
    group: RawOpcode,
    num: RawOpcode,
    size: RawOpcode,
) -> RawOpcode {
    macro_rules! shift_and_mask {
        ($val:expr, $shift:expr, $mask:expr) => {{
            ($val << $shift) & $mask
        }};
    }

    let dir = match dir {
        Direction::None => NONE,
        Direction::Read => READ,
        Direction::Write => WRITE,
        Direction::ReadWrite => READ | WRITE,
    };

    shift_and_mask!(group, GROUP_SHIFT, GROUP_MASK)
        | shift_and_mask!(num, NUM_SHIFT, NUM_MASK)
        | shift_and_mask!(size, SIZE_SHIFT, SIZE_MASK)
        | shift_and_mask!(dir, DIR_SHIFT, DIR_MASK)
}

const NUM_BITS: RawOpcode = 8;
const GROUP_BITS: RawOpcode = 8;

const NUM_SHIFT: RawOpcode = 0;
const GROUP_SHIFT: RawOpcode = NUM_SHIFT + NUM_BITS;
const SIZE_SHIFT: RawOpcode = GROUP_SHIFT + GROUP_BITS;
const DIR_SHIFT: RawOpcode = SIZE_SHIFT + SIZE_BITS;

const NUM_MASK: RawOpcode = (1 << NUM_BITS) - 1;
const GROUP_MASK: RawOpcode = (1 << GROUP_BITS) - 1;
const SIZE_MASK: RawOpcode = (1 << SIZE_BITS) - 1;
const DIR_MASK: RawOpcode = (1 << DIR_BITS) - 1;

#[cfg(any(
    target_arch = "x86",
    target_arch = "arm",
    target_arch = "s390x",
    target_arch = "x86_64",
    target_arch = "aarch64",
    target_arch = "riscv32",
    target_arch = "riscv64",
    target_arch = "loongarch64"
))]
mod consts {
    use super::RawOpcode;

    pub(super) const NONE: RawOpcode = 0;
    pub(super) const READ: RawOpcode = 2;
    pub(super) const WRITE: RawOpcode = 1;
    pub(super) const SIZE_BITS: RawOpcode = 14;
    pub(super) const DIR_BITS: RawOpcode = 2;
}

#[cfg(any(
    target_arch = "mips",
    target_arch = "mips64",
    target_arch = "powerpc",
    target_arch = "powerpc64",
    target_arch = "sparc",
    target_arch = "sparc64"
))]
mod consts {
    use super::RawOpcode;

    pub(super) const NONE: RawOpcode = 1;
    pub(super) const READ: RawOpcode = 2;
    pub(super) const WRITE: RawOpcode = 4;
    pub(super) const SIZE_BITS: RawOpcode = 13;
    pub(super) const DIR_BITS: RawOpcode = 3;
}
