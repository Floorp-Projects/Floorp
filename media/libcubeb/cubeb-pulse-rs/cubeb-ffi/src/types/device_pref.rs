use std::ops;

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct DevicePref(u32);

const DEVICE_PREF_MULTIMEDIA: u32 = 0x1;
const DEVICE_PREF_VOICE: u32 = 0x2;
const DEVICE_PREF_NOTIFICATION: u32 = 0x4;
const DEVICE_PREF_ALL: u32 = 0xF;

impl DevicePref {
    pub fn none() -> Self { DevicePref(0) }

    #[inline] pub fn multimedia() -> Self { DevicePref(DEVICE_PREF_MULTIMEDIA) }
    #[inline] pub fn voice() -> Self { DevicePref(DEVICE_PREF_VOICE) }
    #[inline] pub fn notification() -> Self { DevicePref(DEVICE_PREF_NOTIFICATION) }
    #[inline] pub fn all() -> Self { DevicePref(DEVICE_PREF_ALL) }

    #[inline] pub fn contains(&self, other: Self) -> bool { (*self & other) == other }
    #[inline] pub fn insert(&mut self, other: Self) { self.0 |= other.0; }
    #[inline] pub fn remove(&mut self, other: Self) { self.0 &= !other.0; }
}

impl ops::BitOr for DevicePref {
    type Output = DevicePref;

    #[inline]
    fn bitor(self, other: Self) -> Self {
        DevicePref(self.0 | other.0)
    }
}

impl ops::BitXor for DevicePref {
    type Output = DevicePref;

    #[inline]
    fn bitxor(self, other: Self) -> Self {
        DevicePref(self.0 ^ other.0)
    }
}

impl ops::BitAnd for DevicePref {
    type Output = DevicePref;

    #[inline]
    fn bitand(self, other: Self) -> Self {
        DevicePref(self.0 & other.0)
    }
}

impl ops::Sub for DevicePref {
    type Output = DevicePref;

    #[inline]
    fn sub(self, other: Self) -> Self {
        DevicePref(self.0 & !other.0)
    }
}

impl ops::Not for DevicePref {
    type Output = DevicePref;

    #[inline]
    fn not(self) -> Self {
        DevicePref(!self.0 & DEVICE_PREF_ALL)
    }
}
