//! Implement Fallible Vec
use super::TryClone;
use crate::TryReserveError;
#[allow(unused_imports)]
use alloc::alloc::{alloc, realloc, Layout};
use alloc::vec::Vec;
use core::convert::TryInto as _;

#[cfg(feature = "unstable")]
#[macro_export]
/// macro trying to create a vec, return a
/// Result<Vec<T>,TryReserveError>
macro_rules! try_vec {
   ($elem:expr; $n:expr) => (
        $crate::vec::try_from_elem($elem, $n)
    );
    ($($x:expr),*) => (
        match <alloc::boxed::Box<_> as $crate::boxed::FallibleBox<_>>::try_new([$($x),*]) {
            Err(e) => Err(e),
            Ok(b) => Ok(<[_]>::into_vec(b)),
        }
    );
    ($($x:expr,)*) => ($crate::try_vec![$($x),*])
}

/// trait implementing all fallible methods on vec
pub trait FallibleVec<T> {
    /// see reserve
    fn try_reserve(&mut self, additional: usize) -> Result<(), TryReserveError>;
    /// see push
    fn try_push(&mut self, elem: T) -> Result<(), TryReserveError>;
    /// try push and give back ownership in case of error
    fn try_push_give_back(&mut self, elem: T) -> Result<(), (T, TryReserveError)>;
    /// see with capacity, (Self must be sized by the constraint of Result)
    fn try_with_capacity(capacity: usize) -> Result<Self, TryReserveError>
    where
        Self: core::marker::Sized;
    /// see insert
    fn try_insert(&mut self, index: usize, element: T) -> Result<(), (T, TryReserveError)>;
    /// see append
    fn try_append(&mut self, other: &mut Self) -> Result<(), TryReserveError>;
    /// see resize, only works when the `value` implements Copy, otherwise, look at try_resize_no_copy
    fn try_resize(&mut self, new_len: usize, value: T) -> Result<(), TryReserveError>
    where
        T: Copy + Clone;
    fn try_resize_with<F>(&mut self, new_len: usize, f: F) -> Result<(), TryReserveError>
    where
        F: FnMut() -> T;
    /// resize the vec by trying to clone the value repeatingly
    fn try_resize_no_copy(&mut self, new_len: usize, value: T) -> Result<(), TryReserveError>
    where
        T: TryClone;
    /// see resize, only works when the `value` implements Copy, otherwise, look at try_extend_from_slice_no_copy
    fn try_extend_from_slice(&mut self, other: &[T]) -> Result<(), TryReserveError>
    where
        T: Copy + Clone;
    /// extend the vec by trying to clone the value in `other`
    fn try_extend_from_slice_no_copy(&mut self, other: &[T]) -> Result<(), TryReserveError>
    where
        T: TryClone;
}

/// TryVec is a thin wrapper around alloc::vec::Vec to provide support for
/// fallible allocation.
///
/// See the crate documentation for more.
#[derive(PartialEq)]
pub struct TryVec<T> {
    inner: Vec<T>,
}

impl<T> Default for TryVec<T> {
    #[inline(always)]
    fn default() -> Self {
        Self {
            inner: Default::default(),
        }
    }
}

impl<T: core::fmt::Debug> core::fmt::Debug for TryVec<T> {
    #[inline]
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        self.inner.fmt(f)
    }
}

impl<T> TryVec<T> {
    #[inline(always)]
    pub fn new() -> Self {
        Self { inner: Vec::new() }
    }

    #[inline]
    pub fn with_capacity(capacity: usize) -> Result<Self, TryReserveError> {
        Ok(Self {
            inner: FallibleVec::try_with_capacity(capacity)?,
        })
    }

    #[inline(always)]
    pub fn append(&mut self, other: &mut Self) -> Result<(), TryReserveError> {
        FallibleVec::try_append(&mut self.inner, &mut other.inner)
    }

    #[inline(always)]
    pub fn as_mut_slice(&mut self) -> &mut [T] {
        self
    }

    #[inline(always)]
    pub fn as_slice(&self) -> &[T] {
        self
    }

    #[inline(always)]
    pub fn clear(&mut self) {
        self.inner.clear()
    }

    #[cfg(test)]
    pub fn into_inner(self) -> Vec<T> {
        self.inner
    }

    #[inline(always)]
    pub fn is_empty(&self) -> bool {
        self.inner.is_empty()
    }

