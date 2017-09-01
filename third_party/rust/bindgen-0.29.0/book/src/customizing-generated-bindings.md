# Customizing the Generated Bindings

The translation of classes, structs, enums, and typedefs can be adjusted in a
few ways:

1. By using the `bindgen::Builder`'s configuration methods, when using `bindgen`
   as a library.

2. By passing extra flags and options to the `bindgen` executable.

3. By adding an annotation comment to the C/C++ source code. Annotations are
   specially formatted HTML tags inside doxygen style comments:

     * For single line comments:
       ```c
       /// <div rustbindgen></div>
       ```

     * For multi-line comments:
       ```c
       /**
        * <div rustbindgen></div>
        */
       ```

We'll leave the nitty-gritty details to
the [docs.rs API reference](https://docs.rs/bindgen) and `bindgen --help`, but
provide higher level concept documentation here.
