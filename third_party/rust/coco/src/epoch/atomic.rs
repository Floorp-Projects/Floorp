use std::mem;
use std::ptr;
use std::marker::PhantomData;
use std::sync::atomic::AtomicUsize;
use std::sync::atomic::Ordering::{self, AcqRel, Acquire, Release, SeqCst};

use epoch::Pin;

/// Returns a mask containing unused least significant bits of an aligned pointer to `T`.
fn low_bits<T>() -> usize {
    (1 << mem::align_of::<T>().trailing_zeros()) - 1
}

/// Tags the unused least significant bits of `raw` with `tag`.
///
/// # Panics
///
/// Panics if the tag doesn't fit into the unused bits of the pointer, or if the pointer is
/// unaligned.
fn raw_and_tag<T>(raw: *mut T, tag: usize) -> usize {
    let mask = low_bits::<T>();
    assert!(raw as usize & mask == 0, "unaligned pointer");
    assert!(tag <= mask, "tag too large to fit into the unused bits: {} > {}", tag, mask);
    raw as usize | tag
}

/// A tagged atomic nullable pointer.
///
/// The tag is stored into the unused least significant bits of the pointer. The pointer must be
/// properly aligned.
#[derive(Debug)]
pub struct Atomic<T> {
    data: AtomicUsize,
    _marker: PhantomData<*mut T>, // !Send + !Sync
}

unsafe impl<T: Send + Sync> Send for Atomic<T> {}
unsafe impl<T: Send + Sync> Sync for Atomic<T> {}

impl<T> Atomic<T> {
    /// Constructs a tagged atomic pointer from raw data.
    unsafe fn from_data(data: usize) -> Self {
        Atomic {
            data: AtomicUsize::new(data),
            _marker: PhantomData,
        }
    }

    /// Returns a new, null atomic pointer tagged with `tag`.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of an aligned pointer.
    pub fn null(tag: usize) -> Self {
        unsafe { Self::from_raw(ptr::null_mut(), tag) }
    }

    /// Allocates `data` on the heap and returns a new atomic pointer that points to it and is
    /// tagged with `tag`.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of the pointer, or if the allocated
    /// pointer is unaligned.
    pub fn new(data: T, tag: usize) -> Self {
        unsafe { Self::from_raw(Box::into_raw(Box::new(data)), tag) }
    }

    /// Returns a new atomic pointer initialized with `ptr`.
    pub fn from_ptr(ptr: Ptr<T>) -> Self {
        unsafe { Self::from_data(ptr.data) }
    }

    /// Returns a new atomic pointer initialized with `b` and `tag`.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of the pointer, or if the pointer is
    /// unaligned.
    pub fn from_box(b: Box<T>, tag: usize) -> Self {
        unsafe { Self::from_raw(Box::into_raw(b), tag) }
    }

    /// Returns a new atomic pointer initialized with `raw` and `tag`.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of the pointer, or if the pointer is
    /// unaligned.
    pub unsafe fn from_raw(raw: *mut T, tag: usize) -> Self {
        Self::from_data(raw_and_tag(raw, tag))
    }