    #[inline(always)]
    pub fn iter_mut(&mut self) -> IterMut<T> {
        IterMut {
            inner: self.inner.iter_mut(),
        }
    }

    #[inline(always)]
    pub fn iter(&self) -> Iter<T> {
        Iter {
            inner: self.inner.iter(),
        }
    }

    #[inline(always)]
    pub fn pop(&mut self) -> Option<T> {
        self.inner.pop()
    }

    #[inline(always)]
    pub fn push(&mut self, value: T) -> Result<(), TryReserveError> {
        FallibleVec::try_push(&mut self.inner, value)
    }

    #[inline(always)]
    pub fn reserve(&mut self, additional: usize) -> Result<(), TryReserveError> {
        FallibleVec::try_reserve(&mut self.inner, additional)
    }

    #[inline(always)]
    pub fn resize_with<F>(&mut self, new_len: usize, f: F) -> Result<(), TryReserveError>
    where
        F: FnMut() -> T,
    {
        FallibleVec::try_resize_with(&mut self.inner, new_len, f)
    }
}

impl<T: TryClone> TryClone for TryVec<T> {
    #[inline]
    fn try_clone(&self) -> Result<Self, TryReserveError> {
        self.as_slice().try_into()
    }
}

impl<T: TryClone> TryVec<TryVec<T>> {
    pub fn concat(&self) -> Result<TryVec<T>, TryReserveError> {
        let size = self.iter().map(|v| v.inner.len()).sum();
        let mut result = TryVec::with_capacity(size)?;
        for v in self.iter() {
            result.inner.try_extend_from_slice_no_copy(&v.inner)?;
        }
        Ok(result)
    }
}

impl<T: TryClone> TryVec<T> {
    #[inline(always)]
    pub fn extend_from_slice(&mut self, other: &[T]) -> Result<(), TryReserveError> {
        self.inner.try_extend_from_slice_no_copy(other)
    }
}

impl<T> IntoIterator for TryVec<T> {
    type Item = T;
    type IntoIter = alloc::vec::IntoIter<T>;

    #[inline(always)]
    fn into_iter(self) -> Self::IntoIter {
        self.inner.into_iter()
    }
}

impl<'a, T> IntoIterator for &'a TryVec<T> {
    type Item = &'a T;
    type IntoIter = alloc::slice::Iter<'a, T>;

    #[inline(always)]
    fn into_iter(self) -> Self::IntoIter {
        self.inner.iter()
    }
}

#[cfg(feature = "std_io")]
pub mod std_io {
    use super::*;
    use std::io::{self, Read, Take, Write};

    pub trait TryRead {
        fn try_read_to_end(&mut self, buf: &mut TryVec<u8>) -> io::Result<usize>;

        #[inline]
        fn read_into_try_vec(&mut self) -> io::Result<TryVec<u8>> {
            let mut buf = TryVec::new();
            self.try_read_to_end(&mut buf)?;
            Ok(buf)
        }
    }

    impl<T: Read> TryRead for Take<T> {
        /// This function reserves the upper limit of what `src` can generate before
        /// reading all bytes until EOF in this source, placing them into `buf`. If the
        /// allocation is unsuccessful, or reading from the source generates an error
        /// before reaching EOF, this will return an error. Otherwise, it will return
        /// the number of bytes read.
        ///
        /// Since `Take::limit()` may return a value greater than the number of bytes
        /// which can be read from the source, it's possible this function may fail
        /// in the allocation phase even though allocating the number of bytes available
        /// to read would have succeeded. In general, it is assumed that the callers
        /// have accurate knowledge of the number of bytes of interest and have created
        /// `src` accordingly.
        #[inline]
        fn try_read_to_end(&mut self, buf: &mut TryVec<u8>) -> io::Result<usize> {
            try_read_up_to(self, self.limit(), buf)
        }
    }

    /// Read up to `limit` bytes from `src`, placing them into `buf` and returning the
    /// number of bytes read. Space for `limit` additional bytes is reserved in `buf`, so
    /// this function will return an error if the allocation fails.
    pub fn try_read_up_to<R: Read>(
        src: &mut R,
        limit: u64,
        buf: &mut TryVec<u8>,
    ) -> io::Result<usize> {
        let additional = limit
            .try_into()
            .map_err(|e| io::Error::new(io::ErrorKind::Other, e))?;
        buf.reserve(additional)
            .map_err(|_| io::Error::new(io::ErrorKind::Other, "reserve allocation failed"))?;
        let bytes_read = src.take(limit).read_to_end(&mut buf.inner)?;
        Ok(bytes_read)
    }

