struct LittleArray {
    int a[32];
};

struct BigArray{
    int a[33];
};

struct WithLittleArray {
    struct LittleArray a;
};

struct WithBigArray {
    struct BigArray a;
};
