# What `#[derive(AsRef)]` generates

Deriving `AsRef` generates one or more implementations of `AsRef`, each
corresponding to one of the fields of the decorated type.
This allows types which contain some `T` to be passed anywhere that an
`AsRef<T>` is accepted.




## Newtypes and Structs with One Field

When `AsRef` is derived for a newtype or struct with one field, a single
implementation is generated to expose the underlying field.

```rust
# use derive_more::AsRef;
#
#[derive(AsRef)]
struct MyWrapper(String);
```

Generates:

```rust
# struct MyWrapper(String);
impl AsRef<String> for MyWrapper {
    fn as_ref(&self) -> &String {
        &self.0
    }
}
```

It's also possible to use the `#[as_ref(forward)]` attribute to forward
to the `as_ref` implementation of the field. So here `SingleFieldForward`
implements all `AsRef` for all types that `Vec<i32>` implements `AsRef` for.

```rust
# use derive_more::AsRef;
#
#[derive(AsRef)]
#[as_ref(forward)]
struct SingleFieldForward(Vec<i32>);

let item = SingleFieldForward(vec![]);
let _: &[i32] = (&item).as_ref();
```

This generates:

```rust
# struct SingleFieldForward(Vec<i32>);
impl<__AsRefT: ?::core::marker::Sized> ::core::convert::AsRef<__AsRefT> for SingleFieldForward
where
    Vec<i32>: ::core::convert::AsRef<__AsRefT>,
{
    #[inline]
    fn as_ref(&self) -> &__AsRefT {
        <Vec<i32> as ::core::convert::AsRef<__AsRefT>>::as_ref(&self.0)
    }
}
```




## Structs with Multiple Fields

When `AsRef` is derived for a struct with more than one field (including tuple
structs), you must also mark one or more fields with the `#[as_ref]` attribute.
An implementation will be generated for each indicated field.
You can also exclude a specific field by using `#[as_ref(ignore)]`.

```rust
# use derive_more::AsRef;
#
#[derive(AsRef)]
struct MyWrapper {
    #[as_ref]
    name: String,
    #[as_ref]
    num: i32,
    valid: bool,
}
```

Generates:

```rust
# struct MyWrapper {
#     name: String,
#     num: i32,
#     valid: bool,
# }
impl AsRef<String> for MyWrapper {
    fn as_ref(&self) -> &String {
        &self.name
    }
}

impl AsRef<i32> for MyWrapper {
    fn as_ref(&self) -> &i32 {
        &self.num
    }
}
```

Note that `AsRef<T>` may only be implemented once for any given type `T`.
This means any attempt to mark more than one field of the same type with
`#[as_ref]` will result in a compilation error.

```rust,compile_fail
# use derive_more::AsRef;
#
// Error! Conflicting implementations of AsRef<String>
#[derive(AsRef)]
struct MyWrapper {
    #[as_ref]
    str1: String,
    #[as_ref]
    str2: String,
}
```




## Enums

Deriving `AsRef` for enums is not supported.