    impl Write for TryVec<u8> {
        fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
            self.extend_from_slice(buf)
                .map_err(|_| io::Error::new(io::ErrorKind::Other, "extend_from_slice failed"))?;
            Ok(buf.len())
        }

        #[inline(always)]
        fn flush(&mut self) -> io::Result<()> {
            Ok(())
        }
    }

    #[cfg(test)]
    mod tests {
        use super::*;

        #[test]
        fn try_read_to_end() {
            let mut src = b"1234567890".take(5);
            let mut buf = TryVec::new();
            src.try_read_to_end(&mut buf).unwrap();
            assert_eq!(buf.len(), 5);
            assert_eq!(buf, b"12345".as_ref());
        }

        #[test]
        fn read_into_try_vec() {
            let mut src = b"1234567890".take(5);
            let buf = src.read_into_try_vec().unwrap();
            assert_eq!(buf.len(), 5);
            assert_eq!(buf, b"12345".as_ref());
        }

        #[test]
        fn read_into_try_vec_oom() {
            let mut src = b"1234567890".take(core::usize::MAX.try_into().expect("usize < u64"));
            assert!(src.read_into_try_vec().is_err());
        }

        #[test]
        fn try_read_up_to() {
            let src = b"1234567890";
            let mut buf = TryVec::new();
            super::try_read_up_to(&mut src.as_ref(), 5, &mut buf).unwrap();
            assert_eq!(buf.len(), 5);
            assert_eq!(buf, b"12345".as_ref());
        }

        #[test]
        fn try_read_up_to_oom() {
            let src = b"1234567890";
            let mut buf = TryVec::new();
            let limit = core::usize::MAX.try_into().expect("usize < u64");
            let res = super::try_read_up_to(&mut src.as_ref(), limit, &mut buf);
            assert!(res.is_err());
        }
    }
}

impl<T: PartialEq> PartialEq<Vec<T>> for TryVec<T> {
    #[inline(always)]
    fn eq(&self, other: &Vec<T>) -> bool {
        self.inner.eq(other)
    }
}

impl<'a, T: PartialEq> PartialEq<&'a [T]> for TryVec<T> {
    #[inline(always)]
    fn eq(&self, other: &&[T]) -> bool {
        self.inner.eq(other)
    }
}

impl PartialEq<&str> for TryVec<u8> {
    #[inline]
    fn eq(&self, other: &&str) -> bool {
        self.as_slice() == other.as_bytes()
    }
}

impl core::convert::AsRef<[u8]> for TryVec<u8> {
    #[inline(always)]
    fn as_ref(&self) -> &[u8] {
        self.inner.as_ref()
    }
}

impl<T> core::convert::From<Vec<T>> for TryVec<T> {
    #[inline(always)]
    fn from(value: Vec<T>) -> Self {
        Self { inner: value }
    }
}

impl<T: TryClone> core::convert::TryFrom<&[T]> for TryVec<T> {
    type Error = TryReserveError;

    #[inline]
    fn try_from(value: &[T]) -> Result<Self, Self::Error> {
        let mut v = Self::new();
        v.inner.try_extend_from_slice_no_copy(value)?;
        Ok(v)
    }
}

impl core::convert::TryFrom<&str> for TryVec<u8> {
    type Error = TryReserveError;

    #[inline]
    fn try_from(value: &str) -> Result<Self, Self::Error> {
        let mut v = Self::new();
        v.extend_from_slice(value.as_bytes())?;
        Ok(v)
    }
}

impl<T> core::ops::Deref for TryVec<T> {
    type Target = [T];

    #[inline(always)]
    fn deref(&self) -> &[T] {
        self.inner.deref()
    }
}

impl<T> core::ops::DerefMut for TryVec<T> {
    fn deref_mut(&mut self) -> &mut [T] {
        self.inner.deref_mut()
    }
}

pub struct Iter<'a, T> {
    inner: alloc::slice::Iter<'a, T>,
}

impl<'a, T> Iterator for Iter<'a, T> {
    type Item = &'a T;

    #[inline(always)]
    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next()
    }

    #[inline(always)]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.inner.size_hint()
    }
}

pub struct IterMut<'a, T> {
    inner: alloc::slice::IterMut<'a, T>,
}

