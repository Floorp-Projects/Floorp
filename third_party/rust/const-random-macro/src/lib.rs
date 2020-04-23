extern crate proc_macro;

use getrandom;
use proc_macro::TokenStream;
use proc_macro_hack::proc_macro_hack;
use std::mem;

// Ideally we would use the proper interface for this through the rand crate,
// but due to https://github.com/rust-lang/cargo/issues/5730 this leads to
// issues for no_std crates that try to use rand themselves. So instead we skip
// rand and generate random bytes straight from the OS.
fn gen_random<T>() -> T {
    let mut out = [0u8; 16];
    getrandom::getrandom(&mut out).unwrap();
    unsafe { mem::transmute_copy(&out) }
}

#[proc_macro_hack]
pub fn const_random(input: TokenStream) -> TokenStream {
    match &input.to_string()[..] {
        "u8" => format!("0x{:x}", gen_random::<u8>()).parse().unwrap(),
        "u16" => format!("0x{:x}", gen_random::<u16>()).parse().unwrap(),
        "u32" => format!("0x{:x}", gen_random::<u32>()).parse().unwrap(),
        "u64" => format!("0x{:x}", gen_random::<u64>()).parse().unwrap(),
        "u128" => format!("0x{:x}", gen_random::<u128>()).parse().unwrap(),
        "i8" => format!("0x{:x}", gen_random::<i8>()).parse().unwrap(),
        "i16" => format!("0x{:x}", gen_random::<i16>()).parse().unwrap(),
        "i32" => format!("0x{:x}", gen_random::<i32>()).parse().unwrap(),
        "i64" => format!("0x{:x}", gen_random::<i64>()).parse().unwrap(),
        "i128" => format!("0x{:x}", gen_random::<i128>()).parse().unwrap(),
        _ => panic!("Invalid integer type"),
    }
}
