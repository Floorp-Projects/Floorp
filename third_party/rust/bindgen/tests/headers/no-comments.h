// bindgen-flags: --no-doc-comments

struct Foo {
    int s; /*!< Including this will prevent rustc for compiling it */
};
