use core::alloc::Layout;
use core::mem;

/// Aborts the process.
///
/// To abort, this function simply panics while panicking.
pub(crate) fn abort() -> ! {
    struct Panic;

    impl Drop for Panic {
        fn drop(&mut self) {
            panic!("aborting the process");
        }
    }

    let _panic = Panic;
    panic!("aborting the process");
}

/// Calls a function and aborts if it panics.
///
/// This is useful in unsafe code where we can't recover from panics.
#[inline]
pub(crate) fn abort_on_panic<T>(f: impl FnOnce() -> T) -> T {
    struct Bomb;

    impl Drop for Bomb {
        fn drop(&mut self) {
            abort();
        }
    }

    let bomb = Bomb;
    let t = f();
    mem::forget(bomb);
    t
}

/// Returns the layout for `a` followed by `b` and the offset of `b`.
///
/// This function was adapted from the currently unstable `Layout::extend()`:
/// https://doc.rust-lang.org/nightly/std/alloc/struct.Layout.html#method.extend
#[inline]
pub(crate) fn extend(a: Layout, b: Layout) -> (Layout, usize) {
    let new_align = a.align().max(b.align());
    let pad = padding_needed_for(a, b.align());

    let offset = a.size().checked_add(pad).unwrap();
    let new_size = offset.checked_add(b.size()).unwrap();

    let layout = Layout::from_size_align(new_size, new_align).unwrap();
    (layout, offset)
}

/// Returns the padding after `layout` that aligns the following address to `align`.
///
/// This function was adapted from the currently unstable `Layout::padding_needed_for()`:
/// https://doc.rust-lang.org/nightly/std/alloc/struct.Layout.html#method.padding_needed_for
#[inline]
pub(crate) fn padding_needed_for(layout: Layout, align: usize) -> usize {
    let len = layout.size();
    let len_rounded_up = len.wrapping_add(align).wrapping_sub(1) & !align.wrapping_sub(1);
    len_rounded_up.wrapping_sub(len)
}
