#![forbid(safe_packed_borrows)]
#![allow(unaligned_references)]

// This lint was removed in https://github.com/rust-lang/rust/pull/82525 (nightly-2021-03-28).
// Refs:
// - https://github.com/rust-lang/rust/pull/82525
// - https://github.com/rust-lang/rust/issues/46043

#[repr(packed)]
struct Packed {
    f: u32,
}

#[repr(packed(2))]
struct PackedN {
    f: u32,
}

fn main() {
    let a = Packed { f: 1 };
    &a.f; //~ ERROR borrow of packed field is unsafe and requires unsafe function or block
    let _ = &a.f; //~ ERROR borrow of packed field is unsafe and requires unsafe function or block

    let b = PackedN { f: 1 };
    &b.f; //~ ERROR borrow of packed field is unsafe and requires unsafe function or block
    let _ = &b.f; //~ ERROR borrow of packed field is unsafe and requires unsafe function or block
}
