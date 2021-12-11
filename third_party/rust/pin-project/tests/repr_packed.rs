#![warn(rust_2018_idioms, single_use_lifetimes)]
// unaligned_references did not exist in older compilers and safe_packed_borrows was removed in the latest compilers.
// https://github.com/rust-lang/rust/pull/82525
#![allow(unknown_lints, renamed_and_removed_lints)]
#![forbid(unaligned_references, safe_packed_borrows)]

use std::cell::Cell;

// Ensure that the compiler doesn't copy the fields
// of #[repr(packed)] types during drop, if the field has alignment 1
// (that is, any reference to the field is guaranteed to have proper alignment)
// We are currently unable to statically prevent the usage of #[pin_project]
// on #[repr(packed)] types composed entirely of fields of alignment 1.
// This shouldn't lead to undefined behavior, as long as the compiler doesn't
// try to move the field anyway during drop.
//
// This tests validates that the compiler is doing what we expect.
#[test]
fn weird_repr_packed() {
    // We keep track of the field address during
    // drop using a thread local, to avoid changing
    // the layout of our #[repr(packed)] type.
    thread_local! {
        static FIELD_ADDR: Cell<usize> = Cell::new(0);
    }

    #[repr(packed)]
    struct Foo {
        field: u8,
    }

    impl Drop for Foo {
        fn drop(&mut self) {
            FIELD_ADDR.with(|f| {
                f.set(&self.field as *const u8 as usize);
            })
        }
    }

    #[allow(clippy::let_and_return)]
    let field_addr = {
        // We let this field drop by going out of scope,
        // rather than explicitly calling drop(foo).
        // Calling drop(foo) causes 'foo' to be moved
        // into the 'drop' function, resulting in a different
        // address.
        let x = Foo { field: 27 };
        let field_addr = &x.field as *const u8 as usize;
        field_addr
    };
    assert_eq!(field_addr, FIELD_ADDR.with(|f| f.get()));
}
