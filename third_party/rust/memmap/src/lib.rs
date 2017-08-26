//! A cross-platform Rust API for memory maps.

#![deny(warnings)]

#[cfg(windows)]
mod windows;
#[cfg(windows)]
use windows::MmapInner;

#[cfg(unix)]
mod unix;
#[cfg(unix)]
use unix::MmapInner;

use std::cell::UnsafeCell;
use std::fmt;
use std::fs::{self, File};
use std::io::{Error, ErrorKind, Result};
use std::path::Path;
use std::rc::Rc;
use std::slice;
use std::sync::Arc;
use std::usize;

/// Memory map protection.
///
/// Determines how a memory map may be used. If the memory map is backed by a
/// file, then the file must have permissions corresponding to the operations
/// the protection level allows.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Protection {

    /// A read-only memory map. Writes to the memory map will result in a panic.
    Read,

    /// A read-write memory map. Writes to the memory map will be reflected in
    /// the file after a call to `Mmap::flush` or after the `Mmap` is dropped.
    ReadWrite,

    /// A read, copy-on-write memory map. Writes to the memory map will not be
    /// carried through to the underlying file. It is unspecified whether
    /// changes made to the file after the memory map is created will be
    /// visible.
    ReadCopy,

    /// A readable and executable mapping.
    ReadExecute,
}

impl Protection {

    fn as_open_options(self) -> fs::OpenOptions {
        let mut options = fs::OpenOptions::new();
        options.read(true)
               .write(self.write());

        options
    }

    /// Returns `true` if the `Protection` is writable.
    pub fn write(self) -> bool {
        match self {
            Protection::ReadWrite | Protection::ReadCopy => true,
            _ => false,
        }
    }
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct MmapOptions {
    /// Indicates that the memory map should be suitable for a stack.
    ///
    /// This option should only be used with anonymous memory maps.
    pub stack: bool,
}

/// A memory-mapped buffer.
///
/// A file-backed `Mmap` buffer may be used to read or write data to a file. Use
/// `Mmap::open(..)` to create a file-backed memory map. An anonymous `Mmap`
/// buffer may be used any place that an in-memory byte buffer is needed, and
/// gives the added features of a memory map. Use `Mmap::anonymous(..)` to
/// create an anonymous memory map.
///
/// Changes written to a memory-mapped file are not guaranteed to be durable
/// until the memory map is flushed, or it is dropped.
///
/// ```
/// use std::io::Write;
/// use memmap::{Mmap, Protection};
///
/// let file_mmap = Mmap::open_path("README.md", Protection::Read).unwrap();
/// let bytes: &[u8] = unsafe { file_mmap.as_slice() };
/// assert_eq!(b"# memmap", &bytes[0..8]);
///
/// let mut anon_mmap = Mmap::anonymous(4096, Protection::ReadWrite).unwrap();
/// unsafe { anon_mmap.as_mut_slice() }.write(b"foo").unwrap();
/// assert_eq!(b"foo\0\0", unsafe { &anon_mmap.as_slice()[0..5] });
/// ```
pub struct Mmap {
    inner: MmapInner
}

impl Mmap {

    /// Opens a file-backed memory map.
    ///
    /// The file must be opened with read permissions, and write permissions if
    /// the supplied protection is `ReadWrite`. The file must not be empty.
    pub fn open(file: &File, prot: Protection) -> Result<Mmap> {
        let len = try!(file.metadata()).len();
        if len > usize::MAX as u64 {
            return Err(Error::new(ErrorKind::InvalidData,
                                  "file length overflows usize"));
        }
        MmapInner::open(file, prot, 0, len as usize).map(|inner| Mmap { inner: inner })
    }

    /// Opens a file-backed memory map.
    ///
    /// The file must not be empty.
    pub fn open_path<P>(path: P, prot: Protection) -> Result<Mmap>
    where P: AsRef<Path> {
        let file = try!(prot.as_open_options().open(path));
        let len = try!(file.metadata()).len();
        if len > usize::MAX as u64 {
            return Err(Error::new(ErrorKind::InvalidData,
                                  "file length overflows usize"));
        }
        MmapInner::open(&file, prot, 0, len as usize).map(|inner| Mmap { inner: inner })
    }

