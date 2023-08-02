# What `#[derive(Debug)]` generates

This derive macro is a clever superset of `Debug` from standard library. Additional features include:
- not imposing redundant trait bounds;
- `#[debug(skip)]` attribute to skip formatting struct field or enum variant;
- `#[debug("...", args...)]` to specify custom formatting for a particular struct or enum variant field;
- `#[debug(bounds(...))]` to impose additional custom trait bounds.




## The format of the format

You supply a format by placing an attribute on particular struct or enum variant field (not enum variant itself):
`#[debug("...", args...)]`. The format is exactly like in [`format!()`] or any other [`format_args!()`]-based macros.

The variables available in the arguments is `self` and each member of the variant, with members of tuple structs being
named with a leading underscore and their index, i.e. `_0`, `_1`, `_2`, etc.




### Generic data types

When deriving `Debug` for a generic struct/enum, all generic type arguments _used_ during formatting
are bound by respective formatting trait.

E.g., for a structure `Foo` defined like this:
```rust
use derive_more::Debug;

#[derive(Debug)]
struct Foo<'a, T1, T2: Trait, T3, T4> {
    #[debug("{a}")]
    a: T1,
    #[debug("{b}")]
    b: <T2 as Trait>::Type,
    #[debug("{c:?}")]
    c: Vec<T3>,
    #[debug("{d:p}")]
    d: &'a T1,
    #[debug(skip)]
    e: T4,
}

trait Trait { type Type; }
```

The following where clauses would be generated:
* `T1: Display + Pointer`
* `<T2 as Trait>::Type: Debug`
* `Bar<T3>: Display`




### Custom trait bounds

Sometimes you may want to specify additional trait bounds on your generic type parameters, so that they could be used
during formatting. This can be done with a `#[debug(bound(...))]` attribute.

`#[debug(bound(...))]` accepts code tokens in a format similar to the format used in angle bracket list (or `where`
clause predicates): `T: MyTrait, U: Trait1 + Trait2`.

Using `#[debug("...", ...)]` formatting we'll try our best to infer trait bounds, but in more advanced cases this isn't
possible. Our aim is to avoid imposing additional bounds, as they can be added with `#[debug(bound(...))]`.
In the example below, we can infer only that `V: Display`, other bounds have to be supplied by the user:

```rust
use std::fmt::Display;
use derive_more::Debug;

#[derive(Debug)]
#[debug(bound(T: MyTrait, U: Display))]
struct MyStruct<T, U, V, F> {
    #[debug("{}", a.my_function())]
    a: T,
    #[debug("{}", b.to_string().len())]
    b: U,
    #[debug("{c}")]
    c: V,
    #[debug(skip)]
    d: F,
}

trait MyTrait { fn my_function(&self) -> i32; }
```




## Example usage

```rust
use std::path::PathBuf;
use derive_more::Debug;

#[derive(Debug)]
struct MyInt(i32);

#[derive(Debug)]
struct MyIntHex(#[debug("{_0:x}")] i32);

#[derive(Debug)]
enum E {
    Skipped {
        x: u32,
        #[debug(skip)]
        y: u32,
    },
    Binary {
        #[debug("{i:b}")]
        i: i8,
    },
    Path(#[debug("{}", _0.display())] PathBuf),
}

assert_eq!(format!("{:?}", MyInt(-2)), "MyInt(-2)");
assert_eq!(format!("{:?}", MyIntHex(-255)), "MyIntHex(ffffff01)");
assert_eq!(format!("{:?}", E::Skipped { x: 10, y: 20 }), "Skipped { x: 10, .. }");
assert_eq!(format!("{:?}", E::Binary { i: -2 }), "Binary { i: 11111110 }");
assert_eq!(format!("{:?}", E::Path("abc".into())), "Path(abc)");
```

[`format!()`]: https://doc.rust-lang.org/stable/std/macro.format.html
[`format_args!()`]: https://doc.rust-lang.org/stable/std/macro.format_args.html
