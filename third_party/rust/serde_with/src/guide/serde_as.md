# `serde_as` Annotation

This is an alternative to serde's with-annotation.
It is more flexible and composable but work with fewer types.

The scheme is based on two new traits, [`SerializeAs`] and [`DeserializeAs`], which need to be implemented by all types which want to be compatible with `serde_as`.
The proc macro attribute [`#[serde_as]`][crate::serde_as] exists as a usability boost for users.

This site contains some general advice how to use this crate and then lists the implemented conversions for `serde_as`.
The basic design of the system was done by [@markazmierczak](https://github.com/markazmierczak).

1. [Switching from serde's with to `serde_as`](#switching-from-serdes-with-to-serde_as)
    1. [Deserializing Optional Fields](#deserializing-optional-fields)
    2. [Implementing `SerializeAs` / `DeserializeAs`](#implementing-serializeas--deserializeas)
    3. [Using `#[serde_as]` on types without `SerializeAs` and `Serialize` implementations](#using-serde_as-on-types-without-serializeas-and-serialize-implementations)
    4. [Using `#[serde_as]` with serde's remote derives](#using-serde_as-with-serdes-remote-derives)
    5. [Re-exporting `serde_as`](#re-exporting-serde_as)
2. [De/Serialize Implementations Available](#deserialize-implementations-available)
    1. [Bytes / `Vec<u8>` to hex string](#bytes--vecu8-to-hex-string)
    2. [`Default` from `null`](#default-from-null)
    3. [De/Serialize with `FromStr` and `Display`](#deserialize-with-fromstr-and-display)
    4. [`Duration` as seconds](#duration-as-seconds)
    5. [Ignore deserialization errors](#ignore-deserialization-errors)
    6. [`Maps` to `Vec` of tuples](#maps-to-vec-of-tuples)
    7. [`NaiveDateTime` like UTC timestamp](#naivedatetime-like-utc-timestamp)
    8. [`None` as empty `String`](#none-as-empty-string)
    9. [Timestamps as seconds since UNIX epoch](#timestamps-as-seconds-since-unix-epoch)
    10. [Value into JSON String](#value-into-json-string)
    11. [`Vec` of tuples to `Maps`](#vec-of-tuples-to-maps)

## Switching from serde's with to `serde_as`

For the user the main difference is that instead of

```rust,ignore
#[serde(with = "...")]
```

you now have to write

```rust,ignore
#[serde_as(as = "...")]
```

and place the `#[serde_as]` attribute *before* the `#[derive]` attribute.
You still need the `#[derive(Serialize, Deserialize)]` on the struct/enum.

All together this looks like:

```rust
use serde::{Deserialize, Serialize};
use serde_with::{serde_as, DisplayFromStr};

#[serde_as]
#[derive(Serialize, Deserialize)]
struct A {
    #[serde_as(as = "DisplayFromStr")]
    mime: mime::Mime,
}
```

The main advantage is that you can compose `serde_as` stuff, which is not possible with the with-annotation.
For example, the `mime` field from above could be nested in one or more data structures:

```rust
# use std::collections::BTreeMap;
# use serde::{Deserialize, Serialize};
# use serde_with::{serde_as, DisplayFromStr};
#
#[serde_as]
#[derive(Serialize, Deserialize)]
struct A {
    #[serde_as(as = "Option<BTreeMap<_, Vec<DisplayFromStr>>>")]
    mime: Option<BTreeMap<String, Vec<mime::Mime>>>,
}
```

### Deserializing Optional Fields

During deserialization serde treats fields of `Option<T>` as optional and does not require them to be present.
This breaks when adding either the `serde_as` annotation or serde's `with` annotation.
The default behavior can be restored by adding serde's `default` attribute.

```rust
# use serde::{Deserialize, Serialize};
# use serde_with::{serde_as, DisplayFromStr};
#
#[serde_as]
#[derive(Serialize, Deserialize)]
struct A {
    #[serde_as(as = "Option<DisplayFromStr>")]
    // Allows deserialization without providing a value for `val`
    #[serde(default)]
    val: Option<u32>,
}
```

In the future this behavior might change and `default` would be applied on `Option<T>` fields.
You can add your feedback at [serde_with#185].

### Implementing `SerializeAs` / `DeserializeAs`

You can support [`SerializeAs`] / [`DeserializeAs`] on your own types too.
Most "leaf" types do not need to implement these traits since they are supported implicitly.
"Leaf" type refers to types which directly serialize like plain data types.
[`SerializeAs`] / [`DeserializeAs`] is very important for collection types, like `Vec` or `BTreeMap`, since they need special handling for they key/value de/serialization such that the conversions can be done on the key/values.
You also find them implemented on the conversion types, such as the [`DisplayFromStr`] type.
These make up the bulk of this crate and allow you to perform all the nice conversions to [hex strings], the [bytes to string converter], or [duration to UNIX epoch].

The trait documentations for [`SerializeAs`] and [`DeserializeAs`] describe in details how to implement them for container types like `Box` or `Vec` and other types.

### Using `#[serde_as]` on types without `SerializeAs` and `Serialize` implementations

The `SerializeAs` and `DeserializeAs` traits can easily be used together with types from other crates without running into orphan rule problems.
This is a distinct advantage of the `serde_as` system.
For this example we assume we have a type `RemoteType` from a dependency which does not implement `Serialize` nor `SerializeAs`.
We assume we have a module containing a `serialize` and a `deserialize` function, which can be used in the `#[serde(with = "MODULE")]` annotation.
You find an example in the [official serde documentation](https://serde.rs/custom-date-format.html).

Our goal is to serialize this `Data` struct.
Right now we do not have anything we can use to replace `???` with, since `_` only works if `RemoteType` would implement `Serialize`, which it does not.

```rust
# #[cfg(FALSE)] {
#[serde_as]
#[derive(serde::Serialize)]
struct Data {
    #[serde_as(as = "Vec<???>")]
    vec: Vec<RemoteType>,
}
# }
```

We need to create a new type for which we can implement `SerializeAs`, to replace the `???`.
The `SerializeAs` implementation is **always** written for a local type.
This allows it to seamlessly work with types from dependencies without running into orphan rule problems.

```rust
# #[cfg(FALSE)] {
struct Localtype;

impl SerializeAs<RemoteType> for LocalType {
    fn serialize_as<S>(value: &RemoteType, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {  
        MODULE::serialize(value, serializer)
    }
}
# }
```

This is how the final implementation looks like.
We assumed we have a module `MODULE` with a `serialize` function already, which we use here to provide the implementation.
As can be seen this is mostly boilerplate, since the most part is encapsulated in `$module::serialize`.
The final `Data` struct will now look like:

```rust
# #[cfg(FALSE)] {
#[serde_as]
#[derive(serde::Serialize)]
struct Data {
    #[serde_as(as = "Vec<LocalType>")]
    vec: Vec<RemoteType>,
}
# }
```

### Using `#[serde_as]` with serde's remote derives

A special case of the above section is using it on remote derives.
This is a special functionality of serde where it derives the de-/serialization code for a type from another crate if all fields are `pub`.
You can find all the details in the [official serde documentation](https://serde.rs/remote-derive.html).

```rust
# #[cfg(FALSE)] {
// Pretend that this is somebody else's crate, not a module.
mod other_crate {
    // Neither Serde nor the other crate provides Serialize and Deserialize
    // impls for this struct.
    pub struct Duration {
        pub secs: i64,
        pub nanos: i32,
    }
}

////////////////////////////////////////////////////////////////////////////////

use other_crate::Duration;

// Serde calls this the definition of the remote type. It is just a copy of the
// remote data structure. The `remote` attribute gives the path to the actual
// type we intend to derive code for.
#[derive(serde::Serialize, serde::Deserialize)]
#[serde(remote = "Duration")]
struct DurationDef {
    secs: i64,
    nanos: i32,
}
# }
```

Our goal is it now to use `Duration` within `serde_as`.
We make use of the existing `DurationDef` type and its `serialize` and `deserialize` functions.
We can write this implementation.
The implementation for `DeserializeAs` works analogue.

```rust
# #[cfg(FALSE)] {
impl SerializeAs<Duration> for DurationDef {
    fn serialize_as<S>(value: &Duration, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {  
        DurationDef::serialize(value, serializer)
    }
}
# }
```

This now allows us to use `Duration` for serialization.

```rust
# #[cfg(FALSE)] {
use other_crate::Duration;

#[serde_as]
#[derive(serde::Serialize)]
struct Data {
    #[serde_as(as = "Vec<DurationDef>")]
    vec: Vec<Duration>,
}
# }
```

### Re-exporting `serde_as`

If `serde_as` is being used in a context where the `serde_with` crate is not available from the root
path, but is re-exported at some other path, the `crate = "..."` attribute argument should be used
to specify its path. This may be the case if `serde_as` is being used in a procedural macro -
otherwise, users of that macro would need to add `serde_with` to their own Cargo manifest.

The `crate` argument will generally be used in conjunction with [`serde`'s own `crate` argument].

For example, a type definition may be defined in a procedural macro:

```rust,ignore
// some_other_lib_derive/src/lib.rs

use proc_macro::TokenStream;
use quote::quote;

#[proc_macro]
pub fn define_some_type(_item: TokenStream) -> TokenStream {
    let def = quote! {
        #[serde(crate = "::some_other_lib::serde")]
        #[::some_other_lib::serde_with::serde_as(crate = "::some_other_lib::serde_with")]
        #[derive(::some_other_lib::serde::Deserialize)]
        struct Data {
            #[serde_as(as = "_")]
            a: u32,
        }
    };

    TokenStream::from(def)
}
```

This can be re-exported through a library which also re-exports `serde` and `serde_with`:

```rust,ignore
// some_other_lib/src/lib.rs

pub use serde;
pub use serde_with;
pub use some_other_lib_derive::define_some_type;
```

And the procedural macro can be used by other crates without any additional imports:

```rust,ignore
// consuming_crate/src/main.rs

some_other_lib::define_some_type!();
```

## De/Serialize Implementations Available

### Bytes / `Vec<u8>` to hex string

[`Hex`]

Requires the `hex` feature.

```ignore
// Rust
#[serde_as(as = "serde_with::hex::Hex")]
value: Vec<u8>,

// JSON
"value": "deadbeef",
```

### `Default` from `null`

[`DefaultOnNull`]

```ignore
// Rust
#[serde_as(as = "DefaultOnNull")]
value: u32,
#[serde_as(as = "DefaultOnNull<DisplayFromStr>")]
value2: u32,

// JSON
"value": 123,
"value2": "999",

// Deserializes null into the Default value, i.e.,
null => 0
```

### De/Serialize with `FromStr` and `Display`

Useful if a type implements `FromStr` / `Display` but not `Deserialize` / `Serialize`.

[`DisplayFromStr`]

```ignore
// Rust
#[serde_as(as = "serde_with::DisplayFromStr")]
value: u128,
#[serde_as(as = "serde_with::DisplayFromStr")]
mime: mime::Mime,

// JSON
"value": "340282366920938463463374607431768211455",
"mime": "text/*",
```

### `Duration` as seconds

[`DurationSeconds`]

```ignore
// Rust
#[serde_as(as = "serde_with::DurationSeconds<u64>")]
value: Duration,

// JSON
"value": 86400,
```

[`DurationSecondsWithFrac`] supports subsecond precision:

```ignore
// Rust
#[serde_as(as = "serde_with::DurationSecondsWithFrac<f64>")]
value: Duration,

// JSON
"value": 1.234,
```

Different serialization formats are possible:

```ignore
// Rust
#[serde_as(as = "serde_with::DurationSecondsWithFrac<String>")]
value: Duration,

// JSON
"value": "1.234",
```

The same conversions are also implemented for [`chrono::Duration`] with the `chrono` feature.

### Ignore deserialization errors

Check the documentation for [`DefaultOnError`].

### `Maps` to `Vec` of tuples

```ignore
// Rust
#[serde_as(as = "Vec<(_, _)>")]
value: HashMap<String, u32>, // also works with BTreeMap

// JSON
"value": [
    ["hello", 1],
    ["world", 2]
],
```

The [inverse operation](#vec-of-tuples-to-maps) is also available.

### `NaiveDateTime` like UTC timestamp

Requires the `chrono` feature.

```ignore
// Rust
#[serde_as(as = "chrono::DateTime<chrono::Utc>")]
value: chrono::NaiveDateTime,

// JSON
"value": "1994-11-05T08:15:30Z",
                             ^ Pretend DateTime is UTC
```

### `None` as empty `String`

[`NoneAsEmptyString`]

```ignore
// Rust
#[serde_as(as = "serde_with::NoneAsEmptyString")]
value: Option<String>,

// JSON
"value": "", // converts to None

"value": "Hello World!", // converts to Some
```

### Timestamps as seconds since UNIX epoch

[`TimestampSeconds`]

```ignore
// Rust
#[serde_as(as = "serde_with::TimestampSeconds<i64>")]
value: SystemTime,

// JSON
"value": 86400,
```

[`TimestampSecondsWithFrac`] supports subsecond precision:

```ignore
// Rust
#[serde_as(as = "serde_with::TimestampSecondsWithFrac<f64>")]
value: SystemTime,

// JSON
"value": 1.234,
```

Different serialization formats are possible:

```ignore
// Rust
#[serde_as(as = "serde_with::TimestampSecondsWithFrac<String>")]
value: SystemTime,

// JSON
"value": "1.234",
```

The same conversions are also implemented for [`chrono::DateTime<Utc>`] and [`chrono::DateTime<Local>`] with the `chrono` feature.

### Value into JSON String

Some JSON APIs are weird and return a JSON encoded string in a JSON response

[`JsonString`]

Requires the `json` feature.

```ignore
// Rust
#[derive(Deserialize, Serialize)]
struct OtherStruct {
    value: usize,
}

#[serde_as(as = "serde_with::json::JsonString")]
value: OtherStruct,

// JSON
"value": "{\"value\":5}",
```

### `Vec` of tuples to `Maps`

```ignore
// Rust
#[serde_as(as = "HashMap<_, _>")] // also works with BTreeMap
value: Vec<(String, u32)>,

// JSON
"value": {
    "hello": 1,
    "world": 2
},
```

The [inverse operation](#maps-to-vec-of-tuples) is also available.

[`chrono::DateTime<Local>`]: chrono_crate::DateTime
[`chrono::DateTime<Utc>`]: chrono_crate::DateTime
[`chrono::Duration`]: https://docs.rs/chrono/latest/chrono/struct.Duration.html
[`DefaultOnError`]: crate::DefaultOnError
[`DefaultOnNull`]: crate::DefaultOnNull
[`DeserializeAs`]: crate::DeserializeAs
[`DisplayFromStr`]: crate::DisplayFromStr
[`DurationSeconds`]: crate::DurationSeconds
[`DurationSecondsWithFrac`]: crate::DurationSecondsWithFrac
[`Hex`]: crate::hex::Hex
[`JsonString`]: crate::json::JsonString
[`NoneAsEmptyString`]: crate::NoneAsEmptyString
[`SerializeAs`]: crate::SerializeAs
[bytes to string converter]: crate::BytesOrString
[duration to UNIX epoch]: crate::DurationSeconds
[hex strings]: crate::hex::Hex
[serde_with#185]: https://github.com/jonasbb/serde_with/issues/185
[`serde`'s own `crate` argument]: https://serde.rs/container-attrs.html#crate
