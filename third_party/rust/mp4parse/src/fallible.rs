//! The types in this module are thin wrappers around the stdlib types to add
//! support for fallible allocation. Though the fallible allocator is only
//! enabled with the mp4parse_fallible feature, the API differences from the
//! stdlib types ensure that all operations which allocate return a Result.
//! For the most part, this simply means adding a Result return value to
//! functions which return nothing or a non-Result value. However, these types
//! implement some traits whose API cannot communicate failure, but which do
//! require allocation, so it is important that these wrapper types do not
//! implement these traits.
//!
//! Specifically, do not implement any of the following traits:
//! - Clone
//! - Extend
//! - From
//! - FromIterator
//!
//! This list may not be exhaustive. Exercise caution when implementing
//! any new traits to ensure they won't potentially allocate in a way that
//! can't return a Result to indicate allocation failure.

#[cfg(feature = "mp4parse_fallible")]
extern "C" {
    fn malloc(bytes: usize) -> *mut u8;
    fn realloc(ptr: *mut u8, bytes: usize) -> *mut u8;
}

use std::convert::TryInto as _;
use std::hash::Hash;
use std::io::Read;
use std::io::Take;
use std::iter::IntoIterator;
use BMFFBox;

type Result<T> = std::result::Result<T, super::Error>;

pub trait TryRead {
    fn try_read_to_end(&mut self, buf: &mut TryVec<u8>) -> Result<usize>;

    fn read_into_try_vec(&mut self) -> Result<TryVec<u8>> {
        let mut buf = TryVec::new();
        let _ = self.try_read_to_end(&mut buf)?;
        Ok(buf)
    }
}

impl<'a, T: Read> TryRead for BMFFBox<'a, T> {
    fn try_read_to_end(&mut self, buf: &mut TryVec<u8>) -> Result<usize> {
        try_read_up_to(self, self.bytes_left(), buf)
    }
}

impl<T: Read> TryRead for Take<T> {
    /// With the `mp4parse_fallible` feature enabled, this function reserves the
    /// upper limit of what `src` can generate before reading all bytes until EOF
    /// in this source, placing them into buf. If the allocation is unsuccessful,
    /// or reading from the source generates an error before reaching EOF, this
    /// will return an error. Otherwise, it will return the number of bytes read.
    ///
    /// Since `Take::limit()` may return a value greater than the number of bytes
    /// which can be read from the source, it's possible this function may fail
    /// in the allocation phase even though allocating the number of bytes available
    /// to read would have succeeded. In general, it is assumed that the callers
    /// have accurate knowledge of the number of bytes of interest and have created
    /// `src` accordingly.
    ///
    /// With the `mp4parse_fallible` feature disabled, this is essentially a wrapper
    /// around `std::io::Read::read_to_end()`.
    fn try_read_to_end(&mut self, buf: &mut TryVec<u8>) -> Result<usize> {
        try_read_up_to(self, self.limit(), buf)
    }
}

fn try_read_up_to<R: Read>(src: &mut R, limit: u64, buf: &mut TryVec<u8>) -> Result<usize> {
    let additional = limit.try_into()?;
    buf.reserve(additional)?;
    let bytes_read = src.read_to_end(&mut buf.inner)?;
    Ok(bytes_read)
}

/// TryBox is a thin wrapper around std::boxed::Box to provide support for
/// fallible allocation.
///
/// See the `fallible` module documentation for more.
pub struct TryBox<T> {
    inner: std::boxed::Box<T>,
}

impl<T> TryBox<T> {
    pub fn try_new(x: T) -> Result<Self> {
        let inner;

        #[cfg(feature = "mp4parse_fallible")]
        {
            let size = std::mem::size_of::<T>();
            let new_ptr = unsafe { malloc(size) as *mut T };

            if new_ptr.is_null() {
                return Err(super::Error::OutOfMemory);
            }

            // If we did a simple assignment: *new_ptr = x, then the value
            // pointed to by new_ptr would immediately be dropped, but
            // that would cause an invalid memory access. Instead, we use
            // replace() to avoid the immediate drop and forget() to
            // ensure that drop never happens.
            inner = unsafe {
                std::mem::forget(std::mem::replace(&mut *new_ptr, x));
                Box::from_raw(new_ptr)
            };
        }
        #[cfg(not(feature = "mp4parse_fallible"))]
        {
            inner = Box::new(x);
        }

        Ok(Self { inner })
    }

    pub fn into_raw(b: TryBox<T>) -> *mut T {
        Box::into_raw(b.inner)
    }

    /// # Safety
    ///
    /// See std::boxed::from_raw
    pub unsafe fn from_raw(raw: *mut T) -> Self {
        Self {
            inner: Box::from_raw(raw),
        }
    }
}

/// TryHashMap is a thin wrapper around the hashbrown HashMap to provide
/// support for fallible allocation. This is the same library that stdlib
/// currently uses, but we use it directly since it provides try_reserve().
///
/// See the `fallible` module documentation for more.
#[derive(Default)]
pub struct TryHashMap<K, V> {
    inner: hashbrown::hash_map::HashMap<K, V>,
}

