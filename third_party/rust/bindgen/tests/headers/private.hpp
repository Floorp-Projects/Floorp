
struct HasPrivate {
    int mNotPrivate;
    /** <div rustbindgen private></div> */
    int mIsPrivate;
};


/** <div rustbindgen private></div> */
struct VeryPrivate {
    int mIsPrivate;
    int mIsAlsoPrivate;
};


/** <div rustbindgen private></div> */
struct ContradictPrivate {
    /** <div rustbindgen private="false"></div> */
    int mNotPrivate;
    int mIsPrivate;
};
