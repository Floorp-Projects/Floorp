//! Adapted from [`nom`](https://github.com/Geal/nom) by removing the
//! `IResult::Incomplete` variant which:
//!
//! - we don't need,
//! - is an unintuitive footgun when working with non-streaming use cases, and
//! - more than doubles compilation time.
//!
//! ## Whitespace handling strategy
//!
//! As (sy)nom is a parser combinator library, the parsers provided here and
//! that you implement yourself are all made up of successively more primitive
//! parsers, eventually culminating in a small number of fundamental parsers
//! that are implemented in Rust. Among these are `punct!` and `keyword!`.
//!
//! All synom fundamental parsers (those not combined out of other parsers)
//! should be written to skip over leading whitespace in their input. This way,
//! as long as every parser eventually boils down to some combination of
//! fundamental parsers, we get correct whitespace handling at all levels for
//! free.
//!
//! For our use case, this strategy is a huge improvement in usability,
//! correctness, and compile time over nom's `ws!` strategy.

extern crate unicode_xid;

#[doc(hidden)]
pub mod space;

#[doc(hidden)]
pub mod helper;

/// The result of a parser.
#[derive(Debug, PartialEq, Eq, Clone)]
pub enum IResult<I, O> {
    /// Parsing succeeded. The first field contains the rest of the unparsed
    /// data and the second field contains the parse result.
    Done(I, O),
    /// Parsing failed.
    Error,
}

impl<'a, O> IResult<&'a str, O> {
    /// Unwraps the result, asserting the the parse is complete. Panics with a
    /// message based on the given string if the parse failed or is incomplete.
    ///
    /// ```rust
    /// extern crate syn;
    /// #[macro_use] extern crate synom;
    ///
    /// use syn::Ty;
    /// use syn::parse::ty;
    ///
    /// // One or more Rust types separated by commas.
    /// named!(comma_separated_types -> Vec<Ty>,
    ///     separated_nonempty_list!(punct!(","), ty)
    /// );
    ///
    /// fn main() {
    ///     let input = "&str, Map<K, V>, String";
    ///
    ///     let parsed = comma_separated_types(input).expect("comma-separated types");
    ///
    ///     assert_eq!(parsed.len(), 3);
    ///     println!("{:?}", parsed);
    /// }
    /// ```
    pub fn expect(self, name: &str) -> O {
        match self {
            IResult::Done(mut rest, o) => {
                rest = space::skip_whitespace(rest);
                if rest.is_empty() {
                    o
                } else {
                    panic!("unparsed tokens after {}: {:?}", name, rest)
                }
            }
            IResult::Error => panic!("failed to parse {}", name),
        }
    }
}

/// Define a function from a parser combination.
///
/// - **Syntax:** `named!(NAME -> TYPE, PARSER)` or `named!(pub NAME -> TYPE, PARSER)`
///
/// ```rust
/// # extern crate syn;
/// # #[macro_use] extern crate synom;
/// # use syn::Ty;
/// # use syn::parse::ty;
/// // One or more Rust types separated by commas.
/// named!(pub comma_separated_types -> Vec<Ty>,
///     separated_nonempty_list!(punct!(","), ty)
/// );
/// # fn main() {}
/// ```
#[macro_export]
macro_rules! named {
    ($name:ident -> $o:ty, $submac:ident!( $($args:tt)* )) => {
        fn $name(i: &str) -> $crate::IResult<&str, $o> {
            $submac!(i, $($args)*)
        }
    };

    (pub $name:ident -> $o:ty, $submac:ident!( $($args:tt)* )) => {
        pub fn $name(i: &str) -> $crate::IResult<&str, $o> {
            $submac!(i, $($args)*)
        }
    };
}

/// Invoke the given parser function with the passed in arguments.
///
/// - **Syntax:** `call!(FUNCTION, ARGS...)`
///
///   where the signature of the function is `fn(&str, ARGS...) -> IResult<&str, T>`
/// - **Output:** `T`, the result of invoking the function `FUNCTION`
///
/// ```rust
/// #[macro_use] extern crate synom;
///
/// use synom::IResult;
///
/// // Parses any string up to but not including the given character, returning
/// // the content up to the given character. The given character is required to
/// // be present in the input string.
/// fn skip_until(input: &str, ch: char) -> IResult<&str, &str> {
///     if let Some(pos) = input.find(ch) {
///         IResult::Done(&input[pos..], &input[..pos])
///     } else {
///         IResult::Error
///     }
/// }
///
/// // Parses any string surrounded by tilde characters '~'. Returns the content
/// // between the tilde characters.
/// named!(surrounded_by_tilde -> &str, delimited!(
///     punct!("~"),
///     call!(skip_until, '~'),
///     punct!("~")
/// ));
///
/// fn main() {
///     let input = "~ abc def ~";
///
///     let inner = surrounded_by_tilde(input).expect("surrounded by tilde");
///
///     println!("{:?}", inner);
/// }
/// ```
#[macro_export]
macro_rules! call {
    ($i:expr, $fun:expr $(, $args:expr)*) => {
        $fun($i $(, $args)*)
    };
}