impl<'a, T> Iterator for IterMut<'a, T> {
    type Item = &'a mut T;

    #[inline(always)]
    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next()
    }

    #[inline(always)]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.inner.size_hint()
    }
}

/// Grow capacity exponentially
#[cold]
fn vec_try_reserve_for_growth<T>(v: &mut Vec<T>, additional: usize) -> Result<(), TryReserveError> {
    // saturating, because can't use CapacityOverflow here if rust_1_57 flag is enabled
    FallibleVec::try_reserve(v, additional.max(v.capacity().saturating_mul(2) - v.len()))
}

fn needs_to_grow<T>(v: &Vec<T>, len: usize) -> bool {
    v.len().checked_add(len).map_or(true, |needed| needed > v.capacity())
}

#[cfg(not(any(feature = "unstable", feature = "rust_1_57")))]
fn vec_try_reserve<T>(v: &mut Vec<T>, additional: usize) -> Result<(), TryReserveError> {
    let available = v.capacity().checked_sub(v.len()).expect("capacity >= len");
    if additional > available {
        let increase = additional
            .checked_sub(available)
            .expect("additional > available");
        let new_cap = v
            .capacity()
            .checked_add(increase)
            .ok_or(TryReserveError::CapacityOverflow)?;
        vec_try_extend(v, new_cap)?;
        debug_assert!(v.capacity() == new_cap);
    }

    Ok(())
}

#[cfg(not(any(feature = "unstable", feature = "rust_1_57")))]
fn vec_try_extend<T>(v: &mut Vec<T>, new_cap: usize) -> Result<(), TryReserveError> {
    let old_len = v.len();
    let old_cap: usize = v.capacity();

    if old_cap >= new_cap {
        return Ok(());
    }

    let elem_size = core::mem::size_of::<T>();
    let new_alloc_size = new_cap
        .checked_mul(elem_size)
        .ok_or(TryReserveError::CapacityOverflow)?;

    // required for alloc safety
    // See https://doc.rust-lang.org/stable/std/alloc/trait.GlobalAlloc.html#safety-1
    // Should be unreachable given prior `old_cap >= new_cap` check.
    assert!(new_alloc_size > 0);

    let align = core::mem::align_of::<T>();

    let (new_ptr, layout) = {
        if old_cap == 0 {
            let layout = Layout::from_size_align(new_alloc_size, align).expect("Invalid layout");
            let new_ptr = unsafe { alloc(layout) };
            (new_ptr, layout)
        } else {
            let old_alloc_size = old_cap
                .checked_mul(elem_size)
                .ok_or(TryReserveError::CapacityOverflow)?;
            let layout = Layout::from_size_align(old_alloc_size, align).expect("Invalid layout");
            let new_ptr = unsafe { realloc(v.as_mut_ptr() as *mut u8, layout, new_alloc_size) };
            (new_ptr, layout)
        }
    };

    if new_ptr.is_null() {
        return Err(TryReserveError::AllocError { layout });
    }

    let new_vec = unsafe { Vec::from_raw_parts(new_ptr.cast(), old_len, new_cap) };

    core::mem::forget(core::mem::replace(v, new_vec));
    Ok(())
}

impl<T> FallibleVec<T> for Vec<T> {
    #[inline(always)]
    fn try_reserve(&mut self, additional: usize) -> Result<(), TryReserveError> {
        #[cfg(all(feature = "unstable", not(feature = "rust_1_57")))]
        {
            self.try_reserve(additional)
        }

        #[cfg(not(feature = "rust_1_57"))]
        {
            vec_try_reserve(self, additional)
        }

        #[cfg(feature = "rust_1_57")]
        {
            // TryReserveError is an opaque type in 1.57
            self.try_reserve(additional).map_err(|_| {
                crate::make_try_reserve_error(self.len(), additional, core::mem::size_of::<T>(), core::mem::align_of::<T>())
            })
        }
    }

    #[inline]
    fn try_push(&mut self, elem: T) -> Result<(), TryReserveError> {
        if self.len() == self.capacity() {
            vec_try_reserve_for_growth(self, 1)?;
        }
        Ok(self.push(elem))
    }

    #[inline]
    fn try_push_give_back(&mut self, elem: T) -> Result<(), (T, TryReserveError)> {
        if self.len() == self.capacity() {
            if let Err(e) = vec_try_reserve_for_growth(self, 1) {
                return Err((elem, e));
            }
        }
        Ok(self.push(elem))
    }

