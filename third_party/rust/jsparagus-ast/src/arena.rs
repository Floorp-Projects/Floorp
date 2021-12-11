use bumpalo::Bump;

use std::{
    self,
    fmt::{self, Debug, Formatter},
    ops, ptr,
};

pub use bumpalo::collections::{String, Vec};

pub struct Box<'alloc, T: ?Sized>(&'alloc mut T);

impl<'alloc, T> Box<'alloc, T> {
    pub fn unbox(self) -> T {
        // This pointer read is safe because the reference `self.0` is
        // guaranteed to be unique--not just now, but we're guaranteed it's not
        // borrowed from some other reference. This in turn is because we never
        // construct an alloc::Box with a borrowed reference, only with a fresh
        // one just allocated from a Bump.
        unsafe { ptr::read(self.0 as *mut T) }
    }
}

impl<'alloc, T: ?Sized> ops::Deref for Box<'alloc, T> {
    type Target = T;

    fn deref(&self) -> &T {
        self.0
    }
}

impl<'alloc, T: ?Sized> ops::DerefMut for Box<'alloc, T> {
    fn deref_mut(&mut self) -> &mut T {
        self.0
    }
}

impl<'alloc, T: ?Sized> Debug for Box<'alloc, T>
where
    T: Debug,
{
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl<'alloc, T> PartialEq for Box<'alloc, T>
where
    T: PartialEq<T> + ?Sized,
{
    fn eq(&self, other: &Box<'alloc, T>) -> bool {
        PartialEq::eq(&**self, &**other)
    }

    fn ne(&self, other: &Box<'alloc, T>) -> bool {
        PartialEq::ne(&**self, &**other)
    }
}

pub fn alloc<'alloc, T>(allocator: &'alloc Bump, value: T) -> Box<'alloc, T> {
    Box(allocator.alloc(value))
}

pub fn alloc_with<'alloc, F, T>(allocator: &'alloc Bump, gen: F) -> Box<'alloc, T>
where
    F: FnOnce() -> T,
{
    Box(allocator.alloc_with(gen))
}

pub fn alloc_str<'alloc>(allocator: &'alloc Bump, value: &str) -> &'alloc str {
    String::from_str_in(value, allocator).into_bump_str()
}

pub fn map_vec<'alloc, A, B>(
    source: &mut Vec<A>,
    mut map_fn: impl FnMut(&mut A) -> B,
    allocator: &'alloc Bump,
) -> Vec<'alloc, B> {
    let mut out = Vec::with_capacity_in(source.len(), allocator);
    for item in source {
        out.push(map_fn(item));
    }
    out
}