/// Transform the result of a parser by applying a function or closure.
///
/// - **Syntax:** `map!(THING, FN)`
/// - **Output:** the return type of function FN applied to THING
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::{Item, Ident};
/// use syn::parse::item;
///
/// fn get_item_ident(item: Item) -> Ident {
///     item.ident
/// }
///
/// // Parses an item and returns the name (identifier) of the item only.
/// named!(item_ident -> Ident,
///     map!(item, get_item_ident)
/// );
///
/// // Or equivalently:
/// named!(item_ident2 -> Ident,
///     map!(item, |i: Item| i.ident)
/// );
///
/// fn main() {
///     let input = "fn foo() {}";
///
///     let parsed = item_ident(input).expect("item");
///
///     assert_eq!(parsed, "foo");
/// }
/// ```
#[macro_export]
macro_rules! map {
    ($i:expr, $submac:ident!( $($args:tt)* ), $g:expr) => {
        match $submac!($i, $($args)*) {
            $crate::IResult::Error => $crate::IResult::Error,
            $crate::IResult::Done(i, o) => {
                $crate::IResult::Done(i, call!(o, $g))
            }
        }
    };

    ($i:expr, $f:expr, $g:expr) => {
        map!($i, call!($f), $g)
    };
}

/// Parses successfully if the given parser fails to parse. Does not consume any
/// of the input.
///
/// - **Syntax:** `not!(THING)`
/// - **Output:** `()`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
/// use synom::IResult;
///
/// // Parses a shebang line like `#!/bin/bash` and returns the part after `#!`.
/// // Note that a line starting with `#![` is an inner attribute, not a
/// // shebang.
/// named!(shebang -> &str, preceded!(
///     tuple!(tag!("#!"), not!(tag!("["))),
///     take_until!("\n")
/// ));
///
/// fn main() {
///     let bin_bash = "#!/bin/bash\n";
///     let parsed = shebang(bin_bash).expect("shebang");
///     assert_eq!(parsed, "/bin/bash");
///
///     let inner_attr = "#![feature(specialization)]\n";
///     let err = shebang(inner_attr);
///     assert_eq!(err, IResult::Error);
/// }
/// ```
#[macro_export]
macro_rules! not {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {
        match $submac!($i, $($args)*) {
            $crate::IResult::Done(_, _) => $crate::IResult::Error,
            $crate::IResult::Error => $crate::IResult::Done($i, ()),
        }
    };
}

/// Conditionally execute the given parser.
///
/// If you are familiar with nom, this is nom's `cond_with_error` parser.
///
/// - **Syntax:** `cond!(CONDITION, THING)`
/// - **Output:** `Some(THING)` if the condition is true, else `None`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::parse::boolean;
///
/// // Parses a tuple of booleans like `(true, false, false)`, possibly with a
/// // dotdot indicating omitted values like `(true, true, .., true)`. Returns
/// // separate vectors for the booleans before and after the dotdot. The second
/// // vector is None if there was no dotdot.
/// named!(bools_with_dotdot -> (Vec<bool>, Option<Vec<bool>>), do_parse!(
///     punct!("(") >>
///     before: separated_list!(punct!(","), boolean) >>
///     after: option!(do_parse!(
///         // Only allow comma if there are elements before dotdot, i.e. cannot
///         // be `(, .., true)`.
///         cond!(!before.is_empty(), punct!(",")) >>
///         punct!("..") >>
///         after: many0!(preceded!(punct!(","), boolean)) >>
///         // Only allow trailing comma if there are elements after dotdot,
///         // i.e. cannot be `(true, .., )`.
///         cond!(!after.is_empty(), option!(punct!(","))) >>
///         (after)
///     )) >>
///     // Allow trailing comma if there is no dotdot but there are elements.
///     cond!(!before.is_empty() && after.is_none(), option!(punct!(","))) >>
///     punct!(")") >>
///     (before, after)
/// ));
///
/// fn main() {
///     let input = "(true, false, false)";
///     let parsed = bools_with_dotdot(input).expect("bools with dotdot");
///     assert_eq!(parsed, (vec![true, false, false], None));
///
///     let input = "(true, true, .., true)";
///     let parsed = bools_with_dotdot(input).expect("bools with dotdot");
///     assert_eq!(parsed, (vec![true, true], Some(vec![true])));
///
///     let input = "(.., true)";
///     let parsed = bools_with_dotdot(input).expect("bools with dotdot");
///     assert_eq!(parsed, (vec![], Some(vec![true])));
///
///     let input = "(true, true, ..)";
///     let parsed = bools_with_dotdot(input).expect("bools with dotdot");
///     assert_eq!(parsed, (vec![true, true], Some(vec![])));
///
///     let input = "(..)";
///     let parsed = bools_with_dotdot(input).expect("bools with dotdot");
///     assert_eq!(parsed, (vec![], Some(vec![])));
/// }
/// ```
#[macro_export]
macro_rules! cond {
    ($i:expr, $cond:expr, $submac:ident!( $($args:tt)* )) => {
        if $cond {
            match $submac!($i, $($args)*) {
                $crate::IResult::Done(i, o) => $crate::IResult::Done(i, ::std::option::Option::Some(o)),
                $crate::IResult::Error => $crate::IResult::Error,
            }
        } else {
            $crate::IResult::Done($i, ::std::option::Option::None)
        }
    };

    ($i:expr, $cond:expr, $f:expr) => {
        cond!($i, $cond, call!($f))
    };
}

