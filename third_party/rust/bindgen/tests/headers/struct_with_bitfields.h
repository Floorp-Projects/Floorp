// bindgen-flags: --no-unstable-rust
struct bitfield {
    unsigned short
    a   :1,
    b   :1,
    c   :1,
        :1,
        :2,
    d   :2;
    int e;
    unsigned int f : 2;
    unsigned int g : 32;
};
