// bindgen-flags: --no-unstable-rust
union foo {
    int a;
    struct {
        int b : 7;
        int c : 25;
    };
};