/// Fail to parse if condition is false, otherwise parse the given parser.
///
/// This is typically used inside of `option!` or `alt!`.
///
/// - **Syntax:** `cond_reduce!(CONDITION, THING)`
/// - **Output:** `THING`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::parse::boolean;
///
/// #[derive(Debug, PartialEq)]
/// struct VariadicBools {
///     data: Vec<bool>,
///     variadic: bool,
/// }
///
/// // Parse one or more comma-separated booleans, possibly ending in "..." to
/// // indicate there may be more.
/// named!(variadic_bools -> VariadicBools, do_parse!(
///     data: separated_nonempty_list!(punct!(","), boolean) >>
///     trailing_comma: option!(punct!(",")) >>
///     // Only allow "..." if there is a comma after the last boolean. Using
///     // `cond_reduce!` is more convenient here than using `cond!`. The
///     // alternatives are:
///     //
///     //   - `cond!(c, option!(p))` or `option!(cond!(c, p))`
///     //     Gives `Some(Some("..."))` for variadic and `Some(None)` or `None`
///     //     which both mean not variadic.
///     //   - `cond_reduce!(c, option!(p))`
///     //     Incorrect; would fail to parse if there is no trailing comma.
///     //   - `option!(cond_reduce!(c, p))`
///     //     Gives `Some("...")` for variadic and `None` otherwise. Perfect!
///     variadic: option!(cond_reduce!(trailing_comma.is_some(), punct!("..."))) >>
///     (VariadicBools {
///         data: data,
///         variadic: variadic.is_some(),
///     })
/// ));
///
/// fn main() {
///     let input = "true, true";
///     let parsed = variadic_bools(input).expect("variadic bools");
///     assert_eq!(parsed, VariadicBools {
///         data: vec![true, true],
///         variadic: false,
///     });
///
///     let input = "true, ...";
///     let parsed = variadic_bools(input).expect("variadic bools");
///     assert_eq!(parsed, VariadicBools {
///         data: vec![true],
///         variadic: true,
///     });
/// }
/// ```
#[macro_export]
macro_rules! cond_reduce {
    ($i:expr, $cond:expr, $submac:ident!( $($args:tt)* )) => {
        if $cond {
            $submac!($i, $($args)*)
        } else {
            $crate::IResult::Error
        }
    };

    ($i:expr, $cond:expr, $f:expr) => {
        cond_reduce!($i, $cond, call!($f))
    };
}

/// Parse two things, returning the value of the second.
///
/// - **Syntax:** `preceded!(BEFORE, THING)`
/// - **Output:** `THING`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::Expr;
/// use syn::parse::expr;
///
/// // An expression preceded by ##.
/// named!(pound_pound_expr -> Expr,
///     preceded!(punct!("##"), expr)
/// );
///
/// fn main() {
///     let input = "## 1 + 1";
///
///     let parsed = pound_pound_expr(input).expect("pound pound expr");
///
///     println!("{:?}", parsed);
/// }
/// ```
#[macro_export]
macro_rules! preceded {
    ($i:expr, $submac:ident!( $($args:tt)* ), $submac2:ident!( $($args2:tt)* )) => {
        match tuple!($i, $submac!($($args)*), $submac2!($($args2)*)) {
            $crate::IResult::Done(remaining, (_, o)) => $crate::IResult::Done(remaining, o),
            $crate::IResult::Error => $crate::IResult::Error,
        }
    };

    ($i:expr, $submac:ident!( $($args:tt)* ), $g:expr) => {
        preceded!($i, $submac!($($args)*), call!($g))
    };

    ($i:expr, $f:expr, $submac:ident!( $($args:tt)* )) => {
        preceded!($i, call!($f), $submac!($($args)*))
    };

    ($i:expr, $f:expr, $g:expr) => {
        preceded!($i, call!($f), call!($g))
    };
}

