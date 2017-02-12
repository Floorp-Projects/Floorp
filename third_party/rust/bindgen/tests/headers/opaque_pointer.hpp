
/**
 * <div rustbindgen opaque></div>
 */
struct OtherOpaque {
    int c;
};

/**
 * <div rustbindgen opaque></div>
 */
template <typename T>
struct Opaque {
    T whatever;
};

struct WithOpaquePtr {
    Opaque<int>* whatever;
    Opaque<float> other;
    OtherOpaque t;
};

