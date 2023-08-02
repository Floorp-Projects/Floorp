# What `#[derive(AsMut)]` generates

Deriving `AsMut` generates one or more implementations of `AsMut`, each
corresponding to one of the fields of the decorated type.
This allows types which contain some `T` to be passed anywhere that an
`AsMut<T>` is accepted.




## Newtypes and Structs with One Field

When `AsMut` is derived for a newtype or struct with one field, a single
implementation is generated to expose the underlying field.

```rust
# use derive_more::AsMut;
#
#[derive(AsMut)]
struct MyWrapper(String);
```

Generates:

```rust
# struct MyWrapper(String);
impl AsMut<String> for MyWrapper {
    fn as_mut(&mut self) -> &mut String {
        &mut self.0
    }
}
```

It's also possible to use the `#[as_mut(forward)]` attribute to forward
to the `as_mut` implementation of the field. So here `SingleFieldForward`
implements all `AsMut` for all types that `Vec<i32>` implements `AsMut` for.

```rust
# use derive_more::AsMut;
#
#[derive(AsMut)]
#[as_mut(forward)]
struct SingleFieldForward(Vec<i32>);

let mut item = SingleFieldForward(vec![]);
let _: &mut [i32] = (&mut item).as_mut();
```

This generates:

```rust
# struct SingleFieldForward(Vec<i32>);
impl<__AsMutT: ?::core::marker::Sized> ::core::convert::AsMut<__AsMutT> for SingleFieldForward
where
    Vec<i32>: ::core::convert::AsMut<__AsMutT>,
{
    #[inline]
    fn as_mut(&mut self) -> &mut __AsMutT {
        <Vec<i32> as ::core::convert::AsMut<__AsMutT>>::as_mut(&mut self.0)
    }
}
```




## Structs with Multiple Fields

When `AsMut` is derived for a struct with more than one field (including tuple
structs), you must also mark one or more fields with the `#[as_mut]` attribute.
An implementation will be generated for each indicated field.
You can also exclude a specific field by using `#[as_mut(ignore)]`.

```rust
# use derive_more::AsMut;
#
#[derive(AsMut)]
struct MyWrapper {
    #[as_mut]
    name: String,
    #[as_mut]
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
impl AsMut<String> for MyWrapper {
    fn as_mut(&mut self) -> &mut String {
        &mut self.name
    }
}

impl AsMut<i32> for MyWrapper {
    fn as_mut(&mut self) -> &mut i32 {
        &mut self.num
    }
}
```

Note that `AsMut<T>` may only be implemented once for any given type `T`. This means any attempt to
mark more than one field of the same type with `#[as_mut]` will result in a compilation error.

```rust,compile_fail
# use derive_more::AsMut;
#
// Error! Conflicting implementations of AsMut<String>
#[derive(AsMut)]
struct MyWrapper {
    #[as_mut]
    str1: String,
    #[as_mut]
    str2: String,
}
```




## Enums

Deriving `AsMut` for enums is not supported.
