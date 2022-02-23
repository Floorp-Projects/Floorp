#![forbid(unaligned_references)]
#![allow(safe_packed_borrows)]

// Refs: https://github.com/rust-lang/rust/issues/82523

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
    &a.f; //~ ERROR reference to packed field is unaligned
    let _ = &a.f; //~ ERROR reference to packed field is unaligned

    let b = PackedN { f: 1 };
    &b.f; //~ ERROR reference to packed field is unaligned
    let _ = &b.f; //~ ERROR reference to packed field is unaligned
}
