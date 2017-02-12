// bindgen-unstable

enum {
  foo = 4,
  bar = 8,
};

typedef unsigned long long EasyToOverflow;
const EasyToOverflow k = 0x80000000;

const EasyToOverflow k_expr = 1ULL << 60;

const long long BAZ = (1 << foo) | bar;
const double fuzz = (1 + 50.0f);
const char BAZZ = '5';
const char WAT = '\0';

const char* bytestring = "Foo";
const char* NOT_UTF8 = "\xf0\x28\x8c\x28";
