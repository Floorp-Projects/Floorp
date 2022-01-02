use std::default::Default;

// demonstrates "automagical" elf32/64 switches via cfg on arch and pub use hacks.
// SIZEOF_* will change depending on whether it's an x86_64 system or 32-bit x86, or really any cfg you can think of.
// similarly the printers will be different, since they have different impls. #typepuns4life

#[cfg(target_pointer_width = "64")]
pub use goblin::elf64 as elf;

#[cfg(target_pointer_width = "32")]
pub use goblin::elf32 as elf;

#[cfg(any(target_pointer_width = "64", target_pointer_width = "32"))]
use crate::elf::{header, sym};

#[cfg(any(target_pointer_width = "64", target_pointer_width = "32"))]
fn main() {
    let header: header::Header = Default::default();
    let sym: sym::Sym = Default::default();
    println!("header: {:?}, sym: {:?}", header, sym);
    println!("sizeof header: {}", header::SIZEOF_EHDR);
    println!("sizeof sym: {}", sym::SIZEOF_SYM);
}
