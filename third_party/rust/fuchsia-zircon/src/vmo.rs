// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Type-safe bindings for Zircon vmo objects.

use {AsHandleRef, Cookied, HandleBased, Handle, HandleRef, Status};
use {sys, into_result};
use std::{mem, ptr};

/// An object representing a Zircon
/// [virtual memory object](https://fuchsia.googlesource.com/zircon/+/master/docs/objects/vm_object.md).
///
/// As essentially a subtype of `Handle`, it can be freely interconverted.
#[derive(Debug, Eq, PartialEq)]
pub struct Vmo(Handle);
impl_handle_based!(Vmo);
impl Cookied for Vmo {}

impl Vmo {
    /// Create a virtual memory object.
    ///
    /// Wraps the
    /// `zx_vmo_create`
    /// syscall. See the
    /// [Shared Memory: Virtual Memory Objects (VMOs)](https://fuchsia.googlesource.com/zircon/+/master/docs/concepts.md#Shared-Memory_Virtual-Memory-Objects-VMOs)
    /// for more information.
    pub fn create(size: u64, options: VmoOpts) -> Result<Vmo, Status> {
        let mut handle = 0;
        let status = unsafe { sys::zx_vmo_create(size, options as u32, &mut handle) };
        into_result(status, ||
            Vmo::from(Handle(handle)))
    }

    /// Read from a virtual memory object.
    ///
    /// Wraps the `zx_vmo_read` syscall.
    pub fn read(&self, data: &mut [u8], offset: u64) -> Result<usize, Status> {
        unsafe {
            let mut actual = 0;
            let status = sys::zx_vmo_read(self.raw_handle(), data.as_mut_ptr(),
                offset, data.len(), &mut actual);
            into_result(status, || actual)
        }
    }

    /// Write to a virtual memory object.
    ///
    /// Wraps the `zx_vmo_write` syscall.
    pub fn write(&self, data: &[u8], offset: u64) -> Result<usize, Status> {
        unsafe {
            let mut actual = 0;
            let status = sys::zx_vmo_write(self.raw_handle(), data.as_ptr(),
                offset, data.len(), &mut actual);
            into_result(status, || actual)
        }
    }

    /// Get the size of a virtual memory object.
    ///
    /// Wraps the `zx_vmo_get_size` syscall.
    pub fn get_size(&self) -> Result<u64, Status> {
        let mut size = 0;
        let status = unsafe { sys::zx_vmo_get_size(self.raw_handle(), &mut size) };
        into_result(status, || size)
    }

    /// Attempt to change the size of a virtual memory object.
    ///
    /// Wraps the `zx_vmo_set_size` syscall.
    pub fn set_size(&self, size: u64) -> Result<(), Status> {
        let status = unsafe { sys::zx_vmo_set_size(self.raw_handle(), size) };
        into_result(status, || ())
    }

    /// Perform an operation on a range of a virtual memory object.
    ///
    /// Wraps the
    /// [zx_vmo_op_range](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/vmo_op_range.md)
    /// syscall.
    pub fn op_range(&self, op: VmoOp, offset: u64, size: u64) -> Result<(), Status> {
        let status = unsafe {
            sys::zx_vmo_op_range(self.raw_handle(), op as u32, offset, size, ptr::null_mut(), 0)
        };
        into_result(status, || ())
    }

    /// Look up a list of physical addresses corresponding to the pages held by the VMO from
    /// `offset` to `offset`+`size`, and store them in `buffer`.
    ///
    /// Wraps the
    /// [zx_vmo_op_range](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/vmo_op_range.md)
    /// syscall with ZX_VMO_OP_LOOKUP.
    pub fn lookup(&self, offset: u64, size: u64, buffer: &mut [sys::zx_paddr_t])
        -> Result<(), Status>
    {
        let status = unsafe {
            sys::zx_vmo_op_range(self.raw_handle(), sys::ZX_VMO_OP_LOOKUP, offset, size,
                buffer.as_mut_ptr() as *mut u8, buffer.len() * mem::size_of::<sys::zx_paddr_t>())
        };
        into_result(status, || ())
    }

    /// Create a new virtual memory object that clones a range of this one.
    ///
    /// Wraps the
    /// [zx_vmo_clone](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/vmo_clone.md)
    /// syscall.
    pub fn clone(&self, options: VmoCloneOpts, offset: u64, size: u64) -> Result<Vmo, Status> {
        let mut out = 0;
        let status = unsafe {
            sys::zx_vmo_clone(self.raw_handle(), options as u32, offset, size, &mut out)
        };
        into_result(status, || Vmo::from(Handle(out)))
    }
}

/// Options for creating virtual memory objects. None supported yet.
#[repr(u32)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum VmoOpts {
    /// Default options.
    Default = 0,
}

impl Default for VmoOpts {
    fn default() -> Self {
        VmoOpts::Default
    }
}

#[repr(u32)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum VmoOp {
    /// Commit `size` bytes worth of pages starting at byte `offset` for the VMO.
    Commit = sys::ZX_VMO_OP_COMMIT,
    /// Release a range of pages previously committed to the VMO from `offset` to `offset`+`size`.
    Decommit = sys::ZX_VMO_OP_DECOMMIT,
    // Presently unsupported.
    Lock = sys::ZX_VMO_OP_LOCK,
    // Presently unsupported.
    Unlock = sys::ZX_VMO_OP_UNLOCK,
    /// Perform a cache sync operation.
    CacheSync = sys::ZX_VMO_OP_CACHE_SYNC,
    /// Perform a cache invalidation operation.
    CacheInvalidate = sys::ZX_VMO_OP_CACHE_INVALIDATE,
    /// Perform a cache clean operation.
    CacheClean = sys::ZX_VMO_OP_CACHE_CLEAN,
    /// Perform cache clean and invalidation operations together.
    CacheCleanInvalidate = sys::ZX_VMO_OP_CACHE_CLEAN_INVALIDATE,
}