/// Parse two things, returning the value of the first.
///
/// - **Syntax:** `terminated!(THING, AFTER)`
/// - **Output:** `THING`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::Expr;
/// use syn::parse::expr;
///
/// // An expression terminated by ##.
/// named!(expr_pound_pound -> Expr,
///     terminated!(expr, punct!("##"))
/// );
///
/// fn main() {
///     let input = "1 + 1 ##";
///
///     let parsed = expr_pound_pound(input).expect("expr pound pound");
///
///     println!("{:?}", parsed);
/// }
/// ```
#[macro_export]
macro_rules! terminated {
    ($i:expr, $submac:ident!( $($args:tt)* ), $submac2:ident!( $($args2:tt)* )) => {
        match tuple!($i, $submac!($($args)*), $submac2!($($args2)*)) {
            $crate::IResult::Done(remaining, (o, _)) => $crate::IResult::Done(remaining, o),
            $crate::IResult::Error => $crate::IResult::Error,
        }
    };

    ($i:expr, $submac:ident!( $($args:tt)* ), $g:expr) => {
        terminated!($i, $submac!($($args)*), call!($g))
    };

    ($i:expr, $f:expr, $submac:ident!( $($args:tt)* )) => {
        terminated!($i, call!($f), $submac!($($args)*))
    };

    ($i:expr, $f:expr, $g:expr) => {
        terminated!($i, call!($f), call!($g))
    };
}

/// Parse zero or more values using the given parser.
///
/// - **Syntax:** `many0!(THING)`
/// - **Output:** `Vec<THING>`
///
/// You may also be looking for:
///
/// - `separated_list!` - zero or more values with separator
/// - `separated_nonempty_list!` - one or more values
/// - `terminated_list!` - zero or more, allows trailing separator
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::Item;
/// use syn::parse::item;
///
/// named!(items -> Vec<Item>, many0!(item));
///
/// fn main() {
///     let input = "
///         fn a() {}
///         fn b() {}
///     ";
///
///     let parsed = items(input).expect("items");
///
///     assert_eq!(parsed.len(), 2);
///     println!("{:?}", parsed);
/// }
/// ```
#[macro_export]
macro_rules! many0 {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {{
        let ret;
        let mut res   = ::std::vec::Vec::new();
        let mut input = $i;

        loop {
            if input.is_empty() {
                ret = $crate::IResult::Done(input, res);
                break;
            }

            match $submac!(input, $($args)*) {
                $crate::IResult::Error => {
                    ret = $crate::IResult::Done(input, res);
                    break;
                }
                $crate::IResult::Done(i, o) => {
                    // loop trip must always consume (otherwise infinite loops)
                    if i.len() == input.len() {
                        ret = $crate::IResult::Error;
                        break;
                    }

                    res.push(o);
                    input = i;
                }
            }
        }

        ret
    }};

    ($i:expr, $f:expr) => {
        $crate::many0($i, $f)
    };
}

// Improve compile time by compiling this loop only once per type it is used
// with.
//
// Not public API.
#[doc(hidden)]
pub fn many0<'a, T>(mut input: &'a str,
                    f: fn(&'a str) -> IResult<&'a str, T>)
                    -> IResult<&'a str, Vec<T>> {
    let mut res = Vec::new();

    loop {
        if input.is_empty() {
            return IResult::Done(input, res);
        }

        match f(input) {
            IResult::Error => {
                return IResult::Done(input, res);
            }
            IResult::Done(i, o) => {
                // loop trip must always consume (otherwise infinite loops)
                if i.len() == input.len() {
                    return IResult::Error;
                }

                res.push(o);
                input = i;
            }
        }
    }
}

