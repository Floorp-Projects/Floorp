# Whitelisting

Whitelisting allows us to be precise about which type, function, and global
variable definitions `bindgen` generates bindings for. By default, if we don't
specify any whitelisting rules, everything is considered whitelisted. This may
not be desirable because of either

* the generated bindings contain a lot of extra defintions we don't plan on using, or
* the header file contains C++ features for which Rust does not have a
  corresponding form (such as partial template specialization), and we would
  like to avoid these definitions

If we specify whitelisting rules, then `bindgen` will only generate bindings to
types, functions, and global variables that match the whitelisting rules, or are
transitively used by a definition that matches them.

### Library

* [`bindgen::Builder::whitelisted_type`](https://docs.rs/bindgen/0.23.1/bindgen/struct.Builder.html#method.whitelisted_type)
* [`bindgen::Builder::whitelisted_function`](https://docs.rs/bindgen/0.23.1/bindgen/struct.Builder.html#method.whitelisted_function)
* [`bindgen::Builder::whitelisted_var`](https://docs.rs/bindgen/0.23.1/bindgen/struct.Builder.html#method.whitelisted_function)

### Command Line

* `--whitelist-type <type>`
* `--whitelist-function <function>`
* `--whitelist-var <var>`

### Annotations

None.
