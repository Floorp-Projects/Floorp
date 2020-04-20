#![deny(safe_packed_borrows)]

// Refs: https://github.com/rust-lang/rust/issues/46043

#[repr(packed)]
struct A {
    field: u32,
}

#[repr(packed(2))]
struct B {
    field: u32,
}

fn main() {
    let a = A { field: 1 };
    &a.field; //~ ERROR borrow of packed field is unsafe and requires unsafe function or block

    let b = B { field: 1 };
    &b.field; //~ ERROR borrow of packed field is unsafe and requires unsafe function or block
}
