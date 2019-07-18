// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use core::cmp;
use core::mem;
use core::num::Wrapping;
use core::ops;
use core::sync::atomic::Ordering;
use fallback;

#[cfg(feature = "nightly")]
use core::sync::atomic::{
    AtomicI16, AtomicI32, AtomicI64, AtomicI8, AtomicU16, AtomicU32, AtomicU64, AtomicU8,
};

#[cfg(not(feature = "nightly"))]
use core::sync::atomic::AtomicUsize;
#[cfg(not(feature = "nightly"))]
const SIZEOF_USIZE: usize = mem::size_of::<usize>();
#[cfg(not(feature = "nightly"))]
const ALIGNOF_USIZE: usize = mem::align_of::<usize>();

#[cfg(feature = "nightly")]
#[inline]
pub const fn atomic_is_lock_free<T>() -> bool {
    let size = mem::size_of::<T>();
    // FIXME: switch to … && … && … once that operator is supported in const functions
    (1 == size.count_ones()) & (8 >= size) & (mem::align_of::<T>() >= size)
}

#[cfg(not(feature = "nightly"))]
#[inline]
pub fn atomic_is_lock_free<T>() -> bool {
    let size = mem::size_of::<T>();
    1 == size.count_ones() && SIZEOF_USIZE >= size && mem::align_of::<T>() >= ALIGNOF_USIZE
}

#[inline]
pub unsafe fn atomic_load<T>(dst: *mut T, order: Ordering) -> T {
    match mem::size_of::<T>() {
        #[cfg(all(feature = "nightly", target_has_atomic = "8"))]
        1 if mem::align_of::<T>() >= 1 =>
        {
            mem::transmute_copy(&(*(dst as *const AtomicU8)).load(order))
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "16"))]
        2 if mem::align_of::<T>() >= 2 =>
        {
            mem::transmute_copy(&(*(dst as *const AtomicU16)).load(order))
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "32"))]
        4 if mem::align_of::<T>() >= 4 =>
        {
            mem::transmute_copy(&(*(dst as *const AtomicU32)).load(order))
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "64"))]
        8 if mem::align_of::<T>() >= 8 =>
        {
            mem::transmute_copy(&(*(dst as *const AtomicU64)).load(order))
        }
        #[cfg(not(feature = "nightly"))]
        SIZEOF_USIZE if mem::align_of::<T>() >= ALIGNOF_USIZE =>
        {
            mem::transmute_copy(&(*(dst as *const AtomicUsize)).load(order))
        }
        _ => fallback::atomic_load(dst),
    }
}

#[inline]
pub unsafe fn atomic_store<T>(dst: *mut T, val: T, order: Ordering) {
    match mem::size_of::<T>() {
        #[cfg(all(feature = "nightly", target_has_atomic = "8"))]
        1 if mem::align_of::<T>() >= 1 =>
        {
            (*(dst as *const AtomicU8)).store(mem::transmute_copy(&val), order)
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "16"))]
        2 if mem::align_of::<T>() >= 2 =>
        {
            (*(dst as *const AtomicU16)).store(mem::transmute_copy(&val), order)
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "32"))]
        4 if mem::align_of::<T>() >= 4 =>
        {
            (*(dst as *const AtomicU32)).store(mem::transmute_copy(&val), order)
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "64"))]
        8 if mem::align_of::<T>() >= 8 =>
        {
            (*(dst as *const AtomicU64)).store(mem::transmute_copy(&val), order)
        }
        #[cfg(not(feature = "nightly"))]
        SIZEOF_USIZE if mem::align_of::<T>() >= ALIGNOF_USIZE =>
        {
            (*(dst as *const AtomicUsize)).store(mem::transmute_copy(&val), order)
        }
        _ => fallback::atomic_store(dst, val),
    }
}

#[inline]
pub unsafe fn atomic_swap<T>(dst: *mut T, val: T, order: Ordering) -> T {
    match mem::size_of::<T>() {
        #[cfg(all(feature = "nightly", target_has_atomic = "8"))]
        1 if mem::align_of::<T>() >= 1 =>
        {
            mem::transmute_copy(&(*(dst as *const AtomicU8)).swap(mem::transmute_copy(&val), order))
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "16"))]
        2 if mem::align_of::<T>() >= 2 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU16)).swap(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "32"))]
        4 if mem::align_of::<T>() >= 4 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU32)).swap(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "64"))]
        8 if mem::align_of::<T>() >= 8 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU64)).swap(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(not(feature = "nightly"))]
        SIZEOF_USIZE if mem::align_of::<T>() >= ALIGNOF_USIZE =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicUsize)).swap(mem::transmute_copy(&val), order),
            )
        }
        _ => fallback::atomic_swap(dst, val),
    }
}

