use std::ops;

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct DeviceFmt(u32);

const DEVICE_FMT_S16LE: u32 = 0x0010;
const DEVICE_FMT_S16BE: u32 = 0x0020;
const DEVICE_FMT_F32LE: u32 = 0x1000;
const DEVICE_FMT_F32BE: u32 = 0x2000;
const DEVICE_FMT_S16_MASK: u32 = DEVICE_FMT_S16LE | DEVICE_FMT_S16BE;
const DEVICE_FMT_F32_MASK: u32 = DEVICE_FMT_F32LE | DEVICE_FMT_F32BE;
const DEVICE_FMT_ALL: u32   = DEVICE_FMT_S16_MASK | DEVICE_FMT_F32_MASK;


impl DeviceFmt {
    pub fn empty() -> Self { DeviceFmt(0) }

    #[inline] pub fn s16le() -> Self { DeviceFmt(DEVICE_FMT_S16LE) }
    #[inline] pub fn s16be() -> Self { DeviceFmt(DEVICE_FMT_S16BE) }
    #[inline] pub fn f32le() -> Self { DeviceFmt(DEVICE_FMT_F32LE) }
    #[inline] pub fn f32be() -> Self { DeviceFmt(DEVICE_FMT_F32BE) }
    #[inline] pub fn all() -> Self { DeviceFmt(DEVICE_FMT_ALL) }

    #[inline] pub fn s16ne() -> Self {
        if cfg!(target_endian = "little") {
            DeviceFmt::s16le()
        } else {
            DeviceFmt::s16be()
        }
    }

    #[inline] pub fn f32ne() -> Self {
        if cfg!(target_endian = "little") {
            DeviceFmt::f32le()
        } else {
            DeviceFmt::f32be()
        }
    }

    #[inline] pub fn contains(&self, other: Self) -> bool { (*self & other) == other }
    #[inline] pub fn insert(&mut self, other: Self) { self.0 |= other.0; }
    #[inline] pub fn remove(&mut self, other: Self) { self.0 &= !other.0; }
}

impl ops::BitOr for DeviceFmt {
    type Output = DeviceFmt;

    #[inline]
    fn bitor(self, other: Self) -> Self {
        DeviceFmt(self.0 | other.0)
    }
}

impl ops::BitXor for DeviceFmt {
    type Output = DeviceFmt;

    #[inline]
    fn bitxor(self, other: Self) -> Self {
        DeviceFmt(self.0 ^ other.0)
    }
}

impl ops::BitAnd for DeviceFmt {
    type Output = DeviceFmt;

    #[inline]
    fn bitand(self, other: Self) -> Self {
        DeviceFmt(self.0 & other.0)
    }
}

impl ops::Sub for DeviceFmt {
    type Output = DeviceFmt;

    #[inline]
    fn sub(self, other: Self) -> Self {
        DeviceFmt(self.0 & !other.0)
    }
}

impl ops::Not for DeviceFmt {
    type Output = DeviceFmt;

    #[inline]
    fn not(self) -> Self {
        DeviceFmt(!self.0 & DEVICE_FMT_ALL)
    }
}
