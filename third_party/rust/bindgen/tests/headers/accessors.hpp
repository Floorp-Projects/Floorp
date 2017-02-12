struct SomeAccessors {
    int mNoAccessor;
    /** <div rustbindgen accessor></div> */
    int mBothAccessors;
    /** <div rustbindgen accessor="unsafe"></div> */
    int mUnsafeAccessors;
    /** <div rustbindgen accessor="immutable"></div> */
    int mImmutableAccessor;
};

/** <div rustbindgen accessor></div> */
struct AllAccessors {
    int mBothAccessors;
    int mAlsoBothAccessors;
};

/** <div rustbindgen accessor="unsafe"></div> */
struct AllUnsafeAccessors {
    int mBothAccessors;
    int mAlsoBothAccessors;
};

/** <div rustbindgen accessor></div> */
struct ContradictAccessors {
    int mBothAccessors;
    /** <div rustbindgen accessor="false"></div> */
    int mNoAccessors;
    /** <div rustbindgen accessor="unsafe"></div> */
    int mUnsafeAccessors;
    /** <div rustbindgen accessor="immutable"></div> */
    int mImmutableAccessor;
};

/** <div rustbindgen accessor replaces="Replaced"></div> */
struct Replacing {
    int mAccessor;
};

struct Replaced {
    int noOp;
};

/** <div rustbindgen accessor></div> */
struct Wrapper {
    Replaced mReplaced;
};
