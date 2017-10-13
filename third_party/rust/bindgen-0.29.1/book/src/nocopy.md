# Preventing the Derivation of `Copy` and `Clone`

`bindgen` will attempt to derive the `Copy` and `Clone` traits on a best-effort
basis. Sometimes, it might not understand that although adding `#[derive(Copy,
Clone)]` to a translated type definition will compile, it still shouldn't do
that for reasons it can't know. In these cases, the `nocopy` annotation can be
used to prevent bindgen to autoderive the `Copy` and `Clone` traits for a type.

```c
/**
 * Although bindgen can't know, this struct is not safe to move because pthread
 * mutexes can't move in memory!
 *
 * <div rustbindgen nocopy></div>
 */
struct MyMutexWrapper {
    pthread_mutex_t raw;
    // ...
};
```
