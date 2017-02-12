// FIXME: libclang < 3.9 does not expose `__float128` in its interface, so this
// test will fail. Once we remove support for `--features llvm_stable` and
// require libclang >= 3.9, we can reenable this test.
//
// static __float128 global = 1.0;

// FIXME: We have no way to get 128 bit aligned structs in Rust at the moment,
// and therefore the generated layout tests for this struct will fail. When we
// can enforce 128 bit alignment, we can re-enable this test.
//
// struct A {
//     __float128 f;
// };