    /// Loads the tagged atomic pointer.
    ///
    /// This operation uses the `Acquire` ordering.
    pub fn load<'p>(&self, _: &'p Pin) -> Ptr<'p, T> {
        unsafe { Ptr::from_data(self.data.load(Acquire)) }
    }

    /// Loads the tagged atomic pointer as a raw pointer and a tag.
    ///
    /// Argument `order` describes the memory ordering of this operation.
    pub fn load_raw(&self, order: Ordering) -> (*mut T, usize) {
        let p = unsafe { Ptr::<T>::from_data(self.data.load(order)) };
        (p.as_raw(), p.tag())
    }

    /// Stores `new` tagged with `tag` into the atomic.
    ///
    /// This operation uses the `Release` ordering.
    pub fn store<'p>(&self, new: Ptr<'p, T>) {
        self.data.store(new.data, Release);
    }

    /// Stores `new` tagged with `tag` into the atomic and returns it.
    ///
    /// This operation uses the `Release` ordering.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of the pointer, or if the pointer is
    /// unaligned.
    pub fn store_box<'p>(&self, new: Box<T>, tag: usize, _: &'p Pin) -> Ptr<'p, T> {
        let ptr = unsafe { Ptr::from_raw(Box::into_raw(new), tag) };
        self.data.store(ptr.data, Release);
        ptr
    }

    /// Stores `new` tagged with `tag` into the atomic.
    ///
    /// Argument `order` describes the memory ordering of this operation.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of the pointer, or if the pointer is
    /// unaligned.
    pub unsafe fn store_raw<'p>(
        &self,
        new: *mut T,
        tag: usize,
        order: Ordering,
        _: &'p Pin,
    ) -> Ptr<'p, T> {
        let ptr = Ptr::from_raw(new, tag);
        self.data.store(ptr.data, order);
        ptr
    }

    /// Stores `new` into the atomic, returning the old tagged pointer.
    ///
    /// This operation uses the `AcqRel` ordering.
    pub fn swap<'p>(&self, new: Ptr<'p, T>) -> Ptr<'p, T> {
        unsafe { Ptr::from_data(self.data.swap(new.data, AcqRel)) }
    }

    /// Stores `new` tagged with `tag` into the atomic, returning the old tagged pointer.
    ///
    /// This operation uses the `AcqRel` ordering.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of the pointer, or if the pointer is
    /// unaligned.
    pub fn swap_box<'p>(&self, new: Box<T>, tag: usize, _: &'p Pin) -> Ptr<'p, T> {
        let data = unsafe { Ptr::from_raw(Box::into_raw(new), tag).data };
        unsafe { Ptr::from_data(self.data.swap(data, AcqRel)) }
    }

    /// Stores `new` tagged with `tag` into the atomic, returning the old tagged pointer.
    ///
    /// Argument `order` describes the memory ordering of this operation.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of the pointer, or if the pointer is
    /// unaligned.
    pub unsafe fn swap_raw<'p>(&self, new: *mut T, tag: usize, order: Ordering) -> Ptr<'p, T> {
        let data = Ptr::from_raw(new, tag).data;
        Ptr::from_data(self.data.swap(data, order))
    }

    /// If the tagged atomic pointer is equal to `current`, stores `new`.
    ///
    /// The return value is a result indicating whether the new pointer was stored. On failure the
    /// current value of the tagged atomic pointer is returned.
    ///
    /// This operation uses the `AcqRel` ordering.
    pub fn cas<'p>(
        &self,
        current: Ptr<'p, T>,
        new: Ptr<'p, T>,
    ) -> Result<(), Ptr<'p, T>> {
        let previous = self.data.compare_and_swap(current.data, new.data, AcqRel);
        if previous == current.data {
            Ok(())
        } else {
            unsafe { Err(Ptr::from_data(previous)) }
        }
    }

    /// If the tagged atomic pointer is equal to `current`, stores `new`.
    ///
    /// The return value is a result indicating whether the new pointer was stored. On failure the
    /// current value of the tagged atomic pointer is returned.
    ///
    /// This operation uses the `SeqCst` ordering.
    pub fn cas_sc<'p>(&self, current: Ptr<'p, T>, new: Ptr<'p, T>) -> Result<(), Ptr<'p, T>> {
        let previous = self.data.compare_and_swap(current.data, new.data, SeqCst);
        if previous == current.data {
            Ok(())
        } else {
            unsafe { Err(Ptr::from_data(previous)) }
        }
    }

    /// If the tagged atomic pointer is equal to `current`, stores `new`.
    ///
    /// The return value is a result indicating whether the new pointer was stored. On failure the
    /// current value of the tagged atomic pointer is returned.
    ///
    /// This method can sometimes spuriously fail even when comparison succeeds, which can result
    /// in more efficient code on some platforms.
    ///
    /// This operation uses the `AcqRel` ordering.
    pub fn cas_weak<'p>(&self, current: Ptr<'p, T>, new: Ptr<'p, T>) -> Result<(), Ptr<'p, T>> {
        match self.data.compare_exchange_weak(current.data, new.data, AcqRel, Acquire) {
            Ok(_) => Ok(()),
            Err(previous) => unsafe { Err(Ptr::from_data(previous)) },
        }
    }

    /// If the tagged atomic pointer is equal to `current`, stores `new`.
    ///
    /// The return value is a result indicating whether the new pointer was stored. On failure the
    /// current value of the tagged atomic pointer is returned.
    ///
    /// This method can sometimes spuriously fail even when comparison succeeds, which can result
    /// in more efficient code on some platforms.
    ///
    /// This operation uses the `SeqCst` ordering.
    pub fn cas_weak_sc<'p>(
        &self,
        current: Ptr<'p, T>,
        new: Ptr<'p, T>,
    ) -> Result<(), Ptr<'p, T>> {
        match self.data.compare_exchange_weak(current.data, new.data, SeqCst, SeqCst) {
            Ok(_) => Ok(()),
            Err(previous) => unsafe { Err(Ptr::from_data(previous)) },
        }
    }

    /// If the tagged atomic pointer is equal to `current`, stores `new` tagged with `tag`.
    ///
    /// The return value is a result indicating whether the new pointer was stored. On success the
    /// new pointer is returned. On failure the current value of the tagged atomic pointer and
    /// `new` are returned.
    ///
    /// This operation uses the `AcqRel` ordering.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of the pointer, or if the pointer is
    /// unaligned.
    pub fn cas_box<'p>(
        &self,
        current: Ptr<'p, T>,
        mut new: Box<T>,
        tag: usize,
    ) -> Result<Ptr<'p, T>, (Ptr<'p, T>, Box<T>)> {
        let new_data = raw_and_tag(new.as_mut(), tag);
        let previous = self.data.compare_and_swap(current.data, new_data, AcqRel);
        if previous == current.data {
            mem::forget(new);
            unsafe { Ok(Ptr::from_data(new_data)) }
        } else {
            unsafe { Err((Ptr::from_data(previous), new)) }
        }
    }

    /// If the tagged atomic pointer is equal to `current`, stores `new` tagged with `tag`.
    ///
    /// The return value is a result indicating whether the new pointer was stored. On success the
    /// new pointer is returned. On failure the current value of the tagged atomic pointer and
    /// `new` are returned.
    ///
    /// This operation uses the `SeqCst` ordering.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of the pointer, or if the pointer is
    /// unaligned.
    pub fn cas_box_sc<'p>(
        &self,
        current: Ptr<'p, T>,
        mut new: Box<T>,
        tag: usize,
    ) -> Result<Ptr<'p, T>, (Ptr<'p, T>, Box<T>)> {
        let new_data = raw_and_tag(new.as_mut(), tag);
        let previous = self.data.compare_and_swap(current.data, new_data, SeqCst);
        if previous == current.data {
            mem::forget(new);
            unsafe { Ok(Ptr::from_data(new_data)) }
        } else {
            unsafe { Err((Ptr::from_data(previous), new)) }
        }
    }

    /// If the tagged atomic pointer is equal to `current`, stores `new` tagged with `tag`.
    ///
    /// The return value is a result indicating whether the new pointer was stored. On success the
    /// new pointer is returned. On failure the current value of the tagged atomic pointer and
    /// `new` are returned.
    ///
    /// This method can sometimes spuriously fail even when comparison succeeds, which can result
    /// in more efficient code on some platforms.
    ///
    /// This operation uses the `AcqRel` ordering.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of the pointer, or if the pointer is
    /// unaligned.
    pub fn cas_box_weak<'p>(
        &self,
        current: Ptr<'p, T>,
        mut new: Box<T>,
        tag: usize
    ) -> Result<Ptr<'p, T>, (Ptr<'p, T>, Box<T>)> {
        let new_data = raw_and_tag(new.as_mut(), tag);
        match self.data.compare_exchange_weak(current.data, new_data, AcqRel, Acquire) {
            Ok(_) => {
                mem::forget(new);
                unsafe { Ok(Ptr::from_data(new_data)) }
            }
            Err(previous) => unsafe { Err((Ptr::from_data(previous), new)) },
        }
    }

    /// If the tagged atomic pointer is equal to `current`, stores `new` tagged with `tag`.
    ///
    /// The return value is a result indicating whether the new pointer was stored. On success the
    /// new pointer is returned. On failure the current value of the tagged atomic pointer and
    /// `new` are returned.
    ///
    /// This method can sometimes spuriously fail even when comparison succeeds, which can result
    /// in more efficient code on some platforms.
    ///
    /// This operation uses the `AcqRel` ordering.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of the pointer, or if the pointer is
    /// unaligned.
    pub fn cas_box_weak_sc<'p>(
        &self,
        current: Ptr<'p, T>,
        mut new: Box<T>,
        tag: usize,
    ) -> Result<Ptr<'p, T>, (Ptr<'p, T>, Box<T>)> {
        let new_data = raw_and_tag(new.as_mut(), tag);
        match self.data.compare_exchange_weak(current.data, new_data, SeqCst, SeqCst) {
            Ok(_) => {
                mem::forget(new);
                unsafe { Ok(Ptr::from_data(new_data)) }
            }
            Err(previous) => unsafe { Err((Ptr::from_data(previous), new)) },
        }
    }

    /// If the tagged atomic pointer is equal to `current`, stores `new`.
    ///
    /// The return value is a result indicating whether the new pointer was stored. On failure the
    /// current value of the tagged atomic pointer is returned.
    ///
    /// Argument `order` describes the memory ordering of this operation.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of the pointer, or if the pointer is
    /// unaligned.
    pub unsafe fn cas_raw(
        &self,
        current: (*mut T, usize),
        new: (*mut T, usize),
        order: Ordering,
    ) -> Result<(), (*mut T, usize)> {
        let current_data = raw_and_tag(current.0, current.1);
        let new_data = raw_and_tag(new.0, new.1);
        let previous = self.data.compare_and_swap(current_data, new_data, order);
        if previous == current_data {
            Ok(())
        } else {
            let ptr = Ptr::from_data(previous);
            Err((ptr.as_raw(), ptr.tag()))
        }
    }

    /// If the tagged atomic pointer is equal to `current`, stores `new`.
    ///
    /// The return value is a result indicating whether the new pointer was stored. On failure the
    /// current value of the tagged atomic pointer is returned.
    ///
    /// This method can sometimes spuriously fail even when comparison succeeds, which can result
    /// in more efficient code on some platforms.
    ///
    /// Argument `order` describes the memory ordering of this operation.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of the pointer, or if the pointer is
    /// unaligned.
    pub unsafe fn cas_raw_weak(
        &self,
        current: (*mut T, usize),
        new: (*mut T, usize),
        order: Ordering,
    ) -> Result<(), (*mut T, usize)> {
        let current_data = raw_and_tag(current.0, current.1);
        let new_data = raw_and_tag(new.0, new.1);
        let previous = self.data.compare_and_swap(current_data, new_data, order);
        if previous == current_data {
            Ok(())
        } else {
            let ptr = Ptr::from_data(previous);
            Err((ptr.as_raw(), ptr.tag()))
        }
    }
}