    /// Opens a file-backed memory map with the specified offset and length.
    ///
    /// The file must be opened with read permissions, and write permissions if
    /// the supplied protection is `ReadWrite`. The file must not be empty. The
    /// length must be greater than zero.
    pub fn open_with_offset(file: &File,
                            prot: Protection,
                            offset: usize,
                            len: usize) -> Result<Mmap> {
        MmapInner::open(file, prot, offset, len).map(|inner| Mmap { inner: inner })
    }

    /// Opens an anonymous memory map.
    ///
    /// The length must be greater than zero.
    pub fn anonymous(len: usize, prot: Protection) -> Result<Mmap> {
        Mmap::anonymous_with_options(len, prot, Default::default())
    }

    /// Opens an anonymous memory map with the provided options.
    ///
    /// The length must be greater than zero.
    pub fn anonymous_with_options(len: usize,
                                  prot: Protection,
                                  options: MmapOptions) -> Result<Mmap> {
        MmapInner::anonymous(len, prot, options).map(|inner| Mmap { inner: inner })
    }

    /// Flushes outstanding memory map modifications to disk.
    ///
    /// When this returns with a non-error result, all outstanding changes to a
    /// file-backed memory map are guaranteed to be durably stored. The file's
    /// metadata (including last modification timestamp) may not be updated.
    pub fn flush(&self) -> Result<()> {
        let len = self.len();
        self.inner.flush(0, len)
    }

    /// Asynchronously flushes outstanding memory map modifications to disk.
    ///
    /// This method initiates flushing modified pages to durable storage, but it
    /// will not wait for the operation to complete before returning. The file's
    /// metadata (including last modification timestamp) may not be updated.
    pub fn flush_async(&self) -> Result<()> {
        let len = self.len();
        self.inner.flush_async(0, len)
    }

    /// Flushes outstanding memory map modifications in the range to disk.
    ///
    /// The offset and length must be in the bounds of the mmap.
    ///
    /// When this returns with a non-error result, all outstanding changes to a
    /// file-backed memory in the range are guaranteed to be durable stored. The
    /// file's metadata (including last modification timestamp) may not be
    /// updated. It is not guaranteed the only the changes in the specified
    /// range are flushed; other outstanding changes to the mmap may be flushed
    /// as well.
    pub fn flush_range(&self, offset: usize, len: usize) -> Result<()> {
        self.inner.flush(offset, len)
    }

    /// Asynchronously flushes outstanding memory map modifications in the range
    /// to disk.
    ///
    /// The offset and length must be in the bounds of the mmap.
    ///
    /// This method initiates flushing modified pages to durable storage, but it
    /// will not wait for the operation to complete before returning. The file's
    /// metadata (including last modification timestamp) may not be updated. It
    /// is not guaranteed that the only changes flushed are those in the
    /// specified range; other outstanding changes to the mmap may be flushed as
    /// well.
    pub fn flush_async_range(&self, offset: usize, len: usize) -> Result<()> {
        self.inner.flush_async(offset, len)
    }

    /// Change the `Protection` this mapping was created with.
    ///
    /// If you create a read-only file-backed mapping, you can **not** use this method to make the
    /// mapping writeable. Remap the file instead.
    pub fn set_protection(&mut self, prot: Protection) -> Result<()> {
        self.inner.set_protection(prot)
    }

    /// Returns the length of the memory map.
    pub fn len(&self) -> usize {
        self.inner.len()
    }

    /// Returns a pointer to the mapped memory.
    ///
    /// See `Mmap::as_slice` for invariants that must hold when dereferencing
    /// the pointer.
    pub fn ptr(&self) -> *const u8 {
        self.inner.ptr()
    }

    /// Returns a pointer to the mapped memory.
    ///
    /// See `Mmap::as_mut_slice` for invariants that must hold when
    /// dereferencing the pointer.
    pub fn mut_ptr(&mut self) -> *mut u8 {
        self.inner.mut_ptr()
    }

    /// Returns the memory mapped file as an immutable slice.
    ///
    /// ## Unsafety
    ///
    /// The caller must ensure that the file is not concurrently modified.
    pub unsafe fn as_slice(&self) -> &[u8] {
        slice::from_raw_parts(self.ptr(), self.len())
    }

    /// Returns the memory mapped file as a mutable slice.
    ///
    /// ## Unsafety
    ///
    /// The caller must ensure that the file is not concurrently accessed.
    pub unsafe fn as_mut_slice(&mut self) -> &mut [u8] {
        slice::from_raw_parts_mut(self.mut_ptr(), self.len())
    }

