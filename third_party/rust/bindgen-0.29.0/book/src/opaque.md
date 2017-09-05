# Treating a Type as an Opaque Blob of Bytes

Sometimes a type definition is simply not translatable to Rust, for example it
uses
[C++'s SFINAE](https://en.wikipedia.org/wiki/Substitution_failure_is_not_an_error) for
which Rust has no equivalent. In these cases, it is best to treat all
occurrences of the type as an opaque blob of bytes with a size and
alignment. `bindgen` will attempt to detect such cases and do this
automatically, but other times it needs some explicit help from you.

### Library

* [`bindgen::Builder::opaque_type`](https://docs.rs/bindgen/0.23.1/bindgen/struct.Builder.html#method.opaque_type)

### Command Line

* `--opaque-type <type>`

### Annotation

```cpp
/// <div rustbindgen opaque></div>
class Foo {
    // ...
};
```
