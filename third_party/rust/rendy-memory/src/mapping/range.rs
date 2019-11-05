use {
    crate::util::fits_usize,
    std::{
        mem::{align_of, size_of},
        ops::Range,
        ptr::NonNull,
        slice::{from_raw_parts, from_raw_parts_mut},
    },
};

/// Get sub-range of memory mapping.
/// `range` and `fitting` is in memory object space.
/// `ptr` points to the `range.start` offset from memory origin.
/// returns pointer to `fitting.start` offset from memory origin
/// if `fitting` is contained in `range`.
pub(crate) fn mapped_fitting_range(
    ptr: NonNull<u8>,
    range: Range<u64>,
    fitting: Range<u64>,
) -> Option<NonNull<u8>> {
    assert!(
        range.start < range.end,
        "Memory mapping region must have valid size"
    );
    assert!(
        fitting.start < fitting.end,
        "Memory mapping region must have valid size"
    );
    assert!(fits_usize(range.end - range.start));
    assert!(usize::max_value() - (range.end - range.start) as usize >= ptr.as_ptr() as usize);

    if fitting.start < range.start || fitting.end > range.end {
        None
    } else {
        Some(unsafe {
            // for x > 0 and y >= 0: x + y > 0. No overflow due to checks above.
            NonNull::new_unchecked(
                (ptr.as_ptr() as usize + (fitting.start - range.start) as usize) as *mut u8,
            )
        })
    }
}

/// Get sub-range of memory mapping.
/// `range` is in memory object space.
/// `sub` is a range inside `range`.
/// `ptr` points to the `range.start` offset from memory origin.
/// returns pointer to the `range.starti + sub.start` offset from memory origin
/// if `sub` fits in `range`.
pub(crate) fn mapped_sub_range(
    ptr: NonNull<u8>,
    range: Range<u64>,
    sub: Range<u64>,
) -> Option<(NonNull<u8>, Range<u64>)> {
    let fitting = sub.start.checked_add(range.start)?..sub.end.checked_add(range.start)?;
    let ptr = mapped_fitting_range(ptr, range, fitting.clone())?;
    Some((ptr, fitting))
}

/// # Safety
///
/// User must ensure that:
/// * this function won't create aliasing slices.
/// * returned slice doesn't outlive mapping.
/// * `T` Must be plain-old-data type compatible with data in mapped region.
pub(crate) unsafe fn mapped_slice_mut<'a, T>(ptr: NonNull<u8>, size: usize) -> &'a mut [T] {
    assert_eq!(
        size % size_of::<T>(),
        0,
        "Range length must be multiple of element size"
    );
    let offset = ptr.as_ptr() as usize;
    assert_eq!(
        offset % align_of::<T>(),
        0,
        "Range offset must be multiple of element alignment"
    );
    assert!(usize::max_value() - size >= ptr.as_ptr() as usize);
    from_raw_parts_mut(ptr.as_ptr() as *mut T, size)
}

/// # Safety
///
/// User must ensure that:
/// * returned slice doesn't outlive mapping.
/// * `T` Must be plain-old-data type compatible with data in mapped region.
pub(crate) unsafe fn mapped_slice<'a, T>(ptr: NonNull<u8>, size: usize) -> &'a [T] {
    assert_eq!(
        size % size_of::<T>(),
        0,
        "Range length must be multiple of element size"
    );
    let offset = ptr.as_ptr() as usize;
    assert_eq!(
        offset % align_of::<T>(),
        0,
        "Range offset must be multiple of element alignment"
    );
    assert!(usize::max_value() - size >= ptr.as_ptr() as usize);
    from_raw_parts(ptr.as_ptr() as *const T, size)
}
