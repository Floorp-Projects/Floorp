use core::marker::PhantomData;
use core::mem::{self, transmute};
use core::ptr::{drop_in_place, read, NonNull};

extern crate alloc;

use alloc::alloc::{dealloc, Layout};

// Self referential structs are currently not supported with safe vanilla Rust.
// The only reasonable safe alternative is to expect the user to juggle 2 separate
// data structures which is a mess. The library solution rental is both no longer
// maintained and really heavy to compile. So begrudgingly I rolled my own version.
// These are some of the core invariants we require for this to be safe to use.
//
// 1. owner is initialized when UnsafeSelfCell is constructed.
// 2. owner is NEVER changed again.
// 3. The pointer to owner and dependent never changes, even when moved.
// 4. The only access to owner and dependent is as immutable reference.
// 5. owner lives longer than dependent.

#[doc(hidden)]
pub struct JoinedCell<Owner, Dependent> {
    pub owner: Owner,
    pub dependent: Dependent,
}

// Library controlled struct that marks all accesses as unsafe.
// Because the macro generated struct impl can be extended, could be unsafe.
#[doc(hidden)]
pub struct UnsafeSelfCell<ContainedIn, Owner, DependentStatic: 'static> {
    joined_void_ptr: NonNull<u8>,

    // ContainedIn is necessary for type safety since we don't fully
    // prohibit access to the UnsafeSelfCell; swapping between different
    // structs can be unsafe otherwise, see Issue #17.
    contained_in_marker: PhantomData<ContainedIn>,

    owner_marker: PhantomData<Owner>,
    // DependentStatic is only used to correctly derive Send and Sync.
    dependent_marker: PhantomData<DependentStatic>,
}

impl<ContainedIn, Owner, DependentStatic> UnsafeSelfCell<ContainedIn, Owner, DependentStatic> {
    pub unsafe fn new(joined_void_ptr: NonNull<u8>) -> Self {
        Self {
            joined_void_ptr,
            contained_in_marker: PhantomData,
            owner_marker: PhantomData,
            dependent_marker: PhantomData,
        }
    }

    // Calling any of these *unsafe* functions with the wrong Dependent type is UB.

    pub unsafe fn borrow_owner<'a, Dependent>(&'a self) -> &'a Owner {
        let joined_ptr =
            transmute::<NonNull<u8>, NonNull<JoinedCell<Owner, Dependent>>>(self.joined_void_ptr);

        &(*joined_ptr.as_ptr()).owner
    }

    pub unsafe fn borrow_dependent<'a, Dependent>(&'a self) -> &'a Dependent {
        let joined_ptr =
            transmute::<NonNull<u8>, NonNull<JoinedCell<Owner, Dependent>>>(self.joined_void_ptr);

        &(*joined_ptr.as_ptr()).dependent
    }

    pub unsafe fn borrow_mut<'a, Dependent>(&'a mut self) -> (&'a Owner, &'a mut Dependent) {
        let joined_ptr =
            transmute::<NonNull<u8>, NonNull<JoinedCell<Owner, Dependent>>>(self.joined_void_ptr);

        // This function used to return `&'a mut JoinedCell<Owner, Dependent>`.
        // It now creates two references to the fields instead to avoid claiming mutable access
        // to the whole `JoinedCell` (including the owner!) here.
        (
            &(*joined_ptr.as_ptr()).owner,
            &mut (*joined_ptr.as_ptr()).dependent,
        )
    }

    // Any subsequent use of this struct other than dropping it is UB.
    pub unsafe fn drop_joined<Dependent>(&mut self) {
        let joined_ptr =
            transmute::<NonNull<u8>, NonNull<JoinedCell<Owner, Dependent>>>(self.joined_void_ptr);

        // Also used in case drop_in_place(...dependent) fails
        let _guard = OwnerAndCellDropGuard { joined_ptr };

        // IMPORTANT dependent must be dropped before owner.
        // We don't want to rely on an implicit order of struct fields.
        // So we drop the struct, field by field manually.
        drop_in_place(&mut (*joined_ptr.as_ptr()).dependent);

        // Dropping owner
        // and deallocating
        // due to _guard at end of scope.
    }

    pub unsafe fn into_owner<Dependent>(self) -> Owner {
        let joined_ptr =
            transmute::<NonNull<u8>, NonNull<JoinedCell<Owner, Dependent>>>(self.joined_void_ptr);

        // In case drop_in_place(...dependent) fails
        let drop_guard = OwnerAndCellDropGuard::new(joined_ptr);

        // Drop dependent
        drop_in_place(&mut (*joined_ptr.as_ptr()).dependent);

        mem::forget(drop_guard);

        let owner_ptr: *const Owner = &(*joined_ptr.as_ptr()).owner;

        // Move owner out so it can be returned.
        // Must not read before dropping dependent!! (Which happened above.)
        let owner = read(owner_ptr);

        // Deallocate JoinedCell
        let layout = Layout::new::<JoinedCell<Owner, Dependent>>();
        dealloc(self.joined_void_ptr.as_ptr(), layout);

        owner
    }
}

