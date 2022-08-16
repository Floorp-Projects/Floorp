## RON extensions

RON has extensions that can be enabled by adding the following attribute at the top of your RON document:

`#![enable(...)]`

# unwrap_newtypes

You can add this extension by adding the following attribute at the top of your RON document:

`#![enable(unwrap_newtypes)]`

This feature enables RON to automatically unwrap simple tuples.

```rust
struct NewType(u32);
struct Object {
    pub new_type: NewType,
}
```

Without `unwrap_newtypes`, because the value `5` can not be saved into `NewType(u32)`, your RON document would look like this:

``` ron
(
    new_type: (5),
)
```

With the `unwrap_newtypes` extension, this coercion is done automatically. So `5` will be interpreted as `(5)`.

``` ron
#![enable(unwrap_newtypes)]
(
    new_type: 5,
)
```

# implicit_some

You can add this extension by adding the following attribute at the top of your RON document:

`#![enable(implicit_some)]`

This feature enables RON to automatically convert any value to `Some(value)` if the deserialized type requires it.

```rust
struct Object {
    pub value: Option<u32>,
}
```

Without this feature, you would have to write this RON document.

```ron
(
    value: Some(5),
)
```

Enabling the feature would automatically infer `Some(x)` if `x` is given. In this case, RON automatically casts this `5` into a `Some(5)`.

```ron
(
    value: 5,
)
```

With this extension enabled, explicitly given `None` and `Some(..)` will be matched eagerly on `Option<Option<Option<u32>>>`, i.e.
* `5` -> `Some(Some(Some(5)))`
* `None` -> `None`
* `Some(5)` -> `Some(Some(Some(5)))`
* `Some(None)` -> `Some(None)`
* `Some(Some(5))` -> `Some(Some(Some(5)))`
* `Some(Some(None))` -> `Some(Some(None))`
* `Some(Some(Some(5)))` -> `Some(Some(Some(5)))`

# unwrap_variant_newtypes

You can add this extension by adding the following attribute at the top of your RON document:

`#![enable(unwrap_variant_newtypes)]`

This feature enables RON to automatically unwrap newtype enum variants.

```rust
#[derive(Deserialize)]
struct Inner {
    pub a: u8,
	pub b: bool,
}
#[derive(Deserialize)]
pub enum Enum {
	A(A),
	B,
}
```

Without `unwrap_variant_newtypes`, your RON document would look like this:

``` ron
(
    variant: A(Inner(a: 4, b: true)),
)
```

With the `unwrap_variant_newtypes` extension, the first structural layer inside a newtype variant will be unwrapped automatically:

``` ron
#![enable(unwrap_newtypes)]
(
    variant: A(a: 4, b: true),
)
```

Note that when the `unwrap_variant_newtypes` extension is enabled, the first layer inside a newtype variant will **always** be unwrapped, i.e. it is no longer possible to write `A(Inner(a: 4, b: true))` or `A((a: 4, b: true))`.
