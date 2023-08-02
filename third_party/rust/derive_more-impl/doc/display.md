# What `#[derive(Display)]` generates

Deriving `Display` will generate a `Display` implementation, with a `fmt`
method that matches `self` and each of its variants. In the case of a struct or union,
only a single variant is available, and it is thus equivalent to a simple `let` statement.
In the case of an enum, each of its variants is matched.

For each matched variant, a `write!` expression will be generated with
the supplied format, or an automatically inferred one.

You specify the format on each variant by writing e.g. `#[display("my val: {}", some_val * 2)]`.
For enums, you can either specify it on each variant, or on the enum as a whole.

For variants that don't have a format specified, it will simply defer to the format of the
inner variable. If there is no such variable, or there is more than 1, an error is generated.




## The format of the format

You supply a format by attaching an attribute of the syntax: `#[display("...", args...)]`.
The format supplied is passed verbatim to `write!`. The arguments supplied handled specially,
due to constraints in the syntax of attributes. In the case of an argument being a simple
identifier, it is passed verbatim. If an argument is a string, it is **parsed as an expression**,
and then passed to `write!`.

The variables available in the arguments is `self` and each member of the variant,
with members of tuple structs being named with a leading underscore and their index,
i.e. `_0`, `_1`, `_2`, etc.


### Other formatting traits

The syntax does not change, but the name of the attribute is the snake case version of the trait.
E.g. `Octal` -> `octal`, `Pointer` -> `pointer`, `UpperHex` -> `upper_hex`.

Note, that `Debug` has a slightly different API and semantics, described in its docs, and so,
requires a separate `debug` feature.


### Generic data types

When deriving `Display` (or other formatting trait) for a generic struct/enum, all generic type
arguments used during formatting are bound by respective formatting trait.

E.g., for a structure `Foo` defined like this:
```rust
# use derive_more::Display;
#
# trait Trait { type Type; }
#
#[derive(Display)]
#[display("{} {} {:?} {:p}", a, b, c, d)]
struct Foo<'a, T1, T2: Trait, T3> {
    a: T1,
    b: <T2 as Trait>::Type,
    c: Vec<T3>,
    d: &'a T1,
}
```

The following where clauses would be generated:
* `T1: Display + Pointer`
* `<T2 as Trait>::Type: Debug`
* `Bar<T3>: Display`


### Custom trait bounds

Sometimes you may want to specify additional trait bounds on your generic type parameters, so that they
could be used during formatting. This can be done with a `#[display(bound(...))]` attribute.

`#[display(bound(...))]` accepts code tokens in a format similar to the format
used in angle bracket list (or `where` clause predicates): `T: MyTrait, U: Trait1 + Trait2`.

Only type parameters defined on a struct allowed to appear in bound-string and they can only be bound
by traits, i.e. no lifetime parameters or lifetime bounds allowed in bound-string.

`#[display("fmt", ...)]` arguments are parsed as an arbitrary Rust expression and passed to generated
`write!` as-is, it's impossible to meaningfully infer any kind of trait bounds for generic type parameters
used this way. That means that you'll **have to** explicitly specify all trait bound used. Either in the
struct/enum definition, or via `#[display(bound(...))]` attribute.

Note how we have to bound `U` and `V` by `Display` in the following example, as no bound is inferred.
Not even `Display`.

```rust
# use derive_more::Display;
#
# trait MyTrait { fn my_function(&self) -> i32; }
#
#[derive(Display)]
#[display(bound(T: MyTrait, U: Display))]
#[display("{} {} {}", a.my_function(), b.to_string().len(), c)]
struct MyStruct<T, U, V> {
    a: T,
    b: U,
    c: V,
}
```




## Example usage

```rust
# use std::path::PathBuf;
#
# use derive_more::{Display, Octal, UpperHex};
#
#[derive(Display)]
struct MyInt(i32);

#[derive(Display)]
#[display("({x}, {y})")]
struct Point2D {
    x: i32,
    y: i32,
}

#[derive(Display)]
enum E {
    Uint(u32),
    #[display("I am B {:b}", i)]
    Binary {
        i: i8,
    },
    #[display("I am C {}", _0.display())]
    Path(PathBuf),
}

#[derive(Display)]
#[display("Hello there!")]
union U {
    i: u32,
}

#[derive(Octal)]
#[octal("7")]
struct S;

#[derive(UpperHex)]
#[upper_hex("UpperHex")]
struct UH;

#[derive(Display)]
struct Unit;

#[derive(Display)]
struct UnitStruct {}

#[derive(Display)]
#[display("{}", self.sign())]
struct PositiveOrNegative {
    x: i32,
}

impl PositiveOrNegative {
    fn sign(&self) -> &str {
        if self.x >= 0 {
            "Positive"
        } else {
            "Negative"
        }
    }
}

assert_eq!(MyInt(-2).to_string(), "-2");
assert_eq!(Point2D { x: 3, y: 4 }.to_string(), "(3, 4)");
assert_eq!(E::Uint(2).to_string(), "2");
assert_eq!(E::Binary { i: -2 }.to_string(), "I am B 11111110");
assert_eq!(E::Path("abc".into()).to_string(), "I am C abc");
assert_eq!(U { i: 2 }.to_string(), "Hello there!");
assert_eq!(format!("{:o}", S), "7");
assert_eq!(format!("{:X}", UH), "UpperHex");
assert_eq!(Unit.to_string(), "Unit");
assert_eq!(UnitStruct {}.to_string(), "UnitStruct");
assert_eq!(PositiveOrNegative { x: 1 }.to_string(), "Positive");
assert_eq!(PositiveOrNegative { x: -1 }.to_string(), "Negative");
```