/// Parse a value without consuming it from the input data.
///
/// - **Syntax:** `peek!(THING)`
/// - **Output:** `THING`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::Expr;
/// use syn::parse::{ident, expr};
/// use synom::IResult;
///
/// // Parse an expression that begins with an identifier.
/// named!(ident_expr -> Expr,
///     preceded!(peek!(ident), expr)
/// );
///
/// fn main() {
///     // begins with an identifier
///     let input = "banana + 1";
///     let parsed = ident_expr(input).expect("ident");
///     println!("{:?}", parsed);
///
///     // does not begin with an identifier
///     let input = "1 + banana";
///     let err = ident_expr(input);
///     assert_eq!(err, IResult::Error);
/// }
/// ```
#[macro_export]
macro_rules! peek {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {
        match $submac!($i, $($args)*) {
            $crate::IResult::Done(_, o) => $crate::IResult::Done($i, o),
            $crate::IResult::Error => $crate::IResult::Error,
        }
    };

    ($i:expr, $f:expr) => {
        peek!($i, call!($f))
    };
}

/// Parse the part of the input up to but not including the given string. Fail
/// to parse if the given string is not present in the input.
///
/// - **Syntax:** `take_until!("...")`
/// - **Output:** `&str`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
/// use synom::IResult;
///
/// // Parse a single line doc comment: /// ...
/// named!(single_line_doc -> &str,
///     preceded!(punct!("///"), take_until!("\n"))
/// );
///
/// fn main() {
///     let comment = "/// comment\n";
///     let parsed = single_line_doc(comment).expect("single line doc comment");
///     assert_eq!(parsed, " comment");
/// }
/// ```
#[macro_export]
macro_rules! take_until {
    ($i:expr, $substr:expr) => {{
        if $substr.len() > $i.len() {
            $crate::IResult::Error
        } else {
            let substr_vec: Vec<char> = $substr.chars().collect();
            let mut window: Vec<char> = vec![];
            let mut offset = $i.len();
            let mut parsed = false;
            for (o, c) in $i.char_indices() {
                window.push(c);
                if window.len() > substr_vec.len() {
                    window.remove(0);
                }
                if window == substr_vec {
                    parsed = true;
                    window.pop();
                    let window_len: usize = window.iter()
                        .map(|x| x.len_utf8())
                        .fold(0, |x, y| x + y);
                    offset = o - window_len;
                    break;
                }
            }
            if parsed {
                $crate::IResult::Done(&$i[offset..], &$i[..offset])
            } else {
                $crate::IResult::Error
            }
        }
    }};
}

/// Parse the given string from exactly the current position in the input. You
/// almost always want `punct!` or `keyword!` instead of this.
///
/// The `tag!` parser is equivalent to `punct!` but does not ignore leading
/// whitespace. Both `punct!` and `keyword!` skip over leading whitespace. See
/// an explanation of synom's whitespace handling strategy in the top-level
/// crate documentation.
///
/// - **Syntax:** `tag!("...")`
/// - **Output:** `"..."`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::StrLit;
/// use syn::parse::string;
/// use synom::IResult;
///
/// // Parse a proposed syntax for an owned string literal: "abc"s
/// named!(owned_string -> String,
///     map!(
///         terminated!(string, tag!("s")),
///         |lit: StrLit| lit.value
///     )
/// );
///
/// fn main() {
///     let input = r#"  "abc"s  "#;
///     let parsed = owned_string(input).expect("owned string literal");
///     println!("{:?}", parsed);
///
///     let input = r#"  "abc" s  "#;
///     let err = owned_string(input);
///     assert_eq!(err, IResult::Error);
/// }
/// ```
#[macro_export]
macro_rules! tag {
    ($i:expr, $tag:expr) => {
        if $i.starts_with($tag) {
            $crate::IResult::Done(&$i[$tag.len()..], &$i[..$tag.len()])
        } else {
            $crate::IResult::Error
        }
    };
}

