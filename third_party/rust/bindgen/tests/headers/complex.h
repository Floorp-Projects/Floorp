
#define COMPLEX_TEST(ty_, name_)  \
  struct Test##name_ {            \
    ty_ _Complex mMember;         \
                                  \
  };                              \
  struct Test##name_##Ptr {       \
    ty_ _Complex* mMember;        \
  };

COMPLEX_TEST(double, Double)
COMPLEX_TEST(float, Float)

// FIXME: 128-byte-aligned in some machines
// which we can't support right now in Rust.
// COMPLEX_TEST(long double, LongDouble)
