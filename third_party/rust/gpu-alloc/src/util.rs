use alloc::sync::Arc;

/// Guarantees uniqueness only if `Weak` pointers are never created
/// from this `Arc` or clones.
pub(crate) fn is_arc_unique<M>(arc: &mut Arc<M>) -> bool {
    let strong_count = Arc::strong_count(&*arc);
    debug_assert_ne!(strong_count, 0, "This Arc should exist");

    debug_assert!(
        strong_count > 1 || Arc::get_mut(arc).is_some(),
        "`Weak` pointer exists"
    );

    strong_count == 1
}

/// Can be used instead of `Arc::try_unwrap(arc).unwrap()`
/// when it is guaranteed to succeed.
pub(crate) unsafe fn arc_unwrap<M>(mut arc: Arc<M>) -> M {
    use core::{mem::ManuallyDrop, ptr::read};
    debug_assert!(is_arc_unique(&mut arc));

    // Get raw pointer to inner value.
    let raw = Arc::into_raw(arc);

    // As `Arc` is unique and no Weak pointers exist
    // it won't be dereferenced elsewhere.
    let inner = read(raw);

    // Cast to `ManuallyDrop` which guarantees to have same layout
    // and will skip dropping.
    drop(Arc::from_raw(raw as *const ManuallyDrop<M>));
    inner
}

/// Can be used instead of `Arc::try_unwrap`
/// only if `Weak` pointers are never created from this `Arc` or clones.
pub(crate) unsafe fn try_arc_unwrap<M>(mut arc: Arc<M>) -> Option<M> {
    if is_arc_unique(&mut arc) {
        Some(arc_unwrap(arc))
    } else {
        None
    }
}