impl<K, V> TryHashMap<K, V>
where
    K: Eq + Hash,
{
    pub fn get<Q: ?Sized>(&self, k: &Q) -> Option<&V>
    where
        K: std::borrow::Borrow<Q>,
        Q: Hash + Eq,
    {
        self.inner.get(k)
    }

    pub fn insert(&mut self, k: K, v: V) -> Result<Option<V>> {
        self.reserve(if self.inner.capacity() == 0 { 4 } else { 1 })?;
        Ok(self.inner.insert(k, v))
    }

    fn reserve(&mut self, additional: usize) -> Result<()> {
        #[cfg(feature = "mp4parse_fallible")]
        {
            self.inner
                .try_reserve(additional)
                .map_err(|_| super::Error::OutOfMemory)
        }
        #[cfg(not(feature = "mp4parse_fallible"))]
        {
            self.inner.reserve(additional);
            Ok(())
        }
    }
}

#[test]
#[cfg(feature = "mp4parse_fallible")]
fn tryhashmap_oom() {
    match TryHashMap::<char, char>::default().reserve(std::usize::MAX) {
        Ok(_) => panic!("it should be OOM"),
        _ => (),
    }
}

#[derive(Default, PartialEq)]
pub struct TryVec<T> {
    inner: std::vec::Vec<T>,
}

impl<T: std::fmt::Debug> std::fmt::Debug for TryVec<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{:?}", self.inner)
    }
}

/// TryVec is a thin wrapper around std::vec::Vec to provide support for
/// fallible allocation.
///
/// See the `fallible` module documentation for more.
impl<T> TryVec<T> {
    pub fn new() -> Self {
        Self { inner: Vec::new() }
    }

    pub fn with_capacity(capacity: usize) -> Result<Self> {
        let mut v = Self::new();
        v.reserve(capacity)?;
        Ok(v)
    }

    pub fn append(&mut self, other: &mut Self) -> Result<()> {
        self.reserve(other.inner.len())?;
        self.inner.append(&mut other.inner);
        Ok(())
    }

    pub fn as_mut_slice(&mut self) -> &mut [T] {
        self
    }

    pub fn as_slice(&self) -> &[T] {
        self
    }

    pub fn clear(&mut self) {
        self.inner.clear()
    }

    #[cfg(test)]
    pub fn into_inner(self) -> Vec<T> {
        self.inner
    }

    pub fn is_empty(&self) -> bool {
        self.inner.is_empty()
    }

    pub fn iter_mut(&mut self) -> IterMut<T> {
        IterMut {
            inner: self.inner.iter_mut(),
        }
    }

    pub fn iter(&self) -> Iter<T> {
        Iter {
            inner: self.inner.iter(),
        }
    }

    pub fn pop(&mut self) -> Option<T> {
        self.inner.pop()
    }

    pub fn push(&mut self, value: T) -> Result<()> {
        self.reserve(if self.inner.capacity() == 0 { 4 } else { 1 })?;
        self.inner.push(value);
        Ok(())
    }

    fn reserve(&mut self, additional: usize) -> Result<()> {
        #[cfg(feature = "mp4parse_fallible")]
        {
            let available = self
                .inner
                .capacity()
                .checked_sub(self.inner.len())
                .expect("capacity >= len");
            if additional > available {
                let increase = additional
                    .checked_sub(available)
                    .expect("additional > available");
                let new_cap = self
                    .inner
                    .capacity()
                    .checked_add(increase)
                    .ok_or(super::Error::OutOfMemory)?;
                self.try_extend(new_cap)?;
                debug_assert!(self.inner.capacity() == new_cap);
            }
            Ok(())
        }
        #[cfg(not(feature = "mp4parse_fallible"))]
        {
            self.inner.reserve(additional);
            Ok(())
        }
    }

    pub fn resize_with<F>(&mut self, new_len: usize, f: F) -> Result<()>
    where
        F: FnMut() -> T,
    {
        self.reserve(new_len)?;
        self.inner.resize_with(new_len, f);
        Ok(())
    }

    #[cfg(feature = "mp4parse_fallible")]
    fn try_extend(&mut self, new_cap: usize) -> Result<()> {
        let old_ptr = self.as_mut_ptr();
        let old_len = self.inner.len();

        let old_cap: usize = self.inner.capacity();

        if old_cap >= new_cap {
            return Ok(());
        }

        let new_size_bytes = new_cap
            .checked_mul(std::mem::size_of::<T>())
            .ok_or(super::Error::OutOfMemory)?;

        let new_ptr = unsafe {
            if old_cap == 0 {
                malloc(new_size_bytes)
            } else {
                realloc(old_ptr as *mut u8, new_size_bytes)
            }
        };

        if new_ptr.is_null() {
            return Err(super::Error::OutOfMemory);
        }

        let new_vec = unsafe { Vec::from_raw_parts(new_ptr as *mut T, old_len, new_cap) };

        std::mem::forget(std::mem::replace(&mut self.inner, new_vec));
        Ok(())
    }
}