    #[inline]
    fn try_with_capacity(capacity: usize) -> Result<Self, TryReserveError>
    where
        Self: core::marker::Sized,
    {
        let mut n = Self::new();
        FallibleVec::try_reserve(&mut n, capacity)?;
        Ok(n)
    }

    #[inline]
    fn try_insert(&mut self, index: usize, element: T) -> Result<(), (T, TryReserveError)> {
        if self.len() == self.capacity() {
            if let Err(e) = vec_try_reserve_for_growth(self, 1) {
                return Err((element, e));
            }
        }
        Ok(self.insert(index, element))
    }
    #[inline]
    fn try_append(&mut self, other: &mut Self) -> Result<(), TryReserveError> {
        FallibleVec::try_reserve(self, other.len())?;
        Ok(self.append(other))
    }
    fn try_resize(&mut self, new_len: usize, value: T) -> Result<(), TryReserveError>
    where
        T: Copy + Clone,
    {
        let len = self.len();
        if new_len > len {
            FallibleVec::try_reserve(self, new_len - len)?;
        }
        Ok(self.resize(new_len, value))
    }
    fn try_resize_with<F>(&mut self, new_len: usize, f: F) -> Result<(), TryReserveError>
    where
        F: FnMut() -> T,
    {
        let len = self.len();
        if new_len > len {
            FallibleVec::try_reserve(self, new_len - len)?;
        }
        Ok(self.resize_with(new_len, f))
    }
    fn try_resize_no_copy(&mut self, new_len: usize, value: T) -> Result<(), TryReserveError>
    where
        T: TryClone,
    {
        let len = self.len();

        if new_len > len {
            self.try_extend_with(new_len - len, TryExtendElement(value))
        } else {
            Ok(self.truncate(new_len))
        }
    }
    #[inline]
    fn try_extend_from_slice(&mut self, other: &[T]) -> Result<(), TryReserveError>
    where
        T: Clone,
    {
        if needs_to_grow(self, other.len()) {
            vec_try_reserve_for_growth(self, other.len())?;
        }
        Ok(self.extend_from_slice(other))
    }
    fn try_extend_from_slice_no_copy(&mut self, other: &[T]) -> Result<(), TryReserveError>
    where
        T: TryClone,
    {
        if needs_to_grow(self, other.len()) {
            vec_try_reserve_for_growth(self, other.len())?;
        }
        let mut len = self.len();
        let mut iterator = other.iter();
        while let Some(element) = iterator.next() {
            unsafe {
                core::ptr::write(self.get_unchecked_mut(len), element.try_clone()?);
                // NB can't overflow since we would have had to alloc the address space
                len += 1;
                self.set_len(len);
            }
        }
        Ok(())
    }
}

trait ExtendWith<T> {
    fn next(&mut self) -> Result<T, TryReserveError>;
    fn last(self) -> T;
}

struct TryExtendElement<T: TryClone>(T);
impl<T: TryClone> ExtendWith<T> for TryExtendElement<T> {
    #[inline(always)]
    fn next(&mut self) -> Result<T, TryReserveError> {
        self.0.try_clone()
    }
    #[inline(always)]
    fn last(self) -> T {
        self.0
    }
}

trait TryExtend<T> {
    fn try_extend_with<E: ExtendWith<T>>(
        &mut self,
        n: usize,
        value: E,
    ) -> Result<(), TryReserveError>;
}

impl<T> TryExtend<T> for Vec<T> {
    /// Extend the vector by `n` values, using the given generator.
    fn try_extend_with<E: ExtendWith<T>>(
        &mut self,
        n: usize,
        mut value: E,
    ) -> Result<(), TryReserveError> {
        if needs_to_grow(self, n) {
            vec_try_reserve_for_growth(self, n)?;
        }

        unsafe {
            let mut ptr = self.as_mut_ptr().add(self.len());

            let mut local_len = self.len();
            // Write all elements except the last one
            for _ in 1..n {
                core::ptr::write(ptr, value.next()?);
                ptr = ptr.offset(1);
                // Increment the length in every step in case next() panics
                local_len += 1;
                self.set_len(local_len);
            }

            if n > 0 {
                // We can write the last element directly without cloning needlessly
                core::ptr::write(ptr, value.last());
                local_len += 1;
                self.set_len(local_len);
            }

            // len set by scope guard
        }
        Ok(())
    }
}

