//! The underlying UnixString/UnixStr implementation: just a `Vec<u8>`/`[u8]`.

use crate::sys_common::bytestring::debug_fmt_bytestring;
#[cfg(feature = "alloc")]
use crate::sys_common::{AsInner, IntoInner};
use core::fmt;
use core::mem;
use core::str;

#[cfg(feature = "alloc")]
use alloc::borrow::Cow;
#[cfg(feature = "alloc")]
use alloc::boxed::Box;
#[cfg(feature = "alloc")]
use alloc::rc::Rc;
#[cfg(feature = "alloc")]
use alloc::string::String;
#[cfg(feature = "alloc")]
use alloc::sync::Arc;
#[cfg(feature = "alloc")]
use alloc::vec::Vec;

#[cfg(all(feature = "alloc", feature = "toowned_clone_into"))]
use alloc::borrow::ToOwned;

#[cfg(feature = "alloc")]
#[derive(Clone, Hash)]
pub(crate) struct Buf {
    pub inner: Vec<u8>,
}

// FIXME:
// `Buf::as_slice` current implementation relies
// on `Slice` being layout-compatible with `[u8]`.
// When attribute privacy is implemented, `Slice` should be annotated as `#[repr(transparent)]`.
// Anyway, `Slice` representation and layout are considered implementation detail, are
// not documented and must not be relied upon.
pub(crate) struct Slice {
    pub inner: [u8],
}

impl fmt::Debug for Slice {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        debug_fmt_bytestring(&self.inner, formatter)
    }
}

#[cfg(feature = "alloc")]
impl IntoInner<Vec<u8>> for Buf {
    fn into_inner(self) -> Vec<u8> {
        self.inner
    }
}

#[cfg(feature = "alloc")]
impl AsInner<[u8]> for Buf {
    fn as_inner(&self) -> &[u8] {
        &self.inner
    }
}

#[cfg(feature = "alloc")]
impl Buf {
    pub fn from_string(s: String) -> Self {
        Self {
            inner: s.into_bytes(),
        }
    }

    #[inline]
    pub fn with_capacity(capacity: usize) -> Self {
        Buf {
            inner: Vec::with_capacity(capacity),
        }
    }

    #[inline]
    pub fn clear(&mut self) {
        self.inner.clear()
    }

    #[inline]
    pub fn capacity(&self) -> usize {
        self.inner.capacity()
    }

    #[inline]
    pub fn reserve(&mut self, additional: usize) {
        self.inner.reserve(additional)
    }

    #[inline]
    pub fn reserve_exact(&mut self, additional: usize) {
        self.inner.reserve_exact(additional)
    }

    #[inline]
    pub fn shrink_to_fit(&mut self) {
        self.inner.shrink_to_fit()
    }

    #[inline]
    #[cfg(feature = "shrink_to")]
    pub fn shrink_to(&mut self, min_capacity: usize) {
        self.inner.shrink_to(min_capacity)
    }

    #[inline]
    pub fn as_slice(&self) -> &Slice {
        // Safety: Slice just wraps [u8],
        // and &*self.inner is &[u8], therefore
        // transmuting &[u8] to &Slice is safe.
        unsafe { mem::transmute(&*self.inner) }
    }

    #[inline]
    pub fn as_mut_slice(&mut self) -> &mut Slice {
        // Safety: Slice just wraps [u8],
        // and &mut *self.inner is &mut [u8], therefore
        // transmuting &mut [u8] to &mut Slice is safe.
        unsafe { mem::transmute(&mut *self.inner) }
    }

    pub fn into_string(self) -> Result<String, Self> {
        String::from_utf8(self.inner).map_err(|p| Self {
            inner: p.into_bytes(),
        })
    }

    pub fn push_slice(&mut self, s: &Slice) {
        self.inner.extend_from_slice(&s.inner)
    }

    #[inline]
    pub fn into_box(self) -> Box<Slice> {
        unsafe { mem::transmute(self.inner.into_boxed_slice()) }
    }

    #[inline]
    pub fn from_box(boxed: Box<Slice>) -> Self {
        let inner: Box<[u8]> = unsafe { mem::transmute(boxed) };
        Self {
            inner: inner.into_vec(),
        }
    }

    #[inline]
    pub fn into_arc(&self) -> Arc<Slice> {
        self.as_slice().into_arc()
    }

    #[inline]
    pub fn into_rc(&self) -> Rc<Slice> {
        self.as_slice().into_rc()
    }
}

impl Slice {
    #[inline]
    fn from_u8_slice(s: &[u8]) -> &Self {
        unsafe { mem::transmute(s) }
    }

    #[inline]
    pub fn from_str(s: &str) -> &Self {
        Self::from_u8_slice(s.as_bytes())
    }

    pub fn to_str(&self) -> Option<&str> {
        str::from_utf8(&self.inner).ok()
    }

    #[cfg(feature = "alloc")]
    pub fn to_string_lossy(&self) -> Cow<'_, str> {
        String::from_utf8_lossy(&self.inner)
    }

    #[cfg(feature = "alloc")]
    pub fn to_owned(&self) -> Buf {
        Buf {
            inner: self.inner.to_vec(),
        }
    }

    #[cfg(all(feature = "alloc", feature = "toowned_clone_into"))]
    pub fn clone_into(&self, buf: &mut Buf) {
        self.inner.clone_into(&mut buf.inner)
    }

    #[inline]
    #[cfg(feature = "alloc")]
    pub fn into_box(&self) -> Box<Self> {
        let boxed: Box<[u8]> = self.inner.into();
        unsafe { mem::transmute(boxed) }
    }

    #[cfg(feature = "alloc")]
    pub fn empty_box() -> Box<Self> {
        let boxed: Box<[u8]> = Default::default();
        unsafe { mem::transmute(boxed) }
    }

    #[inline]
    #[cfg(feature = "alloc")]
    pub fn into_arc(&self) -> Arc<Self> {
        let arc: Arc<[u8]> = Arc::from(&self.inner);
        unsafe { Arc::from_raw(Arc::into_raw(arc) as *const Self) }
    }

    #[inline]
    #[cfg(feature = "alloc")]
    pub fn into_rc(&self) -> Rc<Self> {
        let rc: Rc<[u8]> = Rc::from(&self.inner);
        unsafe { Rc::from_raw(Rc::into_raw(rc) as *const Self) }
    }

    #[inline]
    #[cfg(feature = "unixstring_ascii")]
    pub fn make_ascii_lowercase(&mut self) {
        self.inner.make_ascii_lowercase()
    }

    #[inline]
    #[cfg(feature = "unixstring_ascii")]
    pub fn make_ascii_uppercase(&mut self) {
        self.inner.make_ascii_uppercase()
    }

    #[inline]
    #[cfg(all(feature = "alloc", feature = "unixstring_ascii"))]
    pub fn to_ascii_lowercase(&self) -> Buf {
        Buf {
            inner: self.inner.to_ascii_lowercase(),
        }
    }

    #[inline]
    #[cfg(all(feature = "alloc", feature = "unixstring_ascii"))]
    pub fn to_ascii_uppercase(&self) -> Buf {
        Buf {
            inner: self.inner.to_ascii_uppercase(),
        }
    }

    #[inline]
    #[cfg(feature = "unixstring_ascii")]
    pub fn is_ascii(&self) -> bool {
        self.inner.is_ascii()
    }

    #[inline]
    #[cfg(feature = "unixstring_ascii")]
    pub fn eq_ignore_ascii_case(&self, other: &Self) -> bool {
        self.inner.eq_ignore_ascii_case(&other.inner)
    }
}
