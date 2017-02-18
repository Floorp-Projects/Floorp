// bindgen-flags: --no-derive-default --blacklist-type foo --raw-line "#[repr(C)] #[derive(Copy, Clone, Debug)] pub struct foo { bar: ::std::os::raw::c_int, }"

struct foo {
  int bar;
};

/**
 * bar should compile. It will normally derive default, but our blacklist of foo
 * and replacement for another type that doesn't implement it would prevent it
 * from building if --no-derive-default didn't work.
 */
struct bar {
  struct foo foo;
  int baz;
};