trait Truncate {
    fn truncate(&mut self, len: usize);
}

impl<T> Truncate for Vec<T> {
    fn truncate(&mut self, len: usize) {
        let current_len = self.len();
        unsafe {
            let mut ptr = self.as_mut_ptr().add(current_len);
            // Set the final length at the end, keeping in mind that
            // dropping an element might panic. Works around a missed
            // optimization, as seen in the following issue:
            // https://github.com/rust-lang/rust/issues/51802
            let mut local_len = self.len();

            // drop any extra elements
            for _ in len..current_len {
                ptr = ptr.offset(-1);
                core::ptr::drop_in_place(ptr);
                local_len -= 1;
                self.set_len(local_len);
            }
        }
    }
}

/// try creating a vec from an `elem` cloned `n` times, see std::from_elem
#[cfg(feature = "unstable")]
pub fn try_from_elem<T: TryClone>(elem: T, n: usize) -> Result<Vec<T>, TryReserveError> {
    <T as SpecFromElem>::try_from_elem(elem, n)
}

// Specialization trait used for Vec::from_elem
#[cfg(feature = "unstable")]
trait SpecFromElem: Sized {
    fn try_from_elem(elem: Self, n: usize) -> Result<Vec<Self>, TryReserveError>;
}

#[cfg(feature = "unstable")]
impl<T: TryClone> SpecFromElem for T {
    default fn try_from_elem(elem: Self, n: usize) -> Result<Vec<T>, TryReserveError> {
        let mut v = Vec::new();
        v.try_resize_no_copy(n, elem)?;
        Ok(v)
    }
}

#[cfg(feature = "unstable")]
impl SpecFromElem for u8 {
    #[inline]
    fn try_from_elem(elem: u8, n: usize) -> Result<Vec<u8>, TryReserveError> {
        unsafe {
            let mut v: Vec<u8> = FallibleVec::try_with_capacity(n)?;
            core::ptr::write_bytes(v.as_mut_ptr(), elem, n);
            v.set_len(n);
            Ok(v)
        }
    }
}

impl<T: TryClone> TryClone for Vec<T> {
    #[inline]
    fn try_clone(&self) -> Result<Self, TryReserveError>
    where
        Self: core::marker::Sized,
    {
        let mut v = Vec::new();
        v.try_extend_from_slice_no_copy(self)?;
        Ok(v)
    }
}

pub trait TryFromIterator<I>: Sized {
    fn try_from_iterator<T: IntoIterator<Item = I>>(iterator: T) -> Result<Self, TryReserveError>;
}

impl<I> TryFromIterator<I> for Vec<I> {
    fn try_from_iterator<T: IntoIterator<Item = I>>(iterator: T) -> Result<Self, TryReserveError>
    where
        T: IntoIterator<Item = I>,
    {
        let mut new = Self::new();
        for i in iterator {
            new.try_push(i)?;
        }
        Ok(new)
    }
}

pub trait TryCollect<I> {
    fn try_collect<C: TryFromIterator<I>>(self) -> Result<C, TryReserveError>;
}

