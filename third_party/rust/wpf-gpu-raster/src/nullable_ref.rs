use std::{marker::PhantomData, ops::Deref};

pub struct Ref<'a, T> {
    ptr: *const T,
    _phantom: PhantomData<&'a T>
}

impl<'a, T> Copy for Ref<'a, T> { }

impl<'a, T> Clone for Ref<'a, T> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<'a, T> Ref<'a, T> {
    pub fn new(p: &'a T) -> Self {
        Ref { ptr: p as *const T, _phantom: PhantomData}
    }
    pub unsafe fn null() -> Self {
        Ref { ptr: std::ptr::null(), _phantom: PhantomData}
    }
    pub fn is_null(&self) -> bool {
        self.ptr.is_null()
    }
    pub fn get_ref(self) -> &'a T {
        unsafe { &*self.ptr }
    }
}

impl<'a, T> PartialEq for Ref<'a, T> {
    fn eq(&self, other: &Self) -> bool {
        self.ptr == other.ptr && self._phantom == other._phantom
    }
}

impl<'a, T> PartialOrd for Ref<'a, T> {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        match self.ptr.partial_cmp(&other.ptr) {
            Some(core::cmp::Ordering::Equal) => {}
            ord => return ord,
        }
        self._phantom.partial_cmp(&other._phantom)
    }
}

impl<'a, T> Deref for Ref<'a, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        unsafe { &*self.ptr }
    }
}