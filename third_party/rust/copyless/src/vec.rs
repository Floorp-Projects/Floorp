use std::ptr;

/// A typesafe helper that separates new value construction from
/// vector growing, allowing LLVM to ideally construct the element in place.
pub struct VecAllocation<'a, T: 'a> {
    vec: &'a mut Vec<T>,
    index: usize,
}

impl<'a, T> VecAllocation<'a, T> {
    /// Consumes self and writes the given value into the allocation.
    // writing is safe because alloc() ensured enough capacity
    // and `Allocation` holds a mutable borrow to prevent anyone else
    // from breaking this invariant.
    #[inline(always)]
    pub fn init(self, value: T) -> usize {
        unsafe {
            ptr::write(self.vec.as_mut_ptr().add(self.index), value);
            self.vec.set_len(self.index + 1);
        }
        self.index
    }
}

/// An entry into a vector, similar to `std::collections::hash_map::Entry`.
pub enum VecEntry<'a, T: 'a> {
    /// Entry has just been freshly allocated.
    Vacant(VecAllocation<'a, T>),
    /// Existing entry.
    Occupied(&'a mut T),
}

impl<'a, T> VecEntry<'a, T> {
    /// Sets the value for this entry.
    #[inline(always)]
    pub fn set(self, value: T) {
        match self {
            VecEntry::Vacant(alloc) => { alloc.init(value); }
            VecEntry::Occupied(slot) => { *slot = value; }
        }
    }
}

/// Helper trait for a `Vec` type that allocates up-front.
pub trait VecHelper<T> {
    /// Grows the vector by a single entry, returning the allocation.
    fn alloc(&mut self) -> VecAllocation<T>;
    /// Either returns an existing element, or grows the vector by one.
    /// Doesn't expect indices to be higher than the current length.
    fn entry(&mut self, index: usize) -> VecEntry<T>;
}

impl<T> VecHelper<T> for Vec<T> {
    fn alloc(&mut self) -> VecAllocation<T> {
        let index = self.len();
        if self.capacity() == index {
            self.reserve(1);
        }
        VecAllocation {
            vec: self,
            index,
        }
    }

    fn entry(&mut self, index: usize) -> VecEntry<T> {
        if index < self.len() {
            VecEntry::Occupied(unsafe {
                self.get_unchecked_mut(index)
            })
        } else {
            assert_eq!(index, self.len());
            VecEntry::Vacant(self.alloc())
        }
    }
}
