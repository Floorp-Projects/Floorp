# Blacklisting

If you need to provide your own custom translation of some type (for example,
because you need to wrap one of its fields in an `UnsafeCell`), you can
explicitly blacklist generation of its definition. Uses of the blacklisted type
will still appear in other types' definitions. (If you don't want the type to
appear in the bindings at
all, [make it opaque](./opaque.html) instead of
blacklisting it.)

### Library

* [`bindgen::Builder::hide_type`](https://docs.rs/bindgen/0.23.1/bindgen/struct.Builder.html#method.hide_type)

### Command Line

* `--blacklist-type <type>`

### Annotations

```cpp
/// <div rustbindgen hide></div>
class Foo {
    // ...
};
```