    /// Creates a splittable mmap view from the mmap.
    pub fn into_view(self) -> MmapView {
        let len = self.len();
        MmapView { inner: Rc::new(UnsafeCell::new(self)),
                   offset: 0,
                   len: len }
    }

    /// Creates a thread-safe splittable mmap view from the mmap.
    pub fn into_view_sync(self) -> MmapViewSync {
        let len = self.len();
        MmapViewSync { inner: Arc::new(UnsafeCell::new(self)),
                       offset: 0,
                       len: len }
    }
}

impl fmt::Debug for Mmap {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Mmap {{ ptr: {:?}, len: {} }}", self.ptr(), self.len())
    }
}

/// A view of a memory map.
///
/// The view may be split into disjoint ranges, each of which will share the
/// underlying memory map.
pub struct MmapView {
    inner: Rc<UnsafeCell<Mmap>>,
    offset: usize,
    len: usize,
}

impl MmapView {

    /// Split the view into disjoint pieces at the specified offset.
    ///
    /// The provided offset must be less than the view's length.
    pub fn split_at(self, offset: usize) -> Result<(MmapView, MmapView)> {
        if self.len < offset {
            return Err(Error::new(ErrorKind::InvalidInput,
                                  "mmap view split offset must be less than the view length"));
        }
        let MmapView { inner, offset: self_offset, len: self_len } = self;
        Ok((MmapView { inner: inner.clone(),
                       offset: self_offset,
                       len: offset },
            MmapView { inner: inner,
                       offset: self_offset + offset,
                       len: self_len - offset }))
    }

    /// Restricts the range of the view to the provided offset and length.
    ///
    /// The provided range must be a subset of the current range
    /// (`offset + len < view.len()`).
    pub fn restrict(&mut self, offset: usize, len: usize) -> Result<()> {
        if offset + len > self.len {
            return Err(Error::new(ErrorKind::InvalidInput,
                                  "mmap view may only be restricted to a subrange \
                                   of the current view"));
        }
        self.offset = self.offset + offset;
        self.len = len;
        Ok(())
    }

    /// Get a reference to the inner mmap.
    ///
    /// The caller must ensure that memory outside the `offset`/`len` range is
    /// not accessed.
    fn inner(&self) -> &Mmap {
        unsafe {
            &*self.inner.get()
        }
    }

    /// Get a mutable reference to the inner mmap.
    ///
    /// The caller must ensure that memory outside the `offset`/`len` range is
    /// not accessed.
    fn inner_mut(&self) -> &mut Mmap {
        unsafe {
            &mut *self.inner.get()
        }
    }

    /// Flushes outstanding view modifications to disk.
    ///
    /// When this returns with a non-error result, all outstanding changes to a
    /// file-backed memory map view are guaranteed to be durably stored. The
    /// file's metadata (including last modification timestamp) may not be
    /// updated.
    pub fn flush(&self) -> Result<()> {
        self.inner_mut().flush_range(self.offset, self.len)
    }

    /// Asynchronously flushes outstanding memory map view modifications to
    /// disk.
    ///
    /// This method initiates flushing modified pages to durable storage, but it
    /// will not wait for the operation to complete before returning. The file's
    /// metadata (including last modification timestamp) may not be updated.
    pub fn flush_async(&self) -> Result<()> {
        self.inner_mut().flush_async_range(self.offset, self.len)
    }

    /// Returns the length of the memory map view.
    pub fn len(&self) -> usize {
        self.len
    }

    /// Returns a shared pointer to the mapped memory.
    ///
    /// See `Mmap::as_slice` for invariants that must hold when dereferencing
    /// the pointer.
    pub fn ptr(&self) -> *const u8 {
        unsafe { self.inner().ptr().offset(self.offset as isize) }
    }

    /// Returns a mutable pointer to the mapped memory.
    ///
    /// See `Mmap::as_mut_slice` for invariants that must hold when
    /// dereferencing the pointer.
    pub fn mut_ptr(&mut self) -> *mut u8 {
        unsafe { self.inner_mut().mut_ptr().offset(self.offset as isize) }
    }

    /// Returns the memory mapped file as an immutable slice.
    ///
    /// ## Unsafety
    ///
    /// The caller must ensure that the file is not concurrently modified.
    pub unsafe fn as_slice(&self) -> &[u8] {
        &self.inner().as_slice()[self.offset..self.offset + self.len]
    }

