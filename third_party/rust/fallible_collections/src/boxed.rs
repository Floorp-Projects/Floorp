//! Implement Fallible Box
use super::TryClone;
use crate::TryReserveError;
use alloc::alloc::Layout;
use alloc::boxed::Box;
use core::borrow::Borrow;
use core::ops::Deref;
use core::ptr::NonNull;

/// trait to implement Fallible Box
pub trait FallibleBox<T> {
    /// try creating a new box, returning a Result<Box<T>,
    /// TryReserveError> if allocation failed
    fn try_new(t: T) -> Result<Self, TryReserveError>
    where
        Self: Sized;
}
/// TryBox is a thin wrapper around alloc::boxed::Box to provide support for
/// fallible allocation.
///
/// See the crate documentation for more.
pub struct TryBox<T> {
    inner: Box<T>,
}

impl<T> TryBox<T> {
    #[inline]
    pub fn try_new(t: T) -> Result<Self, TryReserveError> {
        Ok(Self {
            inner: <Box<T> as FallibleBox<T>>::try_new(t)?,
        })
    }

    #[inline(always)]
    pub fn into_raw(b: TryBox<T>) -> *mut T {
        Box::into_raw(b.inner)
    }

    /// # Safety
    ///
    /// See std::boxed::from_raw
    #[inline(always)]
    pub unsafe fn from_raw(raw: *mut T) -> Self {
        Self {
            inner: Box::from_raw(raw),
        }
    }
}

impl<T: TryClone> TryClone for TryBox<T> {
    fn try_clone(&self) -> Result<Self, TryReserveError> {
        let clone: T = (*self.inner).try_clone()?;
        Self::try_new(clone)
    }
}

impl<T> Deref for TryBox<T> {
    type Target = T;

    #[inline(always)]
    fn deref(&self) -> &T {
        self.inner.deref()
    }
}

fn alloc(layout: Layout) -> Result<NonNull<u8>, TryReserveError> {
    #[cfg(feature = "unstable")] // requires allocator_api
    {
        use core::alloc::Allocator;
        alloc::alloc::Global
            .allocate(layout)
            .map_err(|_e| TryReserveError::AllocError {
                layout,
                #[cfg(not(feature = "rust_1_57"))]
                non_exhaustive: (),
            })
            .map(|v| v.cast())
    }
    #[cfg(not(feature = "unstable"))]
    {
        match layout.size() {
            0 => {
                // Required for alloc safety
                // See https://doc.rust-lang.org/stable/std/alloc/trait.GlobalAlloc.html#safety-1
                Ok(NonNull::dangling())
            }
            1..=core::usize::MAX => {
                let ptr = unsafe { alloc::alloc::alloc(layout) };
                core::ptr::NonNull::new(ptr).ok_or(TryReserveError::AllocError { layout })
            }
            _ => unreachable!("size must be non-negative"),
        }
    }
}

impl<T> FallibleBox<T> for Box<T> {
    fn try_new(t: T) -> Result<Self, TryReserveError> {
        let layout = Layout::for_value(&t);
        let ptr = alloc(layout)?.as_ptr() as *mut T;
        unsafe {
            core::ptr::write(ptr, t);
            Ok(Box::from_raw(ptr))
        }
    }
}

impl<T: TryClone> TryClone for Box<T> {
    #[inline]
    fn try_clone(&self) -> Result<Self, TryReserveError> {
        <Self as FallibleBox<T>>::try_new(Borrow::<T>::borrow(self).try_clone()?)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn boxed() {
        let mut v = <Box<_> as FallibleBox<_>>::try_new(5).unwrap();
        assert_eq!(*v, 5);
        *v = 3;
        assert_eq!(*v, 3);
    }
    // #[test]
    // fn big_alloc() {
    //     let layout = Layout::from_size_align(1_000_000_000_000, 8).unwrap();
    //     let ptr = unsafe { alloc::alloc::alloc(layout) };
    //     assert!(ptr.is_null());
    // }

    #[test]
    fn trybox_zst() {
        let b = <Box<_> as FallibleBox<_>>::try_new(()).expect("ok");
        assert_eq!(b, Box::new(()));
    }

    struct NonCopyType;

    #[test]
    fn trybox_deref() {
        let try_box: TryBox<NonCopyType> = TryBox::try_new(NonCopyType {}).unwrap();
        let _derefed: &NonCopyType = try_box.deref();
    }

    #[test]
    fn trybox_as_deref() {
        let try_box_option: Option<TryBox<NonCopyType>> =
            Some(TryBox::try_new(NonCopyType).unwrap());
        let _ref_option: Option<&NonCopyType> = try_box_option.as_deref();
    }
}
