// bindgen-flags: --no-convert-floats

struct foo {
  float bar, baz;
  double bazz;
  long double* bazzz;
  float _Complex complexFloat;
  double _Complex complexDouble;
};