    /// Returns the memory mapped file as a mutable slice.
    ///
    /// ## Unsafety
    ///
    /// The caller must ensure that the file is not concurrently accessed.
    pub unsafe fn as_mut_slice(&mut self) -> &mut [u8] {
        &mut self.inner_mut().as_mut_slice()[self.offset..self.offset + self.len]
    }

    /// Clones the view of the memory map.
    ///
    /// The underlying memory map is shared, and thus the caller must ensure that the memory
    /// underlying the view is not illegally aliased.
    pub unsafe fn clone(&self) -> MmapView {
        MmapView {
            inner: self.inner.clone(),
            offset: self.offset,
            len: self.len,
        }
    }
}

impl fmt::Debug for MmapView {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "MmapView {{ ptr: {:?}, offset: {}, len: {} }}",
               self.inner().ptr(), self.offset, self.len)
    }
}

/// A thread-safe view of a memory map.
///
/// The view may be split into disjoint ranges, each of which will share the
/// underlying memory map.
pub struct MmapViewSync {
    inner: Arc<UnsafeCell<Mmap>>,
    offset: usize,
    len: usize,
}

impl MmapViewSync {

    /// Split the view into disjoint pieces at the specified offset.
    ///
    /// The provided offset must be less than the view's length.
    pub fn split_at(self, offset: usize) -> Result<(MmapViewSync, MmapViewSync)> {
        if self.len < offset {
            return Err(Error::new(ErrorKind::InvalidInput,
                                      "mmap view split offset must be less than the view length"));
        }
        let MmapViewSync { inner, offset: self_offset, len: self_len } = self;
        Ok((MmapViewSync { inner: inner.clone(),
                           offset: self_offset,
                           len: offset },
            MmapViewSync { inner: inner,
                           offset: self_offset + offset,
                           len: self_len - offset }))
    }

    /// Restricts the range of this view to the provided offset and length.
    ///
    /// The provided range must be a subset of the current range (`offset + len < view.len()`).
    pub fn restrict(&mut self, offset: usize, len: usize) -> Result<()> {
        if offset + len > self.len {
            return Err(Error::new(ErrorKind::InvalidInput,
                                      "mmap view may only be restricted to a subrange \
                                       of the current view"));
        }
        self.offset = self.offset + offset;
        self.len = len;
        Ok(())
    }

    /// Get a reference to the inner mmap.
    ///
    /// The caller must ensure that memory outside the `offset`/`len` range is not accessed.
    fn inner(&self) -> &Mmap {
        unsafe {
            &*self.inner.get()
        }
    }

    /// Get a mutable reference to the inner mmap.
    ///
    /// The caller must ensure that memory outside the `offset`/`len` range is not accessed.
    fn inner_mut(&self) -> &mut Mmap {
        unsafe {
            &mut *self.inner.get()
        }
    }

    /// Flushes outstanding view modifications to disk.
    ///
    /// When this returns with a non-error result, all outstanding changes to a file-backed memory
    /// map view are guaranteed to be durably stored. The file's metadata (including last
    /// modification timestamp) may not be updated.
    pub fn flush(&self) -> Result<()> {
        self.inner_mut().flush_range(self.offset, self.len)
    }

    /// Asynchronously flushes outstanding memory map view modifications to disk.
    ///
    /// This method initiates flushing modified pages to durable storage, but it will not wait
    /// for the operation to complete before returning. The file's metadata (including last
    /// modification timestamp) may not be updated.
    pub fn flush_async(&self) -> Result<()> {
        self.inner_mut().flush_async_range(self.offset, self.len)
    }

    /// Returns the length of the memory map view.
    pub fn len(&self) -> usize {
        self.len
    }

    /// Returns a shared pointer to the mapped memory.
    ///
    /// See `Mmap::as_slice` for invariants that must hold when dereferencing the pointer.
    pub fn ptr(&self) -> *const u8 {
        unsafe { self.inner().ptr().offset(self.offset as isize) }
    }

    /// Returns a mutable pointer to the mapped memory.
    ///
    /// See `Mmap::as_mut_slice` for invariants that must hold when dereferencing the pointer.
    pub fn mut_ptr(&mut self) -> *mut u8 {
        unsafe { self.inner_mut().mut_ptr().offset(self.offset as isize) }
    }

    /// Returns the memory mapped file as an immutable slice.
    ///
    /// ## Unsafety
    ///
    /// The caller must ensure that the file is not concurrently modified.
    pub unsafe fn as_slice(&self) -> &[u8] {
        &self.inner().as_slice()[self.offset..self.offset + self.len]
    }

