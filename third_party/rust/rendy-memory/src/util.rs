pub(crate) fn aligned(value: u64, align: u64) -> u64 {
    debug_assert_ne!(align, 0);
    debug_assert_eq!(align.count_ones(), 1);
    if value == 0 {
        0
    } else {
        1u64 + ((value - 1u64) | (align - 1u64))
    }
}

pub(crate) trait IntegerFitting {
    fn fits_usize(self) -> bool;
    fn fits_isize(self) -> bool;

    fn usize_fits(value: usize) -> bool;
    fn isize_fits(value: isize) -> bool;
}

#[cfg(any(target_pointer_width = "16", target_pointer_width = "32"))]
impl IntegerFitting for u64 {
    fn fits_usize(self) -> bool {
        self <= usize::max_value() as u64
    }
    fn fits_isize(self) -> bool {
        self <= isize::max_value() as u64
    }
    fn usize_fits(_value: usize) -> bool {
        true
    }
    fn isize_fits(value: isize) -> bool {
        value >= 0
    }
}

#[cfg(target_pointer_width = "64")]
impl IntegerFitting for u64 {
    fn fits_usize(self) -> bool {
        true
    }
    fn fits_isize(self) -> bool {
        self <= isize::max_value() as u64
    }
    fn usize_fits(_value: usize) -> bool {
        true
    }
    fn isize_fits(value: isize) -> bool {
        value >= 0
    }
}

#[cfg(not(any(
    target_pointer_width = "16",
    target_pointer_width = "32",
    target_pointer_width = "64"
)))]
impl IntegerFitting for u64 {
    fn fits_usize(self) -> bool {
        true
    }
    fn fits_isize(self) -> bool {
        true
    }
    fn usize_fits(value: usize) -> bool {
        value <= u64::max_value() as usize
    }
    fn isize_fits(value: isize) -> bool {
        value >= 0 && value <= u64::max_value() as isize
    }
}

#[cfg(target_pointer_width = "16")]
impl IntegerFitting for u32 {
    fn fits_usize(self) -> bool {
        self <= usize::max_value() as u32
    }
    fn fits_isize(self) -> bool {
        self <= isize::max_value() as u32
    }
    fn usize_fits(_value: usize) -> bool {
        true
    }
    fn isize_fits(value: isize) -> bool {
        value >= 0
    }
}

#[cfg(target_pointer_width = "32")]
impl IntegerFitting for u32 {
    fn fits_usize(self) -> bool {
        true
    }
    fn fits_isize(self) -> bool {
        self <= isize::max_value() as u32
    }
    fn usize_fits(_value: usize) -> bool {
        true
    }
    fn isize_fits(value: isize) -> bool {
        value >= 0
    }
}

#[cfg(not(any(target_pointer_width = "16", target_pointer_width = "32")))]
impl IntegerFitting for u32 {
    fn fits_usize(self) -> bool {
        true
    }
    fn fits_isize(self) -> bool {
        true
    }
    fn usize_fits(value: usize) -> bool {
        value <= u32::max_value() as usize
    }
    fn isize_fits(value: isize) -> bool {
        value >= 0 && value <= u32::max_value() as isize
    }
}

pub(crate) fn fits_usize<T: IntegerFitting>(value: T) -> bool {
    value.fits_usize()
}

pub(crate) fn fits_u32(value: usize) -> bool {
    u32::usize_fits(value)
}
