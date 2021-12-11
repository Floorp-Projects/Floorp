// Copyright © 2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.
use cubeb_backend::SampleFormat as fmt;
use std::mem;

pub fn allocate_array_by_size<T: Clone + Default>(size: usize) -> Vec<T> {
    assert_eq!(size % mem::size_of::<T>(), 0);
    let elements = size / mem::size_of::<T>();
    allocate_array::<T>(elements)
}

pub fn allocate_array<T: Clone + Default>(elements: usize) -> Vec<T> {
    vec![T::default(); elements]
}

pub fn forget_vec<T>(v: Vec<T>) -> (*mut T, usize) {
    // Drop any excess capacity by into_boxed_slice.
    let mut slice = v.into_boxed_slice();
    let ptr_and_len = (slice.as_mut_ptr(), slice.len());
    mem::forget(slice); // Leak the memory to the external code.
    ptr_and_len
}

#[inline]
pub fn retake_forgotten_vec<T>(ptr: *mut T, len: usize) -> Vec<T> {
    unsafe { Vec::from_raw_parts(ptr, len, len) }
}

pub fn cubeb_sample_size(format: fmt) -> usize {
    match format {
        fmt::S16LE | fmt::S16BE | fmt::S16NE => mem::size_of::<i16>(),
        fmt::Float32LE | fmt::Float32BE | fmt::Float32NE => mem::size_of::<f32>(),
    }
}

pub struct Finalizer<F: FnOnce()>(Option<F>);

impl<F: FnOnce()> Finalizer<F> {
    pub fn dismiss(&mut self) {
        let _ = self.0.take();
        assert!(self.0.is_none());
    }
}

impl<F: FnOnce()> Drop for Finalizer<F> {
    fn drop(&mut self) {
        if let Some(f) = self.0.take() {
            f();
        }
    }
}

pub fn finally<F: FnOnce()>(f: F) -> Finalizer<F> {
    Finalizer(Some(f))
}

#[test]
fn test_forget_vec_and_retake_it() {
    let expected: Vec<u32> = (10..20).collect();
    let leaked = expected.clone();
    let (ptr, len) = forget_vec(leaked);
    let retaken = retake_forgotten_vec(ptr, len);
    for (idx, data) in retaken.iter().enumerate() {
        assert_eq!(*data, expected[idx]);
    }
}

#[test]
fn test_cubeb_sample_size() {
    let pairs = [
        (fmt::S16LE, mem::size_of::<i16>()),
        (fmt::S16BE, mem::size_of::<i16>()),
        (fmt::S16NE, mem::size_of::<i16>()),
        (fmt::Float32LE, mem::size_of::<f32>()),
        (fmt::Float32BE, mem::size_of::<f32>()),
        (fmt::Float32NE, mem::size_of::<f32>()),
    ];

    for pair in pairs.iter() {
        let (fotmat, size) = pair;
        assert_eq!(cubeb_sample_size(*fotmat), *size);
    }
}

#[test]
fn test_finally() {
    let mut x = 0;

    {
        let y = &mut x;
        let _finally = finally(|| {
            *y = 100;
        });
    }
    assert_eq!(x, 100);

    {
        let y = &mut x;
        let mut finally = finally(|| {
            *y = 200;
        });
        finally.dismiss();
    }
    assert_eq!(x, 100);
}