#[inline]
unsafe fn map_result<T, U>(r: Result<T, T>) -> Result<U, U> {
    match r {
        Ok(x) => Ok(mem::transmute_copy(&x)),
        Err(x) => Err(mem::transmute_copy(&x)),
    }
}

#[inline]
pub unsafe fn atomic_compare_exchange<T>(
    dst: *mut T,
    current: T,
    new: T,
    success: Ordering,
    failure: Ordering,
) -> Result<T, T> {
    match mem::size_of::<T>() {
        #[cfg(all(feature = "nightly", target_has_atomic = "8"))]
        1 if mem::align_of::<T>() >= 1 =>
        {
            map_result((*(dst as *const AtomicU8)).compare_exchange(
                mem::transmute_copy(&current),
                mem::transmute_copy(&new),
                success,
                failure,
            ))
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "16"))]
        2 if mem::align_of::<T>() >= 2 =>
        {
            map_result((*(dst as *const AtomicU16)).compare_exchange(
                mem::transmute_copy(&current),
                mem::transmute_copy(&new),
                success,
                failure,
            ))
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "32"))]
        4 if mem::align_of::<T>() >= 4 =>
        {
            map_result((*(dst as *const AtomicU32)).compare_exchange(
                mem::transmute_copy(&current),
                mem::transmute_copy(&new),
                success,
                failure,
            ))
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "64"))]
        8 if mem::align_of::<T>() >= 8 =>
        {
            map_result((*(dst as *const AtomicU64)).compare_exchange(
                mem::transmute_copy(&current),
                mem::transmute_copy(&new),
                success,
                failure,
            ))
        }
        #[cfg(not(feature = "nightly"))]
        SIZEOF_USIZE if mem::align_of::<T>() >= ALIGNOF_USIZE =>
        {
            map_result((*(dst as *const AtomicUsize)).compare_exchange(
                mem::transmute_copy(&current),
                mem::transmute_copy(&new),
                success,
                failure,
            ))
        }
        _ => fallback::atomic_compare_exchange(dst, current, new),
    }
}

#[inline]
pub unsafe fn atomic_compare_exchange_weak<T>(
    dst: *mut T,
    current: T,
    new: T,
    success: Ordering,
    failure: Ordering,
) -> Result<T, T> {
    match mem::size_of::<T>() {
        #[cfg(all(feature = "nightly", target_has_atomic = "8"))]
        1 if mem::align_of::<T>() >= 1 =>
        {
            map_result((*(dst as *const AtomicU8)).compare_exchange_weak(
                mem::transmute_copy(&current),
                mem::transmute_copy(&new),
                success,
                failure,
            ))
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "16"))]
        2 if mem::align_of::<T>() >= 2 =>
        {
            map_result((*(dst as *const AtomicU16)).compare_exchange_weak(
                mem::transmute_copy(&current),
                mem::transmute_copy(&new),
                success,
                failure,
            ))
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "32"))]
        4 if mem::align_of::<T>() >= 4 =>
        {
            map_result((*(dst as *const AtomicU32)).compare_exchange_weak(
                mem::transmute_copy(&current),
                mem::transmute_copy(&new),
                success,
                failure,
            ))
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "64"))]
        8 if mem::align_of::<T>() >= 8 =>
        {
            map_result((*(dst as *const AtomicU64)).compare_exchange_weak(
                mem::transmute_copy(&current),
                mem::transmute_copy(&new),
                success,
                failure,
            ))
        }
        #[cfg(not(feature = "nightly"))]
        SIZEOF_USIZE if mem::align_of::<T>() >= ALIGNOF_USIZE =>
        {
            map_result((*(dst as *const AtomicUsize)).compare_exchange_weak(
                mem::transmute_copy(&current),
                mem::transmute_copy(&new),
                success,
                failure,
            ))
        }
        _ => fallback::atomic_compare_exchange(dst, current, new),
    }
}