/// Pattern-match the result of a parser to select which other parser to run.
///
/// - **Syntax:** `switch!(TARGET, PAT1 => THEN1 | PAT2 => THEN2 | ...)`
/// - **Output:** `T`, the return type of `THEN1` and `THEN2` and ...
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::{Ident, Ty};
/// use syn::parse::{ident, ty};
///
/// #[derive(Debug)]
/// enum UnitType {
///     Struct {
///         name: Ident,
///     },
///     Enum {
///         name: Ident,
///         variant: Ident,
///     },
/// }
///
/// // Parse a unit struct or enum: either `struct S;` or `enum E { V }`.
/// named!(unit_type -> UnitType, do_parse!(
///     which: alt!(keyword!("struct") | keyword!("enum")) >>
///     id: ident >>
///     item: switch!(value!(which),
///         "struct" => map!(
///             punct!(";"),
///             move |_| UnitType::Struct {
///                 name: id,
///             }
///         )
///         |
///         "enum" => map!(
///             delimited!(punct!("{"), ident, punct!("}")),
///             move |variant| UnitType::Enum {
///                 name: id,
///                 variant: variant,
///             }
///         )
///     ) >>
///     (item)
/// ));
///
/// fn main() {
///     let input = "struct S;";
///     let parsed = unit_type(input).expect("unit struct or enum");
///     println!("{:?}", parsed);
///
///     let input = "enum E { V }";
///     let parsed = unit_type(input).expect("unit struct or enum");
///     println!("{:?}", parsed);
/// }
/// ```
#[macro_export]
macro_rules! switch {
    ($i:expr, $submac:ident!( $($args:tt)* ), $($p:pat => $subrule:ident!( $($args2:tt)* ))|* ) => {
        match $submac!($i, $($args)*) {
            $crate::IResult::Error => $crate::IResult::Error,
            $crate::IResult::Done(i, o) => match o {
                $(
                    $p => $subrule!(i, $($args2)*),
                )*
                _ => $crate::IResult::Error,
            }
        }
    };
}

/// Produce the given value without parsing anything. Useful as an argument to
/// `switch!`.
///
/// - **Syntax:** `value!(VALUE)`
/// - **Output:** `VALUE`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::{Ident, Ty};
/// use syn::parse::{ident, ty};
///
/// #[derive(Debug)]
/// enum UnitType {
///     Struct {
///         name: Ident,
///     },
///     Enum {
///         name: Ident,
///         variant: Ident,
///     },
/// }
///
/// // Parse a unit struct or enum: either `struct S;` or `enum E { V }`.
/// named!(unit_type -> UnitType, do_parse!(
///     which: alt!(keyword!("struct") | keyword!("enum")) >>
///     id: ident >>
///     item: switch!(value!(which),
///         "struct" => map!(
///             punct!(";"),
///             move |_| UnitType::Struct {
///                 name: id,
///             }
///         )
///         |
///         "enum" => map!(
///             delimited!(punct!("{"), ident, punct!("}")),
///             move |variant| UnitType::Enum {
///                 name: id,
///                 variant: variant,
///             }
///         )
///     ) >>
///     (item)
/// ));
///
/// fn main() {
///     let input = "struct S;";
///     let parsed = unit_type(input).expect("unit struct or enum");
///     println!("{:?}", parsed);
///
///     let input = "enum E { V }";
///     let parsed = unit_type(input).expect("unit struct or enum");
///     println!("{:?}", parsed);
/// }
/// ```
#[macro_export]
macro_rules! value {
    ($i:expr, $res:expr) => {
        $crate::IResult::Done($i, $res)
    };
}

/// Value surrounded by a pair of delimiters.
///
/// - **Syntax:** `delimited!(OPEN, THING, CLOSE)`
/// - **Output:** `THING`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::Expr;
/// use syn::parse::expr;
///
/// // An expression surrounded by [[ ... ]].
/// named!(double_bracket_expr -> Expr,
///     delimited!(punct!("[["), expr, punct!("]]"))
/// );
///
/// fn main() {
///     let input = "[[ 1 + 1 ]]";
///
///     let parsed = double_bracket_expr(input).expect("double bracket expr");
///
///     println!("{:?}", parsed);
/// }
/// ```
#[macro_export]
macro_rules! delimited {
    ($i:expr, $submac:ident!( $($args:tt)* ), $($rest:tt)+) => {
        match tuple_parser!($i, (), $submac!($($args)*), $($rest)*) {
            $crate::IResult::Error => $crate::IResult::Error,
            $crate::IResult::Done(i1, (_, o, _)) => $crate::IResult::Done(i1, o)
        }
    };

    ($i:expr, $f:expr, $($rest:tt)+) => {
        delimited!($i, call!($f), $($rest)*)
    };
}

