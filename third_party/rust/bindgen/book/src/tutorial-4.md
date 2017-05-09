# Include the Generated Bindings in `src/lib.rs`

We can use the `include!` macro to dump our generated bindings right into our
crate's main entry point, `src/lib.rs`:

```rust,ignore
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
```

Because `bzip2`'s symbols do not follow Rust's style conventions, we suppress a
bunch of warnings with a few `#![allow(...)]` pragmas.

We can run `cargo build` again to check that the bindings themselves compile:

```bash
$ cargo build
   Compiling libbindgen-tutorial-bzip2-sys v0.1.0
    Finished debug [unoptimized + debuginfo] target(s) in 62.8 secs
```

And we can run `cargo test` to verify that the layout, size, and alignment of
our generated Rust FFI structs match what `bindgen` thinks they should be:

```bash
$ cargo test
   Compiling libbindgen-tutorial-bzip2-sys v0.1.0
    Finished debug [unoptimized + debuginfo] target(s) in 0.0 secs
     Running target/debug/deps/bzip2_sys-10413fc2af207810

running 14 tests
test bindgen_test_layout___darwin_pthread_handler_rec ... ok
test bindgen_test_layout___sFILE ... ok
test bindgen_test_layout___sbuf ... ok
test bindgen_test_layout__bindgen_ty_1 ... ok
test bindgen_test_layout__bindgen_ty_2 ... ok
test bindgen_test_layout__opaque_pthread_attr_t ... ok
test bindgen_test_layout__opaque_pthread_cond_t ... ok
test bindgen_test_layout__opaque_pthread_mutex_t ... ok
test bindgen_test_layout__opaque_pthread_condattr_t ... ok
test bindgen_test_layout__opaque_pthread_mutexattr_t ... ok
test bindgen_test_layout__opaque_pthread_once_t ... ok
test bindgen_test_layout__opaque_pthread_rwlock_t ... ok
test bindgen_test_layout__opaque_pthread_rwlockattr_t ... ok
test bindgen_test_layout__opaque_pthread_t ... ok

test result: ok. 14 passed; 0 failed; 0 ignored; 0 measured

   Doc-tests libbindgen-tutorial-bzip2-sys

running 0 tests

test result: ok. 0 passed; 0 failed; 0 ignored; 0 measured
```