impl<I, T> TryCollect<I> for T
where
    T: IntoIterator<Item = I>,
{
    #[inline(always)]
    fn try_collect<C: TryFromIterator<I>>(self) -> Result<C, TryReserveError> {
        C::try_from_iterator(self)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[cfg(feature = "unstable")]
    fn vec() {
        // let v: Vec<u8> = from_elem(1, 10);
        let v: Vec<Vec<u8>> = try_vec![try_vec![42; 10].unwrap(); 100].unwrap();
        println!("{:?}", v);
        let v2 = try_vec![0, 1, 2];
        println!("{:?}", v2);
        assert_eq!(2 + 2, 4);
    }

    #[test]
    fn try_clone_vec() {
        // let v: Vec<u8> = from_elem(1, 10);
        let v = vec![42; 100];
        assert_eq!(v.try_clone().unwrap(), v);
    }

    #[test]
    fn try_clone_oom() {
        let layout = Layout::new::<u8>();
        let v =
            unsafe { Vec::<u8>::from_raw_parts(alloc(layout), core::usize::MAX, core::usize::MAX) };
        assert!(v.try_clone().is_err());
    }

    #[test]
    fn tryvec_try_clone_oom() {
        let layout = Layout::new::<u8>();
        let inner =
            unsafe { Vec::<u8>::from_raw_parts(alloc(layout), core::usize::MAX, core::usize::MAX) };
        let tv = TryVec { inner };
        assert!(tv.try_clone().is_err());
    }

    // #[test]
    // fn try_out_of_mem() {
    //     let v = try_vec![42_u8; 1000000000];
    //     assert_eq!(v.try_clone().unwrap(), v);
    // }

    #[test]
    fn oom() {
        let mut vec: Vec<char> = Vec::new();
        match FallibleVec::try_reserve(&mut vec, core::usize::MAX) {
            Ok(_) => panic!("it should be OOM"),
            _ => (),
        }
    }

    #[test]
    fn tryvec_oom() {
        let mut vec: TryVec<char> = TryVec::new();
        match vec.reserve(core::usize::MAX) {
            Ok(_) => panic!("it should be OOM"),
            _ => (),
        }
    }

    #[test]
    fn try_reserve() {
        let mut vec: Vec<_> = vec![1];
        let additional_room = vec.capacity() - vec.len();
        let additional = additional_room + 1;
        let old_cap = vec.capacity();
        FallibleVec::try_reserve(&mut vec, additional).unwrap();
        assert!(vec.capacity() > old_cap);
    }

    #[test]
    fn tryvec_reserve() {
        let mut vec: TryVec<_> = vec![1].into();
        let old_cap = vec.inner.capacity();
        let new_cap = old_cap + 1;
        vec.reserve(new_cap).unwrap();
        assert!(vec.inner.capacity() >= new_cap);
    }

    #[test]
    fn try_reserve_idempotent() {
        let mut vec: Vec<_> = vec![1];
        let additional_room = vec.capacity() - vec.len();
        let additional = additional_room + 1;
        FallibleVec::try_reserve(&mut vec, additional).unwrap();
        let cap_after_reserve = vec.capacity();
        FallibleVec::try_reserve(&mut vec, additional).unwrap();
        assert_eq!(vec.capacity(), cap_after_reserve);
    }

    #[test]
    fn tryvec_reserve_idempotent() {
        let mut vec: TryVec<_> = vec![1].into();
        let old_cap = vec.inner.capacity();
        let new_cap = old_cap + 1;
        vec.reserve(new_cap).unwrap();
        let cap_after_reserve = vec.inner.capacity();
        vec.reserve(new_cap).unwrap();
        assert_eq!(cap_after_reserve, vec.inner.capacity());
    }

    #[test]
    fn capacity_overflow() {
        let mut vec: Vec<_> = vec![1];
        match FallibleVec::try_reserve(&mut vec, core::usize::MAX) {
            Ok(_) => panic!("capacity calculation should overflow"),
            _ => (),
        }
    }

    #[test]
    fn tryvec_capacity_overflow() {
        let mut vec: TryVec<_> = vec![1].into();
        match vec.reserve(core::usize::MAX) {
            Ok(_) => panic!("capacity calculation should overflow"),
            _ => (),
        }
    }

    #[test]
    fn extend_from_slice() {
        let mut vec: Vec<u8> = b"foo".as_ref().into();
        vec.shrink_to_fit();
        vec.reserve(5);
        assert_eq!(8, vec.capacity());
        vec.try_extend_from_slice(b"bar").unwrap();
        assert_eq!(vec, b"foobar".as_ref());
        vec.try_extend_from_slice(b"1").unwrap();
        assert_eq!(vec, b"foobar1".as_ref());
        assert_eq!(8, vec.capacity());
        vec.try_extend_from_slice(b"11").unwrap();
        assert_eq!(16, vec.capacity());
    }

    #[test]
    fn tryvec_extend_from_slice() {
        let mut vec: TryVec<u8> = b"foo".as_ref().try_into().unwrap();
        vec.extend_from_slice(b"bar").unwrap();
        assert_eq!(vec, b"foobar".as_ref());
    }

    #[test]
    #[cfg(not(any(feature = "unstable", feature = "rust_1_57")))]
    fn try_extend_zst() {
        let mut vec: Vec<()> = Vec::new();
        assert_eq!(vec.capacity(), core::usize::MAX);
        assert!(vec_try_extend(&mut vec, 10).is_ok());
        assert!(vec_try_extend(&mut vec, core::usize::MAX).is_ok());
    }

    #[test]
    fn try_reserve_zst() {
        let mut vec: Vec<()> = Vec::new();
        assert!(FallibleVec::try_reserve(&mut vec, core::usize::MAX).is_ok());
    }
}
