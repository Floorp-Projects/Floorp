// bindgen-flags: --ctypes-prefix "libc" --use-core --raw-line "#![no_std]" --raw-line "mod libc { pub type c_int = i32; pub enum c_void {} }"
struct foo {
  int a, b;
  void* bar;
};
