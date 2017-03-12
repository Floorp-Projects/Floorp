use std::ops;

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct DeviceType(u32);

const DEVICE_TYPE_UNKNOWN: u32 = 0b00;
const DEVICE_TYPE_INPUT:u32 = 0b01;
const DEVICE_TYPE_OUTPUT: u32 = 0b10;
const DEVICE_TYPE_ALL: u32 = 0b11;

impl DeviceType {
    pub fn unknown() -> Self { DeviceType(DEVICE_TYPE_UNKNOWN) }

    #[inline] pub fn input() -> Self { DeviceType(DEVICE_TYPE_INPUT) }
    #[inline] pub fn output() -> Self { DeviceType(DEVICE_TYPE_OUTPUT) }
    #[inline] pub fn all() -> Self { DeviceType(DEVICE_TYPE_ALL) }

    #[inline] pub fn is_input(&self) -> bool { self.contains(DeviceType::input()) }
    #[inline] pub fn is_output(&self) -> bool { self.contains(DeviceType::output()) }

    #[inline] pub fn contains(&self, other: Self) -> bool { (*self & other) == other }
    #[inline] pub fn insert(&mut self, other: Self) { self.0 |= other.0; }
    #[inline] pub fn remove(&mut self, other: Self) { self.0 &= !other.0; }
}

impl ops::BitOr for DeviceType {
    type Output = DeviceType;

    #[inline]
    fn bitor(self, other: Self) -> Self {
        DeviceType(self.0 | other.0)
    }
}

impl ops::BitXor for DeviceType {
    type Output = DeviceType;

    #[inline]
    fn bitxor(self, other: Self) -> Self {
        DeviceType(self.0 ^ other.0)
    }
}

impl ops::BitAnd for DeviceType {
    type Output = DeviceType;

    #[inline]
    fn bitand(self, other: Self) -> Self {
        DeviceType(self.0 & other.0)
    }
}

impl ops::Sub for DeviceType {
    type Output = DeviceType;

    #[inline]
    fn sub(self, other: Self) -> Self {
        DeviceType(self.0 & !other.0)
    }
}

impl ops::Not for DeviceType {
    type Output = DeviceType;

    #[inline]
    fn not(self) -> Self {
        DeviceType(!self.0 & DEVICE_TYPE_ALL)
    }
}
