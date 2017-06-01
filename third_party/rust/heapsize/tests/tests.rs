/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![cfg_attr(feature= "unstable", feature(alloc, heap_api, repr_simd))]

extern crate heapsize;

use heapsize::{HeapSizeOf, heap_size_of};
use std::os::raw::c_void;

const EMPTY: *mut () = 0x1 as *mut ();

/// https://github.com/servo/heapsize/issues/74
#[cfg(feature = "flexible-tests")]
macro_rules! assert_size {
    ($actual: expr, $expected: expr) => {
        {
            let actual = $actual;
            let expected = $expected;
            assert!(actual >= expected, "expected {:?} >= {:?}", actual, expected)
        }
    }
}

#[cfg(not(feature = "flexible-tests"))]
macro_rules! assert_size {
    ($actual: expr, $expected: expr) => {
        assert_eq!($actual, $expected)
    }
}

#[cfg(feature = "unstable")]
mod unstable {
    extern crate alloc;

    use heapsize::heap_size_of;
    use std::os::raw::c_void;

    #[repr(C, simd)]
    struct OverAligned(u64, u64, u64, u64);

    #[test]
    fn check_empty() {
        assert_eq!(::EMPTY, alloc::heap::EMPTY);
    }

    #[cfg(not(target_os = "windows"))]
    #[test]
    fn test_alloc() {
        unsafe {
            // A 64 byte request is allocated exactly.
            let x = alloc::heap::allocate(64, 0);
            assert_size!(heap_size_of(x as *const c_void), 64);
            alloc::heap::deallocate(x, 64, 0);

            // A 255 byte request is rounded up to 256 bytes.
            let x = alloc::heap::allocate(255, 0);
            assert_size!(heap_size_of(x as *const c_void), 256);
            alloc::heap::deallocate(x, 255, 0);

            // A 1MiB request is allocated exactly.
            let x = alloc::heap::allocate(1024 * 1024, 0);
            assert_size!(heap_size_of(x as *const c_void), 1024 * 1024);
            alloc::heap::deallocate(x, 1024 * 1024, 0);

            // An overaligned 1MiB request is allocated exactly.
            let x = alloc::heap::allocate(1024 * 1024, 32);
            assert_size!(heap_size_of(x as *const c_void), 1024 * 1024);
            alloc::heap::deallocate(x, 1024 * 1024, 32);
        }
    }

    #[cfg(target_os = "windows")]
    #[test]
    fn test_alloc() {
        unsafe {
            // A 64 byte request is allocated exactly.
            let x = alloc::heap::allocate(64, 0);
            assert_size!(heap_size_of(x as *const c_void), 64);
            alloc::heap::deallocate(x, 64, 0);

            // A 255 byte request is allocated exactly.
            let x = alloc::heap::allocate(255, 0);
            assert_size!(heap_size_of(x as *const c_void), 255);
            alloc::heap::deallocate(x, 255, 0);

            // A 1MiB request is allocated exactly.
            let x = alloc::heap::allocate(1024 * 1024, 0);
            assert_size!(heap_size_of(x as *const c_void), 1024 * 1024);
            alloc::heap::deallocate(x, 1024 * 1024, 0);

            // An overaligned 1MiB request is over-allocated.
            let x = alloc::heap::allocate(1024 * 1024, 32);
            assert_size!(heap_size_of(x as *const c_void), 1024 * 1024 + 32);
            alloc::heap::deallocate(x, 1024 * 1024, 32);
        }
    }

    #[cfg(not(target_os = "windows"))]
    #[test]
    fn test_simd() {
        let x = Box::new(OverAligned(0, 0, 0, 0));
        assert_size!(unsafe { heap_size_of(&*x as *const _ as *const c_void) }, 32);
    }

    #[cfg(target_os = "windows")]
    #[test]
    fn test_simd() {
        let x = Box::new(OverAligned(0, 0, 0, 0));
        assert_size!(unsafe { heap_size_of(&*x as *const _ as *const c_void) }, 32 + 32);
    }
}

#[test]
fn test_boxed_str() {
    let x = "raclette".to_owned().into_boxed_str();
    assert_size!(x.heap_size_of_children(), 8);
}

#[test]
fn test_heap_size() {

    // Note: jemalloc often rounds up request sizes. However, it does not round up for request
    // sizes of 8 and higher that are powers of two. We take advantage of knowledge here to make
    // the sizes of various heap-allocated blocks predictable.

    //-----------------------------------------------------------------------
    // Start with basic heap block measurement.

    unsafe {
        // EMPTY is the special non-null address used to represent zero-size allocations.
        assert_size!(heap_size_of(EMPTY as *const c_void), 0);
    }

    //-----------------------------------------------------------------------
    // Test HeapSizeOf implementations for various built-in types.

    // Not on the heap; 0 bytes.
    let x = 0i64;
    assert_size!(x.heap_size_of_children(), 0);

    // An i64 is 8 bytes.
    let x = Box::new(0i64);
    assert_size!(x.heap_size_of_children(), 8);

    // An ascii string with 16 chars is 16 bytes in UTF-8.
    let string = String::from("0123456789abcdef");
    assert_size!(string.heap_size_of_children(), 16);

    let string_ref: (&String, ()) = (&string, ());
    assert_size!(string_ref.heap_size_of_children(), 0);

    let slice: &str = &*string;
    assert_size!(slice.heap_size_of_children(), 0);

    // Not on the heap.
    let x: Option<i32> = None;
    assert_size!(x.heap_size_of_children(), 0);

    // Not on the heap.
    let x = Some(0i64);
    assert_size!(x.heap_size_of_children(), 0);

    // The `Some` is not on the heap, but the Box is.
    let x = Some(Box::new(0i64));
    assert_size!(x.heap_size_of_children(), 8);

    // Not on the heap.
    let x = ::std::sync::Arc::new(0i64);
    assert_size!(x.heap_size_of_children(), 0);

    // The `Arc` is not on the heap, but the Box is.
    let x = ::std::sync::Arc::new(Box::new(0i64));
    assert_size!(x.heap_size_of_children(), 8);

    // Zero elements, no heap storage.
    let x: Vec<i64> = vec![];
    assert_size!(x.heap_size_of_children(), 0);

    // Four elements, 8 bytes per element.
    let x = vec![0i64, 1i64, 2i64, 3i64];
    assert_size!(x.heap_size_of_children(), 32);
}

#[test]
fn test_boxed_slice() {
    let x = vec![1i64, 2i64].into_boxed_slice();
    assert_size!(x.heap_size_of_children(), 16)
}