unsafe impl<ContainedIn, Owner, DependentStatic> Send
    for UnsafeSelfCell<ContainedIn, Owner, DependentStatic>
where
    // Only derive Send if Owner and DependentStatic is also Send
    Owner: Send,
    DependentStatic: Send,
{
}

unsafe impl<ContainedIn, Owner, DependentStatic> Sync
    for UnsafeSelfCell<ContainedIn, Owner, DependentStatic>
where
    // Only derive Sync if Owner and DependentStatic is also Sync
    Owner: Sync,
    DependentStatic: Sync,
{
}

// This struct is used to safely deallocate only the owner if dependent
// construction fails.
//
// mem::forget it once it's no longer needed or dtor will be UB.
#[doc(hidden)]
pub struct OwnerAndCellDropGuard<Owner, Dependent> {
    joined_ptr: NonNull<JoinedCell<Owner, Dependent>>,
}

impl<Owner, Dependent> OwnerAndCellDropGuard<Owner, Dependent> {
    pub unsafe fn new(joined_ptr: NonNull<JoinedCell<Owner, Dependent>>) -> Self {
        Self { joined_ptr }
    }
}

impl<Owner, Dependent> Drop for OwnerAndCellDropGuard<Owner, Dependent> {
    fn drop(&mut self) {
        struct DeallocGuard {
            ptr: *mut u8,
            layout: Layout,
        }
        impl Drop for DeallocGuard {
            fn drop(&mut self) {
                unsafe { dealloc(self.ptr, self.layout) }
            }
        }

        // Deallocate even when the drop_in_place(...owner) panics
        let _guard = DeallocGuard {
            ptr: unsafe {
                transmute::<*mut JoinedCell<Owner, Dependent>, *mut u8>(self.joined_ptr.as_ptr())
            },
            layout: Layout::new::<JoinedCell<Owner, Dependent>>(),
        };

        unsafe {
            // We must only drop owner and the struct itself,
            // The whole point of this drop guard is to clean up the partially
            // initialized struct should building the dependent fail.
            drop_in_place(&mut (*self.joined_ptr.as_ptr()).owner);
        }

        // Deallocation happens at end of scope
    }
}

// Older versions of rust do not support addr_of_mut!. What we want to do here
// is to emulate the behavior of that macro by going (incorrectly) via a
// reference cast. Technically this is UB, but testing does not show the older
// compiler versions (ab)using this. For discussions about this behavior see
// https://github.com/Voultapher/self_cell/pull/31 and
// https://github.com/Voultapher/self_cell/issues/30 and
// https://github.com/Voultapher/self_cell/pull/33
//
// Because of 'procedural macros cannot expand to macro definitions'
// we have wrap this in functions.
impl<Owner, Dependent> JoinedCell<Owner, Dependent> {
    #[doc(hidden)]
    #[cfg(not(feature = "old_rust"))]
    pub unsafe fn _field_pointers(this: *mut Self) -> (*mut Owner, *mut Dependent) {
        let owner_ptr = core::ptr::addr_of_mut!((*this).owner);
        let dependent_ptr = core::ptr::addr_of_mut!((*this).dependent);

        (owner_ptr, dependent_ptr)
    }

    #[doc(hidden)]
    #[cfg(feature = "old_rust")]
    #[rustversion::since(1.51)]
    pub unsafe fn _field_pointers(this: *mut Self) -> (*mut Owner, *mut Dependent) {
        let owner_ptr = core::ptr::addr_of_mut!((*this).owner);
        let dependent_ptr = core::ptr::addr_of_mut!((*this).dependent);

        (owner_ptr, dependent_ptr)
    }

    #[doc(hidden)]
    #[cfg(feature = "old_rust")]
    #[rustversion::before(1.51)]
    pub unsafe fn _field_pointers(this: *mut Self) -> (*mut Owner, *mut Dependent) {
        // See comment above, technically this is UB.
        let owner_ptr = &mut (*this).owner as *mut Owner;
        let dependent_ptr = &mut (*this).dependent as *mut Dependent;

        (owner_ptr, dependent_ptr)
    }
}