impl<T: Clone> TryVec<TryVec<T>> {
    pub fn concat(&self) -> Result<TryVec<T>> {
        let size = self.iter().map(|v| v.inner.len()).sum();
        let mut result = TryVec::with_capacity(size)?;
        for v in self.iter() {
            result.extend_from_slice(&v.inner)?;
        }
        Ok(result)
    }
}

impl<T: Clone> TryVec<T> {
    pub fn extend_from_slice(&mut self, other: &[T]) -> Result<()> {
        self.reserve(other.len())?;
        self.inner.extend_from_slice(other);
        Ok(())
    }
}

#[test]
#[cfg(feature = "mp4parse_fallible")]
fn oom() {
    let mut vec: TryVec<char> = TryVec::new();
    match vec.reserve(std::usize::MAX) {
        Ok(_) => panic!("it should be OOM"),
        _ => (),
    }
}

#[test]
fn try_reserve() {
    let mut vec: TryVec<_> = vec![1].into();
    let old_cap = vec.inner.capacity();
    let new_cap = old_cap + 1;
    vec.reserve(new_cap).unwrap();
    assert!(vec.inner.capacity() >= new_cap);
}

#[test]
fn try_reserve_idempotent() {
    let mut vec: TryVec<_> = vec![1].into();
    let old_cap = vec.inner.capacity();
    let new_cap = old_cap + 1;
    vec.reserve(new_cap).unwrap();
    let cap_after_reserve = vec.inner.capacity();
    vec.reserve(new_cap).unwrap();
    assert_eq!(cap_after_reserve, vec.inner.capacity());
}

#[test]
#[cfg(feature = "mp4parse_fallible")]
fn capacity_overflow() {
    let mut vec: TryVec<_> = vec![1].into();
    match vec.reserve(std::usize::MAX) {
        Ok(_) => panic!("capacity calculation should overflow"),
        _ => (),
    }
}

#[test]
fn extend_from_slice() {
    let mut vec: TryVec<u8> = b"foo".as_ref().try_into().unwrap();
    vec.extend_from_slice(b"bar").unwrap();
    assert_eq!(vec, b"foobar".as_ref());
}

impl<T> IntoIterator for TryVec<T> {
    type Item = T;
    type IntoIter = std::vec::IntoIter<T>;

    fn into_iter(self) -> Self::IntoIter {
        self.inner.into_iter()
    }
}

impl<'a, T> IntoIterator for &'a TryVec<T> {
    type Item = &'a T;
    type IntoIter = std::slice::Iter<'a, T>;

    fn into_iter(self) -> Self::IntoIter {
        self.inner.iter()
    }
}

impl std::io::Write for TryVec<u8> {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.extend_from_slice(buf)?;
        Ok(buf.len())
    }

    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

impl<T: PartialEq> PartialEq<Vec<T>> for TryVec<T> {
    fn eq(&self, other: &Vec<T>) -> bool {
        self.inner.eq(other)
    }
}

impl<'a, T: PartialEq> PartialEq<&'a [T]> for TryVec<T> {
    fn eq(&self, other: &&[T]) -> bool {
        self.inner.eq(other)
    }
}

impl PartialEq<&str> for TryVec<u8> {
    fn eq(&self, other: &&str) -> bool {
        self.as_slice() == other.as_bytes()
    }
}

impl std::convert::AsRef<[u8]> for TryVec<u8> {
    fn as_ref(&self) -> &[u8] {
        self.inner.as_ref()
    }
}

impl<T> std::convert::From<Vec<T>> for TryVec<T> {
    fn from(value: Vec<T>) -> Self {
        Self { inner: value }
    }
}

impl<T: Clone> std::convert::TryFrom<&[T]> for TryVec<T> {
    type Error = super::Error;

    fn try_from(value: &[T]) -> Result<Self> {
        let mut v = Self::new();
        v.extend_from_slice(value)?;
        Ok(v)
    }
}

impl std::convert::TryFrom<&str> for TryVec<u8> {
    type Error = super::Error;

    fn try_from(value: &str) -> Result<Self> {
        let mut v = Self::new();
        v.extend_from_slice(value.as_bytes())?;
        Ok(v)
    }
}

impl<T> std::ops::Deref for TryVec<T> {
    type Target = [T];

    fn deref(&self) -> &[T] {
        self.inner.deref()
    }
}

impl<T> std::ops::DerefMut for TryVec<T> {
    fn deref_mut(&mut self) -> &mut [T] {
        self.inner.deref_mut()
    }
}

pub struct Iter<'a, T> {
    inner: std::slice::Iter<'a, T>,
}

impl<'a, T> Iterator for Iter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next()
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.inner.size_hint()
    }
}

pub struct IterMut<'a, T> {
    inner: std::slice::IterMut<'a, T>,
}

impl<'a, T> Iterator for IterMut<'a, T> {
    type Item = &'a mut T;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next()
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.inner.size_hint()
    }
}
