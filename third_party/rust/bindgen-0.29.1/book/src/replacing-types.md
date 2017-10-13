# Replacing One Type with Another

The `replaces` annotation can be used to use a type as a replacement for other
(presumably more complex) type. This is used in Stylo to generate bindings for
structures that for multiple reasons are too complex for bindgen to understand.

For example, in a C++ header:

```cpp
/**
 * <div rustbindgen replaces="nsTArray"></div>
 */
template<typename T>
class nsTArray_Simple {
  T* mBuffer;
public:
  // The existence of a destructor here prevents bindgen from deriving the Clone
  // trait via a simple memory copy.
  ~nsTArray_Simple() {};
};
```

That way, after code generation, the bindings for the `nsTArray` type are
the ones that would be generated for `nsTArray_Simple`.

Replacing is only available as an annotation. To replace a C or C++ definition
with a Rust definition, use [blacklisting](./blacklisting.html).
