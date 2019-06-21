//! This module roughly corresponds to `mach/vm_page_size.h`

use vm_types::{vm_size_t, mach_vm_size_t, mach_vm_offset_t};

extern "C" {
    pub static vm_page_size: vm_size_t;
    pub static vm_page_mask: vm_size_t;
    pub static vm_page_shift: ::libc::c_int;
}

pub unsafe fn mach_vm_trunc_page(x: mach_vm_offset_t) -> mach_vm_offset_t {
    (x & !(vm_page_mask as mach_vm_size_t))
}

pub unsafe fn mach_vm_round_page(x: mach_vm_offset_t) -> mach_vm_offset_t {
    ((x + vm_page_mask as mach_vm_size_t) & !(vm_page_mask as mach_vm_size_t))
}

#[test]
fn page_size_test() {
  unsafe {
    assert!(vm_page_size > 0);
    assert!(vm_page_size % 2 == 0);
    assert_eq!(mach_vm_round_page(1), vm_page_size as mach_vm_size_t);
    println!("{:?}", vm_page_size);
  }
}
