//! `ioctl` opcode behavior for Linux platforms.

use super::{Direction, RawOpcode};

pub(super) const fn compose_opcode(
    dir: Direction,
    group: RawOpcode,
    num: RawOpcode,
    size: RawOpcode,
) -> RawOpcode {
    let dir = match dir {
        Direction::None => NONE,
        Direction::Read => READ,
        Direction::Write => WRITE,
        Direction::ReadWrite => READ | WRITE,
    };

    dir | num | (group << 8) | ((size & IOCPARAM_MASK) << 16)
}

pub const NONE: RawOpcode = 0x2000_0000;
pub const WRITE: RawOpcode = 0x4000_0000;
pub const READ: RawOpcode = 0x8000_0000;
pub const IOCPARAM_MASK: RawOpcode = 0x1FFF;
