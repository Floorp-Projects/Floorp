//! Creates a Waker that can be observed from tests.

use std::mem::forget;
use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::Arc;
use std::task::{RawWaker, RawWakerVTable, Waker};

#[derive(Default)]
pub struct WakerHandle {
    clone_count: AtomicU32,
    drop_count: AtomicU32,
    wake_count: AtomicU32,
}

impl WakerHandle {
    pub fn clone_count(&self) -> u32 {
        self.clone_count.load(Ordering::Relaxed)
    }

    pub fn drop_count(&self) -> u32 {
        self.drop_count.load(Ordering::Relaxed)
    }

    pub fn wake_count(&self) -> u32 {
        self.wake_count.load(Ordering::Relaxed)
    }
}

pub fn waker() -> (Waker, Arc<WakerHandle>) {
    let waker_handle = Arc::new(WakerHandle::default());
    let waker_handle_ptr = Arc::into_raw(waker_handle.clone());
    let raw_waker = RawWaker::new(waker_handle_ptr as *const _, waker_vtable());
    (unsafe { Waker::from_raw(raw_waker) }, waker_handle)
}

pub(super) fn waker_vtable() -> &'static RawWakerVTable {
    &RawWakerVTable::new(clone_raw, wake_raw, wake_by_ref_raw, drop_raw)
}

unsafe fn clone_raw(data: *const ()) -> RawWaker {
    let handle: Arc<WakerHandle> = Arc::from_raw(data as *const _);
    handle.clone_count.fetch_add(1, Ordering::Relaxed);
    forget(handle.clone());
    forget(handle);
    RawWaker::new(data, waker_vtable())
}

unsafe fn wake_raw(data: *const ()) {
    let handle: Arc<WakerHandle> = Arc::from_raw(data as *const _);
    handle.wake_count.fetch_add(1, Ordering::Relaxed);
    handle.drop_count.fetch_add(1, Ordering::Relaxed);
}

unsafe fn wake_by_ref_raw(data: *const ()) {
    let handle: Arc<WakerHandle> = Arc::from_raw(data as *const _);
    handle.wake_count.fetch_add(1, Ordering::Relaxed);
    forget(handle)
}

unsafe fn drop_raw(data: *const ()) {
    let handle: Arc<WakerHandle> = Arc::from_raw(data as *const _);
    handle.drop_count.fetch_add(1, Ordering::Relaxed);
    drop(handle)
}