#[inline]
pub unsafe fn atomic_add<T: Copy>(dst: *mut T, val: T, order: Ordering) -> T
where
    Wrapping<T>: ops::Add<Output = Wrapping<T>>,
{
    match mem::size_of::<T>() {
        #[cfg(all(feature = "nightly", target_has_atomic = "8"))]
        1 if mem::align_of::<T>() >= 1 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU8)).fetch_add(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "16"))]
        2 if mem::align_of::<T>() >= 2 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU16)).fetch_add(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "32"))]
        4 if mem::align_of::<T>() >= 4 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU32)).fetch_add(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "64"))]
        8 if mem::align_of::<T>() >= 8 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU64)).fetch_add(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(not(feature = "nightly"))]
        SIZEOF_USIZE if mem::align_of::<T>() >= ALIGNOF_USIZE =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicUsize)).fetch_add(mem::transmute_copy(&val), order),
            )
        }
        _ => fallback::atomic_add(dst, val),
    }
}

#[inline]
pub unsafe fn atomic_sub<T: Copy>(dst: *mut T, val: T, order: Ordering) -> T
where
    Wrapping<T>: ops::Sub<Output = Wrapping<T>>,
{
    match mem::size_of::<T>() {
        #[cfg(all(feature = "nightly", target_has_atomic = "8"))]
        1 if mem::align_of::<T>() >= 1 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU8)).fetch_sub(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "16"))]
        2 if mem::align_of::<T>() >= 2 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU16)).fetch_sub(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "32"))]
        4 if mem::align_of::<T>() >= 4 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU32)).fetch_sub(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "64"))]
        8 if mem::align_of::<T>() >= 8 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU64)).fetch_sub(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(not(feature = "nightly"))]
        SIZEOF_USIZE if mem::align_of::<T>() >= ALIGNOF_USIZE =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicUsize)).fetch_sub(mem::transmute_copy(&val), order),
            )
        }
        _ => fallback::atomic_sub(dst, val),
    }
}

#[inline]
pub unsafe fn atomic_and<T: Copy + ops::BitAnd<Output = T>>(
    dst: *mut T,
    val: T,
    order: Ordering,
) -> T {
    match mem::size_of::<T>() {
        #[cfg(all(feature = "nightly", target_has_atomic = "8"))]
        1 if mem::align_of::<T>() >= 1 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU8)).fetch_and(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "16"))]
        2 if mem::align_of::<T>() >= 2 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU16)).fetch_and(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "32"))]
        4 if mem::align_of::<T>() >= 4 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU32)).fetch_and(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "64"))]
        8 if mem::align_of::<T>() >= 8 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU64)).fetch_and(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(not(feature = "nightly"))]
        SIZEOF_USIZE if mem::align_of::<T>() >= ALIGNOF_USIZE =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicUsize)).fetch_and(mem::transmute_copy(&val), order),
            )
        }
        _ => fallback::atomic_and(dst, val),
    }
}

#[inline]
pub unsafe fn atomic_or<T: Copy + ops::BitOr<Output = T>>(
    dst: *mut T,
    val: T,
    order: Ordering,
) -> T {
    match mem::size_of::<T>() {
        #[cfg(all(feature = "nightly", target_has_atomic = "8"))]
        1 if mem::align_of::<T>() >= 1 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU8)).fetch_or(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "16"))]
        2 if mem::align_of::<T>() >= 2 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU16)).fetch_or(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "32"))]
        4 if mem::align_of::<T>() >= 4 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU32)).fetch_or(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "64"))]
        8 if mem::align_of::<T>() >= 8 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU64)).fetch_or(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(not(feature = "nightly"))]
        SIZEOF_USIZE if mem::align_of::<T>() >= ALIGNOF_USIZE =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicUsize)).fetch_or(mem::transmute_copy(&val), order),
            )
        }
        _ => fallback::atomic_or(dst, val),
    }
}

