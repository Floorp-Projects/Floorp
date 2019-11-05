use std::{
    alloc, mem,
    ptr::{self, NonNull},
};

/// A typesafe helper that stores the allocated pointer without the data initialized.
pub struct BoxAllocation<T>(
    // ptr cannot be null since it would mean the allocation failed.
    // Note: covariance is acceptable since this eventually becomes a `Box<T>`,
    // which is covariant too.
    NonNull<T>,
);

impl<T> BoxAllocation<T> {
    /// Consumes self and writes the given value into the allocation.
    #[inline(always)] // if this does not get inlined then copying happens
    pub fn init(self, value: T) -> Box<T> {
        if mem::size_of::<T>() == 0 {
            return Box::new(value);
        }

        unsafe {
            let ptr = self.0.as_ptr();
            mem::forget(self);
            ptr::write(ptr, value);
            Box::from_raw(ptr)
        }
    }
}

impl<T> Drop for BoxAllocation<T> {
    fn drop(&mut self) {
        if mem::size_of::<T>() == 0 {
            return;
        }

        let layout = alloc::Layout::new::<T>();
        unsafe {
            alloc::dealloc(self.0.as_ptr() as *mut u8, layout);
        }
    }
}

/// Helper trait for a `Box` type that allocates up-front.
pub trait BoxHelper<T> {
    /// Allocates the storage without providing any data.
    fn alloc() -> BoxAllocation<T>;
}

impl<T> BoxHelper<T> for Box<T> {
    fn alloc() -> BoxAllocation<T> {
        if mem::size_of::<T>() == 0 {
            return BoxAllocation(NonNull::dangling());
        }

        let layout = alloc::Layout::new::<T>();
        BoxAllocation(
            NonNull::new(unsafe { alloc::alloc(layout) as *mut T })
                .unwrap_or_else(|| alloc::handle_alloc_error(layout)), // oom
        )
    }
}