    /// Returns the memory mapped file as a mutable slice.
    ///
    /// ## Unsafety
    ///
    /// The caller must ensure that the file is not concurrently accessed.
    pub unsafe fn as_mut_slice(&mut self) -> &mut [u8] {
        &mut self.inner_mut().as_mut_slice()[self.offset..self.offset + self.len]
    }

    /// Clones the view of the memory map.
    ///
    /// The underlying memory map is shared, and thus the caller must ensure that the memory
    /// underlying the view is not illegally aliased.
    pub unsafe fn clone(&self) -> MmapViewSync {
        MmapViewSync {
            inner: self.inner.clone(),
            offset: self.offset,
            len: self.len,
        }
    }
}

impl fmt::Debug for MmapViewSync {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "MmapViewSync {{ ptr: {:?}, offset: {}, len: {} }}",
               self.inner().ptr(), self.offset, self.len)
    }
}

unsafe impl Sync for MmapViewSync {}
unsafe impl Send for MmapViewSync {}

#[cfg(test)]
mod test {
    extern crate tempdir;

    use std::{fs, iter};
    use std::io::{Read, Write};
    use std::thread;
    use std::sync::Arc;
    use std::ptr;

    use super::*;

    #[test]
    fn map_file() {
        let expected_len = 128;
        let tempdir = tempdir::TempDir::new("mmap").unwrap();
        let path = tempdir.path().join("mmap");

        fs::OpenOptions::new()
                        .write(true)
                        .create(true)
                        .open(&path).unwrap()
                        .set_len(expected_len as u64).unwrap();

        let mut mmap = Mmap::open_path(path, Protection::ReadWrite).unwrap();
        let len = mmap.len();
        assert_eq!(expected_len, len);

        let zeros = iter::repeat(0).take(len).collect::<Vec<_>>();
        let incr = (0..len).map(|n| n as u8).collect::<Vec<_>>();

        // check that the mmap is empty
        assert_eq!(&zeros[..], unsafe { mmap.as_slice() });

        // write values into the mmap
        unsafe { mmap.as_mut_slice() }.write_all(&incr[..]).unwrap();

        // read values back
        assert_eq!(&incr[..], unsafe { mmap.as_slice() });
    }

    /// Checks that a 0-length file will not be mapped.
    #[test]
    fn map_empty_file() {
        let tempdir = tempdir::TempDir::new("mmap").unwrap();
        let path = tempdir.path().join("mmap");

        fs::OpenOptions::new()
                        .write(true)
                        .create(true)
                        .open(&path).unwrap();

        assert!(Mmap::open_path(path, Protection::ReadWrite).is_err());
    }

    #[test]
    fn map_anon() {
        let expected_len = 128;
        let mut mmap = Mmap::anonymous(expected_len, Protection::ReadWrite).unwrap();
        let len = mmap.len();
        assert_eq!(expected_len, len);

        let zeros = iter::repeat(0).take(len).collect::<Vec<_>>();
        let incr = (0..len).map(|n| n as u8).collect::<Vec<_>>();

        // check that the mmap is empty
        assert_eq!(&zeros[..], unsafe { mmap.as_slice() });

        // write values into the mmap
        unsafe { mmap.as_mut_slice() }.write_all(&incr[..]).unwrap();

        // read values back
        assert_eq!(&incr[..], unsafe { mmap.as_slice() });
    }

    #[test]
    fn file_write() {
        let tempdir = tempdir::TempDir::new("mmap").unwrap();
        let path = tempdir.path().join("mmap");

        let mut file = fs::OpenOptions::new()
                                       .read(true)
                                       .write(true)
                                       .create(true)
                                       .open(&path).unwrap();
        file.set_len(128).unwrap();

        let write = b"abc123";
        let mut read = [0u8; 6];

        let mut mmap = Mmap::open_path(&path, Protection::ReadWrite).unwrap();
        unsafe { mmap.as_mut_slice() }.write_all(write).unwrap();
        mmap.flush().unwrap();

        file.read(&mut read).unwrap();
        assert_eq!(write, &read);
    }