impl<T> Default for Atomic<T> {
    fn default() -> Self {
        Atomic {
            data: AtomicUsize::new(0),
            _marker: PhantomData,
        }
    }
}

/// A tagged nullable pointer.
#[derive(Debug)]
pub struct Ptr<'p, T: 'p> {
    data: usize,
    _marker: PhantomData<(*mut T, &'p T)>, // !Send + !Sync
}

impl<'a, T> Clone for Ptr<'a, T> {
    fn clone(&self) -> Self {
        Ptr {
            data: self.data,
            _marker: PhantomData,
        }
    }
}

impl<'a, T> Copy for Ptr<'a, T> {}

impl<'p, T: 'p> Ptr<'p, T> {
    /// Constructs a nullable pointer from raw data.
    unsafe fn from_data(data: usize) -> Self {
        Ptr {
            data: data,
            _marker: PhantomData,
        }
    }

    /// Returns a null pointer with a tag.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of an aligned pointer.
    pub fn null(tag: usize) -> Self {
        unsafe { Self::from_data(raw_and_tag::<T>(ptr::null_mut(), tag)) }
    }

    /// Constructs a tagged pointer from a raw pointer and tag.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of the pointer, or if the pointer is
    /// unaligned.
    pub unsafe fn from_raw(raw: *mut T, tag: usize) -> Self {
        Self::from_data(raw_and_tag(raw, tag))
    }

    /// Returns `true` if the pointer is null.
    pub fn is_null(&self) -> bool {
        self.as_raw().is_null()
    }

    /// Converts the pointer to a reference.
    pub fn as_ref(&self) -> Option<&'p T> {
        unsafe { self.as_raw().as_ref() }
    }

    /// Converts the pointer to a raw pointer.
    pub fn as_raw(&self) -> *mut T {
        (self.data & !low_bits::<T>()) as *mut T
    }

    /// Returns a reference to the pointing object.
    ///
    /// # Panics
    ///
    /// Panics if the pointer is null.
    pub fn unwrap(&self) -> &'p T {
        self.as_ref().unwrap()
    }

    /// Returns the tag.
    pub fn tag(&self) -> usize {
        self.data & low_bits::<T>()
    }

    /// Constructs a new tagged pointer with a different tag.
    ///
    /// # Panics
    ///
    /// Panics if the tag doesn't fit into the unused bits of the pointer.
    pub fn with_tag(&self, tag: usize) -> Self {
        unsafe { Self::from_raw(self.as_raw(), tag) }
    }
}

impl<'p, T> Default for Ptr<'p, T> {
    fn default() -> Self {
        Ptr {
            data: 0,
            _marker: PhantomData,
        }
    }
}