/// One or more values separated by some separator. Does not allow a trailing
/// separator.
///
/// - **Syntax:** `separated_nonempty_list!(SEPARATOR, THING)`
/// - **Output:** `Vec<THING>`
///
/// You may also be looking for:
///
/// - `separated_list!` - one or more values
/// - `terminated_list!` - zero or more, allows trailing separator
/// - `many0!` - zero or more, no separator
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::Ty;
/// use syn::parse::ty;
///
/// // One or more Rust types separated by commas.
/// named!(comma_separated_types -> Vec<Ty>,
///     separated_nonempty_list!(punct!(","), ty)
/// );
///
/// fn main() {
///     let input = "&str, Map<K, V>, String";
///
///     let parsed = comma_separated_types(input).expect("comma-separated types");
///
///     assert_eq!(parsed.len(), 3);
///     println!("{:?}", parsed);
/// }
/// ```
#[macro_export]
macro_rules! separated_nonempty_list {
    ($i:expr, $sep:ident!( $($args:tt)* ), $submac:ident!( $($args2:tt)* )) => {{
        let mut res   = ::std::vec::Vec::new();
        let mut input = $i;

        // get the first element
        match $submac!(input, $($args2)*) {
            $crate::IResult::Error => $crate::IResult::Error,
            $crate::IResult::Done(i, o) => {
                if i.len() == input.len() {
                    $crate::IResult::Error
                } else {
                    res.push(o);
                    input = i;

                    while let $crate::IResult::Done(i2, _) = $sep!(input, $($args)*) {
                        if i2.len() == input.len() {
                            break;
                        }

                        if let $crate::IResult::Done(i3, o3) = $submac!(i2, $($args2)*) {
                            if i3.len() == i2.len() {
                                break;
                            }
                            res.push(o3);
                            input = i3;
                        } else {
                            break;
                        }
                    }
                    $crate::IResult::Done(input, res)
                }
            }
        }
    }};

    ($i:expr, $submac:ident!( $($args:tt)* ), $g:expr) => {
        separated_nonempty_list!($i, $submac!($($args)*), call!($g))
    };

    ($i:expr, $f:expr, $submac:ident!( $($args:tt)* )) => {
        separated_nonempty_list!($i, call!($f), $submac!($($args)*))
    };

    ($i:expr, $f:expr, $g:expr) => {
        separated_nonempty_list!($i, call!($f), call!($g))
    };
}

/// Run a series of parsers and produce all of the results in a tuple.
///
/// - **Syntax:** `tuple!(A, B, C, ...)`
/// - **Output:** `(A, B, C, ...)`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::Ty;
/// use syn::parse::ty;
///
/// named!(two_types -> (Ty, Ty), tuple!(ty, ty));
///
/// fn main() {
///     let input = "&str  Map<K, V>";
///
///     let parsed = two_types(input).expect("two types");
///
///     println!("{:?}", parsed);
/// }
/// ```
#[macro_export]
macro_rules! tuple {
    ($i:expr, $($rest:tt)*) => {
        tuple_parser!($i, (), $($rest)*)
    };
}

/// Internal parser, do not use directly.
#[doc(hidden)]
#[macro_export]
macro_rules! tuple_parser {
    ($i:expr, ($($parsed:tt),*), $e:ident, $($rest:tt)*) => {
        tuple_parser!($i, ($($parsed),*), call!($e), $($rest)*)
    };

    ($i:expr, (), $submac:ident!( $($args:tt)* ), $($rest:tt)*) => {
        match $submac!($i, $($args)*) {
            $crate::IResult::Error => $crate::IResult::Error,
            $crate::IResult::Done(i, o) =>
                tuple_parser!(i, (o), $($rest)*),
        }
    };

    ($i:expr, ($($parsed:tt)*), $submac:ident!( $($args:tt)* ), $($rest:tt)*) => {
        match $submac!($i, $($args)*) {
            $crate::IResult::Error => $crate::IResult::Error,
            $crate::IResult::Done(i, o) =>
                tuple_parser!(i, ($($parsed)* , o), $($rest)*),
        }
    };

    ($i:expr, ($($parsed:tt),*), $e:ident) => {
        tuple_parser!($i, ($($parsed),*), call!($e))
    };

    ($i:expr, (), $submac:ident!( $($args:tt)* )) => {
        $submac!($i, $($args)*)
    };

    ($i:expr, ($($parsed:expr),*), $submac:ident!( $($args:tt)* )) => {
        match $submac!($i, $($args)*) {
            $crate::IResult::Error => $crate::IResult::Error,
            $crate::IResult::Done(i, o) => $crate::IResult::Done(i, ($($parsed),*, o))
        }
    };

    ($i:expr, ($($parsed:expr),*)) => {
        $crate::IResult::Done($i, ($($parsed),*))
    };
}

