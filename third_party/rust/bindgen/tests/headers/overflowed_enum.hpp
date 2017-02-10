// bindgen-flags: -- -std=c++11 -Wno-narrowing

enum Foo {
    BAP_ARM = 0x93fcb9,
    BAP_X86 = 0xb67eed,
    BAP_X86_64 = 0xba7b274f,
};

enum Bar: unsigned short {
    One = 1,
    Big = 65538,
};