#[repr(u32)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum VmoCloneOpts {
    /// Create a copy-on-write clone.
    CopyOnWrite = sys::ZX_VMO_CLONE_COPY_ON_WRITE,
}

impl Default for VmoCloneOpts {
    fn default() -> Self {
        VmoCloneOpts::CopyOnWrite
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn vmo_get_size() {
        let size = 16 * 1024 * 1024;
        let vmo = Vmo::create(size, VmoOpts::Default).unwrap();
        assert_eq!(size, vmo.get_size().unwrap());
    }

    #[test]
    fn vmo_set_size() {
        let start_size = 12;
        let vmo = Vmo::create(start_size, VmoOpts::Default).unwrap();
        assert_eq!(start_size, vmo.get_size().unwrap());

        // Change the size and make sure the new size is reported
        let new_size = 23;
        assert!(vmo.set_size(new_size).is_ok());
        assert_eq!(new_size, vmo.get_size().unwrap());
    }

    #[test]
    fn vmo_read_write() {
        let mut vec1 = vec![0; 16];
        let vmo = Vmo::create(vec1.len() as u64, VmoOpts::Default).unwrap();
        assert_eq!(vmo.write(b"abcdef", 0), Ok(6));
        assert_eq!(16, vmo.read(&mut vec1, 0).unwrap());
        assert_eq!(b"abcdef", &vec1[0..6]);
        assert_eq!(vmo.write(b"123", 2), Ok(3));
        assert_eq!(16, vmo.read(&mut vec1, 0).unwrap());
        assert_eq!(b"ab123f", &vec1[0..6]);
        assert_eq!(15, vmo.read(&mut vec1, 1).unwrap());
        assert_eq!(b"b123f", &vec1[0..5]);
    }

    #[test]
    fn vmo_op_range_unsupported() {
        let vmo = Vmo::create(12, VmoOpts::Default).unwrap();
        assert_eq!(vmo.op_range(VmoOp::Lock, 0, 1), Err(Status::ErrNotSupported));
        assert_eq!(vmo.op_range(VmoOp::Unlock, 0, 1), Err(Status::ErrNotSupported));
    }

    #[test]
    fn vmo_lookup() {
        let vmo = Vmo::create(12, VmoOpts::Default).unwrap();
        let mut buffer = vec![0; 2];

        // Lookup will fail as it is not committed yet.
        assert_eq!(vmo.lookup(0, 12, &mut buffer), Err(Status::ErrNoMemory));

        // Commit and try again.
        assert_eq!(vmo.op_range(VmoOp::Commit, 0, 12), Ok(()));
        assert_eq!(vmo.lookup(0, 12, &mut buffer), Ok(()));
        assert_ne!(buffer[0], 0);
        assert_eq!(buffer[1], 0);

        // If we decommit then lookup should go back to failing.
        assert_eq!(vmo.op_range(VmoOp::Decommit, 0, 12), Ok(()));
        assert_eq!(vmo.lookup(0, 12, &mut buffer), Err(Status::ErrNoMemory));
    }

    #[test]
    fn vmo_cache() {
        let vmo = Vmo::create(12, VmoOpts::Default).unwrap();

        // Cache operations should all succeed.
        assert_eq!(vmo.op_range(VmoOp::CacheSync, 0, 12), Ok(()));
        assert_eq!(vmo.op_range(VmoOp::CacheInvalidate, 0, 12), Ok(()));
        assert_eq!(vmo.op_range(VmoOp::CacheClean, 0, 12), Ok(()));
        assert_eq!(vmo.op_range(VmoOp::CacheCleanInvalidate, 0, 12), Ok(()));
    }

    #[test]
    fn vmo_clone() {
        let original = Vmo::create(12, VmoOpts::Default).unwrap();
        assert_eq!(original.write(b"one", 0), Ok(3));

        // Clone the VMO, and make sure it contains what we expect.
        let clone = original.clone(VmoCloneOpts::CopyOnWrite, 0, 10).unwrap();
        let mut read_buffer = vec![0; 16];
        assert_eq!(clone.read(&mut read_buffer, 0), Ok(10));
        assert_eq!(&read_buffer[0..3], b"one");

        // Writing to the original will affect the clone too, surprisingly.
        assert_eq!(original.write(b"two", 0), Ok(3));
        assert_eq!(original.read(&mut read_buffer, 0), Ok(12));
        assert_eq!(&read_buffer[0..3], b"two");
        assert_eq!(clone.read(&mut read_buffer, 0), Ok(10));
        assert_eq!(&read_buffer[0..3], b"two");

        // However, writing to the clone will not affect the original
        assert_eq!(clone.write(b"three", 0), Ok(5));
        assert_eq!(original.read(&mut read_buffer, 0), Ok(12));
        assert_eq!(&read_buffer[0..3], b"two");
        assert_eq!(clone.read(&mut read_buffer, 0), Ok(10));
        assert_eq!(&read_buffer[0..5], b"three");

        // And now that the copy-on-write has happened, writing to the original will not affect the
        // clone. How bizarre.
        assert_eq!(original.write(b"four", 0), Ok(4));
        assert_eq!(original.read(&mut read_buffer, 0), Ok(12));
        assert_eq!(&read_buffer[0..4], b"four");
        assert_eq!(clone.read(&mut read_buffer, 0), Ok(10));
        assert_eq!(&read_buffer[0..5], b"three");
    }
}