/// Run a series of parsers, returning the result of the first one which
/// succeeds.
///
/// Optionally allows for the result to be transformed.
///
/// - **Syntax:** `alt!(THING1 | THING2 => { FUNC } | ...)`
/// - **Output:** `T`, the return type of `THING1` and `FUNC(THING2)` and ...
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::Ident;
/// use syn::parse::ident;
///
/// named!(ident_or_bang -> Ident,
///     alt!(
///         ident
///         |
///         punct!("!") => { |_| "BANG".into() }
///     )
/// );
///
/// fn main() {
///     let input = "foo";
///     let parsed = ident_or_bang(input).expect("identifier or `!`");
///     assert_eq!(parsed, "foo");
///
///     let input = "!";
///     let parsed = ident_or_bang(input).expect("identifier or `!`");
///     assert_eq!(parsed, "BANG");
/// }
/// ```
#[macro_export]
macro_rules! alt {
    ($i:expr, $e:ident | $($rest:tt)*) => {
        alt!($i, call!($e) | $($rest)*)
    };

    ($i:expr, $subrule:ident!( $($args:tt)*) | $($rest:tt)*) => {
        match $subrule!($i, $($args)*) {
            res @ $crate::IResult::Done(_, _) => res,
            _ => alt!($i, $($rest)*)
        }
    };

    ($i:expr, $subrule:ident!( $($args:tt)* ) => { $gen:expr } | $($rest:tt)+) => {
        match $subrule!($i, $($args)*) {
            $crate::IResult::Done(i, o) => $crate::IResult::Done(i, $gen(o)),
            $crate::IResult::Error => alt!($i, $($rest)*)
        }
    };

    ($i:expr, $e:ident => { $gen:expr } | $($rest:tt)*) => {
        alt!($i, call!($e) => { $gen } | $($rest)*)
    };

    ($i:expr, $e:ident => { $gen:expr }) => {
        alt!($i, call!($e) => { $gen })
    };

    ($i:expr, $subrule:ident!( $($args:tt)* ) => { $gen:expr }) => {
        match $subrule!($i, $($args)*) {
            $crate::IResult::Done(i, o) => $crate::IResult::Done(i, $gen(o)),
            $crate::IResult::Error => $crate::IResult::Error,
        }
    };

    ($i:expr, $e:ident) => {
        alt!($i, call!($e))
    };

    ($i:expr, $subrule:ident!( $($args:tt)*)) => {
        $subrule!($i, $($args)*)
    };
}

/// Run a series of parsers, one after another, optionally assigning the results
/// a name. Fail if any of the parsers fails.
///
/// Produces the result of evaluating the final expression in parentheses with
/// all of the previously named results bound.
///
/// - **Syntax:** `do_parse!(name: THING1 >> THING2 >> (RESULT))`
/// - **Output:** `RESULT`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::{Ident, TokenTree};
/// use syn::parse::{ident, tt};
///
/// // Parse a macro invocation like `stringify!($args)`.
/// named!(simple_mac -> (Ident, TokenTree), do_parse!(
///     name: ident >>
///     punct!("!") >>
///     body: tt >>
///     (name, body)
/// ));
///
/// fn main() {
///     let input = "stringify!($args)";
///     let (name, body) = simple_mac(input).expect("macro invocation");
///     println!("{:?}", name);
///     println!("{:?}", body);
/// }
/// ```
#[macro_export]
macro_rules! do_parse {
    ($i:expr, ( $($rest:expr),* )) => {
        $crate::IResult::Done($i, ( $($rest),* ))
    };

    ($i:expr, $e:ident >> $($rest:tt)*) => {
        do_parse!($i, call!($e) >> $($rest)*)
    };

    ($i:expr, $submac:ident!( $($args:tt)* ) >> $($rest:tt)*) => {
        match $submac!($i, $($args)*) {
            $crate::IResult::Error => $crate::IResult::Error,
            $crate::IResult::Done(i, _) =>
                do_parse!(i, $($rest)*),
        }
    };

    ($i:expr, $field:ident : $e:ident >> $($rest:tt)*) => {
        do_parse!($i, $field: call!($e) >> $($rest)*)
    };

    ($i:expr, $field:ident : $submac:ident!( $($args:tt)* ) >> $($rest:tt)*) => {
        match $submac!($i, $($args)*) {
            $crate::IResult::Error => $crate::IResult::Error,
            $crate::IResult::Done(i, o) => {
                let $field = o;
                do_parse!(i, $($rest)*)
            },
        }
    };

    ($i:expr, mut $field:ident : $e:ident >> $($rest:tt)*) => {
        do_parse!($i, mut $field: call!($e) >> $($rest)*)
    };

    ($i:expr, mut $field:ident : $submac:ident!( $($args:tt)* ) >> $($rest:tt)*) => {
        match $submac!($i, $($args)*) {
            $crate::IResult::Error => $crate::IResult::Error,
            $crate::IResult::Done(i, o) => {
                let mut $field = o;
                do_parse!(i, $($rest)*)
            },
        }
    };
}