    #[test]
    fn flush_range() {
        let tempdir = tempdir::TempDir::new("mmap").unwrap();
        let path = tempdir.path().join("mmap");

        let file = fs::OpenOptions::new()
                                   .read(true)
                                   .write(true)
                                   .create(true)
                                   .open(&path).unwrap();
        file.set_len(128).unwrap();
        let write = b"abc123";

        let mut mmap = Mmap::open_with_offset(&file, Protection::ReadWrite, 2, write.len()).unwrap();
        unsafe { mmap.as_mut_slice() }.write_all(write).unwrap();
        mmap.flush_range(0, write.len()).unwrap();
    }

    #[test]
    fn map_copy() {
        let tempdir = tempdir::TempDir::new("mmap").unwrap();
        let path = tempdir.path().join("mmap");

        let mut file = fs::OpenOptions::new()
                                       .read(true)
                                       .write(true)
                                       .create(true)
                                       .open(&path).unwrap();
        file.set_len(128).unwrap();

        let nulls = b"\0\0\0\0\0\0";
        let write = b"abc123";
        let mut read = [0u8; 6];

        let mut mmap = Mmap::open_path(&path, Protection::ReadCopy).unwrap();
        unsafe { mmap.as_mut_slice() }.write(write).unwrap();
        mmap.flush().unwrap();

        // The mmap contains the write
        unsafe { mmap.as_slice() }.read(&mut read).unwrap();
        assert_eq!(write, &read);

        // The file does not contain the write
        file.read(&mut read).unwrap();
        assert_eq!(nulls, &read);

        // another mmap does not contain the write
        let mmap2 = Mmap::open_path(&path, Protection::Read).unwrap();
        unsafe { mmap2.as_slice() }.read(&mut read).unwrap();
        assert_eq!(nulls, &read);
    }

    #[test]
    fn map_offset() {
        let tempdir = tempdir::TempDir::new("mmap").unwrap();
        let path = tempdir.path().join("mmap");

        let file = fs::OpenOptions::new()
                                   .read(true)
                                   .write(true)
                                   .create(true)
                                   .open(&path)
                                   .unwrap();

        file.set_len(500000 as u64).unwrap();

        let offset = 5099;
        let len = 50050;

        let mut mmap = Mmap::open_with_offset(&file,
                                              Protection::ReadWrite,
                                              offset,
                                              len).unwrap();
        assert_eq!(len, mmap.len());

        let zeros = iter::repeat(0).take(len).collect::<Vec<_>>();
        let incr = (0..len).map(|n| n as u8).collect::<Vec<_>>();

        // check that the mmap is empty
        assert_eq!(&zeros[..], unsafe { mmap.as_slice() });

        // write values into the mmap
        unsafe { mmap.as_mut_slice() }.write_all(&incr[..]).unwrap();

        // read values back
        assert_eq!(&incr[..], unsafe { mmap.as_slice() });
    }

    #[test]
    fn index() {
        let mut mmap = Mmap::anonymous(128, Protection::ReadWrite).unwrap();
        unsafe { mmap.as_mut_slice()[0] = 42 };
        assert_eq!(42, unsafe { mmap.as_slice()[0] });
    }

    #[test]
    fn sync_send() {
        let mmap = Arc::new(Mmap::anonymous(128, Protection::ReadWrite).unwrap());
        thread::spawn(move || {
            unsafe {
                mmap.as_slice();
            }
        });
    }

    #[test]
    fn view() {
        let len = 128;
        let split = 32;
        let mut view = Mmap::anonymous(len, Protection::ReadWrite).unwrap().into_view();
        let incr = (0..len).map(|n| n as u8).collect::<Vec<_>>();
        // write values into the view
        unsafe { view.as_mut_slice() }.write_all(&incr[..]).unwrap();

        let (mut view1, view2) = view.split_at(32).unwrap();
        assert_eq!(view1.len(), split);
        assert_eq!(view2.len(), len - split);

        assert_eq!(&incr[0..split], unsafe { view1.as_slice() });
        assert_eq!(&incr[split..], unsafe { view2.as_slice() });

        view1.restrict(10, 10).unwrap();
        assert_eq!(&incr[10..20], unsafe { view1.as_slice() })
    }

    #[test]
    fn view_sync() {
        let len = 128;
        let split = 32;
        let mut view = Mmap::anonymous(len, Protection::ReadWrite).unwrap().into_view_sync();
        let incr = (0..len).map(|n| n as u8).collect::<Vec<_>>();
        // write values into the view
        unsafe { view.as_mut_slice() }.write_all(&incr[..]).unwrap();

        let (mut view1, view2) = view.split_at(32).unwrap();
        assert_eq!(view1.len(), split);
        assert_eq!(view2.len(), len - split);

        assert_eq!(&incr[0..split], unsafe { view1.as_slice() });
        assert_eq!(&incr[split..], unsafe { view2.as_slice() });

        view1.restrict(10, 10).unwrap();
        assert_eq!(&incr[10..20], unsafe { view1.as_slice() })
    }

