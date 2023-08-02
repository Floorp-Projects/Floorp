# What `#[derive(Into)]` generates

This derive creates the exact opposite of `#[derive(From)]`.
Instead of allowing you to create a new instance of the struct from the values
it should contain, it allows you to extract the values from the struct. One
thing to note is that this derive doesn't actually generate an implementation
for the `Into` trait. Instead, it derives `From` for the values contained in
the struct and thus has an indirect implementation of `Into` as
[recommended by the docs][1].




## Structs

For structs with a single field you can call `.into()` to extract the inner type.

```rust
# use derive_more::Into;
#
#[derive(Debug, Into, PartialEq)]
struct Int(i32);

assert_eq!(2, Int(2).into());
```

For structs having multiple fields, `.into()` extracts a tuple containing the
desired content for each field.

```rust
# use derive_more::Into;
#
#[derive(Debug, Into, PartialEq)]
struct Point(i32, i32);

assert_eq!((1, 2), Point(1, 2).into());
```

To specify concrete types for deriving conversions into, use `#[into(<types>)]`.

```rust
# use std::borrow::Cow;
#
# use derive_more::Into;
#
#[derive(Debug, Into, PartialEq)]
#[into(Cow<'static, str>, String)]
struct Str(Cow<'static, str>);

assert_eq!("String".to_owned(), String::from(Str("String".into())));
assert_eq!(Cow::Borrowed("Cow"), <Cow<_>>::from(Str("Cow".into())));

#[derive(Debug, Into, PartialEq)]
#[into((i64, i64), (i32, i32))]
struct Point {
    x: i32,
    y: i32,
}

assert_eq!((1_i64, 2_i64), Point { x: 1_i32, y: 2_i32 }.into());
assert_eq!((3_i32, 4_i32), Point { x: 3_i32, y: 4_i32 }.into());
```

In addition to converting to owned types, this macro supports deriving into
reference (mutable or not) via `#[into(ref(...))]`/`#[into(ref_mut(...))]`.

```rust
# use derive_more::Into;
#
#[derive(Debug, Into, PartialEq)]
#[into(owned, ref(i32), ref_mut)]
struct Int(i32);

assert_eq!(2, Int(2).into());
assert_eq!(&2, <&i32>::from(&Int(2)));
assert_eq!(&mut 2, <&mut i32>::from(&mut Int(2)));
```

In case there are fields, that shouldn't be included in the conversion, use the
`#[into(skip)]` attribute.

```rust
# use std::marker::PhantomData;
#
# use derive_more::Into;
#
# struct Gram;
#
#[derive(Debug, Into, PartialEq)]
#[into(i32, i64, i128)]
struct Mass<Unit> {
    value: i32,
    #[into(skip)]
    _unit: PhantomData<Unit>,
}

assert_eq!(5, Mass::<Gram>::new(5).into());
assert_eq!(5_i64, Mass::<Gram>::new(5).into());
assert_eq!(5_i128, Mass::<Gram>::new(5).into());
#
# impl<Unit> Mass<Unit> {
#     fn new(value: i32) -> Self {
#         Self {
#             value,
#             _unit: PhantomData,
#         }
#     }
# }
```

## Enums

Deriving `Into` for enums is not supported as it would not always be successful,
so `TryInto` should be used instead.




[1]: https://doc.rust-lang.org/core/convert/trait.Into.html