#[inline]
pub unsafe fn atomic_xor<T: Copy + ops::BitXor<Output = T>>(
    dst: *mut T,
    val: T,
    order: Ordering,
) -> T {
    match mem::size_of::<T>() {
        #[cfg(all(feature = "nightly", target_has_atomic = "8"))]
        1 if mem::align_of::<T>() >= 1 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU8)).fetch_xor(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "16"))]
        2 if mem::align_of::<T>() >= 2 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU16)).fetch_xor(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "32"))]
        4 if mem::align_of::<T>() >= 4 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU32)).fetch_xor(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "64"))]
        8 if mem::align_of::<T>() >= 8 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU64)).fetch_xor(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(not(feature = "nightly"))]
        SIZEOF_USIZE if mem::align_of::<T>() >= ALIGNOF_USIZE =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicUsize)).fetch_xor(mem::transmute_copy(&val), order),
            )
        }
        _ => fallback::atomic_xor(dst, val),
    }
}

#[inline]
pub unsafe fn atomic_min<T: Copy + cmp::Ord>(dst: *mut T, val: T, order: Ordering) -> T {
    // Silence warning, fetch_min is not stable yet
    #[cfg(not(feature = "nightly"))]
    let _ = order;

    match mem::size_of::<T>() {
        #[cfg(all(feature = "nightly", target_has_atomic = "8"))]
        1 if mem::align_of::<T>() >= 1 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicI8)).fetch_min(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "16"))]
        2 if mem::align_of::<T>() >= 2 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicI16)).fetch_min(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "32"))]
        4 if mem::align_of::<T>() >= 4 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicI32)).fetch_min(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "64"))]
        8 if mem::align_of::<T>() >= 8 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicI64)).fetch_min(mem::transmute_copy(&val), order),
            )
        }
        _ => fallback::atomic_min(dst, val),
    }
}

#[inline]
pub unsafe fn atomic_max<T: Copy + cmp::Ord>(dst: *mut T, val: T, order: Ordering) -> T {
    // Silence warning, fetch_min is not stable yet
    #[cfg(not(feature = "nightly"))]
    let _ = order;

    match mem::size_of::<T>() {
        #[cfg(all(feature = "nightly", target_has_atomic = "8"))]
        1 if mem::align_of::<T>() >= 1 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicI8)).fetch_max(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "16"))]
        2 if mem::align_of::<T>() >= 2 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicI16)).fetch_max(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "32"))]
        4 if mem::align_of::<T>() >= 4 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicI32)).fetch_max(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "64"))]
        8 if mem::align_of::<T>() >= 8 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicI64)).fetch_max(mem::transmute_copy(&val), order),
            )
        }
        _ => fallback::atomic_max(dst, val),
    }
}

#[inline]
pub unsafe fn atomic_umin<T: Copy + cmp::Ord>(dst: *mut T, val: T, order: Ordering) -> T {
    // Silence warning, fetch_min is not stable yet
    #[cfg(not(feature = "nightly"))]
    let _ = order;

    match mem::size_of::<T>() {
        #[cfg(all(feature = "nightly", target_has_atomic = "8"))]
        1 if mem::align_of::<T>() >= 1 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU8)).fetch_min(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "16"))]
        2 if mem::align_of::<T>() >= 2 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU16)).fetch_min(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "32"))]
        4 if mem::align_of::<T>() >= 4 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU32)).fetch_min(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "64"))]
        8 if mem::align_of::<T>() >= 8 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU64)).fetch_min(mem::transmute_copy(&val), order),
            )
        }
        _ => fallback::atomic_min(dst, val),
    }
}

#[inline]
pub unsafe fn atomic_umax<T: Copy + cmp::Ord>(dst: *mut T, val: T, order: Ordering) -> T {
    // Silence warning, fetch_min is not stable yet
    #[cfg(not(feature = "nightly"))]
    let _ = order;

    match mem::size_of::<T>() {
        #[cfg(all(feature = "nightly", target_has_atomic = "8"))]
        1 if mem::align_of::<T>() >= 1 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU8)).fetch_max(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "16"))]
        2 if mem::align_of::<T>() >= 2 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU16)).fetch_max(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "32"))]
        4 if mem::align_of::<T>() >= 4 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU32)).fetch_max(mem::transmute_copy(&val), order),
            )
        }
        #[cfg(all(feature = "nightly", target_has_atomic = "64"))]
        8 if mem::align_of::<T>() >= 8 =>
        {
            mem::transmute_copy(
                &(*(dst as *const AtomicU64)).fetch_max(mem::transmute_copy(&val), order),
            )
        }
        _ => fallback::atomic_max(dst, val),
    }
}