    #[test]
    fn view_write() {
        let len   = 131072; // 256KiB
        let split =  66560; // 65KiB + 10B

        let tempdir = tempdir::TempDir::new("mmap").unwrap();
        let path = tempdir.path().join("mmap");

        let mut file = fs::OpenOptions::new()
                                       .read(true)
                                       .write(true)
                                       .create(true)
                                       .open(&path).unwrap();
        file.set_len(len).unwrap();

        let incr = (0..len).map(|n| n as u8).collect::<Vec<_>>();
        let incr1 = incr[0..split].to_owned();
        let incr2 = incr[split..].to_owned();

        let (mut view1, mut view2) = Mmap::open_path(&path, Protection::ReadWrite)
                                         .unwrap()
                                         .into_view_sync()
                                         .split_at(split)
                                         .unwrap();


        let join1 = thread::spawn(move || {
            unsafe { view1.as_mut_slice() }.write(&incr1).unwrap();
            view1.flush().unwrap();
        });

        let join2 = thread::spawn(move || {
            unsafe { view2.as_mut_slice() }.write(&incr2).unwrap();
            view2.flush().unwrap();
        });

        join1.join().unwrap();
        join2.join().unwrap();

        let mut buf = Vec::new();
        file.read_to_end(&mut buf).unwrap();
        assert_eq!(incr, &buf[..]);
    }

    #[test]
    fn view_sync_send() {
        let view = Arc::new(Mmap::anonymous(128, Protection::ReadWrite).unwrap().into_view_sync());
        thread::spawn(move || {
            unsafe {
                view.as_slice();
            }
        });
    }

    #[test]
    fn set_prot() {
        let mut map = Mmap::anonymous(1, Protection::Read).unwrap();
        map.set_protection(Protection::ReadWrite).unwrap();

        // We should now be able to write to the memory. If not this will cause a SIGSEGV.
        unsafe { ptr::write(map.mut_ptr(), 0xf1); }

        map.set_protection(Protection::Read).unwrap();

        assert_eq!(unsafe { ptr::read(map.mut_ptr()) }, 0xf1);
    }

    #[test]
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    fn jit_x86() {
        use std::mem;

        let mut map = Mmap::anonymous(4096, Protection::ReadWrite).unwrap();

        {
            let mut jitmem = unsafe { map.as_mut_slice() };
            jitmem[0] = 0xB8;   // mov eax, 0xAB
            jitmem[1] = 0xAB;
            jitmem[2] = 0x00;
            jitmem[3] = 0x00;
            jitmem[4] = 0x00;
            jitmem[5] = 0xC3;   // ret
        }

        map.set_protection(Protection::ReadExecute).unwrap();

        let jitfn: extern "C" fn() -> u8 = unsafe { mem::transmute(map.mut_ptr()) };
        assert_eq!(jitfn(), 0xab);
    }

    #[test]
    fn offset_set_protection() {
        let tempdir = tempdir::TempDir::new("mmap").unwrap();
        let path = tempdir.path().join("mmap");

        let file = fs::OpenOptions::new()
                                   .read(true)
                                   .write(true)
                                   .create(true)
                                   .open(&path)
                                   .unwrap();

        file.set_len(500000 as u64).unwrap();

        let offset = 5099;
        let len = 50050;

        let mut mmap = Mmap::open_with_offset(&file,
                                              Protection::ReadWrite,
                                              offset,
                                              len).unwrap();
        assert_eq!(len, mmap.len());

        let zeros = iter::repeat(0).take(len).collect::<Vec<_>>();
        let incr = (0..len).map(|n| n as u8).collect::<Vec<_>>();

        // check that the mmap is empty
        assert_eq!(&zeros[..], unsafe { mmap.as_slice() });

        // write values into the mmap
        unsafe { mmap.as_mut_slice() }.write_all(&incr[..]).unwrap();

        // change to read-only protection
        mmap.set_protection(Protection::Read).unwrap();

        // read values back
        assert_eq!(&incr[..], unsafe { mmap.as_slice() });
    }
}
