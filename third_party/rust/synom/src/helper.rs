use IResult;
use space::{skip_whitespace, word_break};

/// Parse a piece of punctuation like "+" or "+=".
///
/// See also `keyword!` for parsing keywords, which are subtly different from
/// punctuation.
///
/// - **Syntax:** `punct!("...")`
/// - **Output:** `&str`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// // Parse zero or more bangs.
/// named!(many_bangs -> Vec<&str>,
///     many0!(punct!("!"))
/// );
///
/// fn main() {
///     let input = "!! !";
///     let parsed = many_bangs(input).expect("bangs");
///     assert_eq!(parsed, ["!", "!", "!"]);
/// }
/// ```
#[macro_export]
macro_rules! punct {
    ($i:expr, $punct:expr) => {
        $crate::helper::punct($i, $punct)
    };
}

// Not public API.
#[doc(hidden)]
pub fn punct<'a>(input: &'a str, token: &'static str) -> IResult<&'a str, &'a str> {
    let input = skip_whitespace(input);
    if input.starts_with(token) {
        IResult::Done(&input[token.len()..], token)
    } else {
        IResult::Error
    }
}

/// Parse a keyword like "fn" or "struct".
///
/// See also `punct!` for parsing punctuation, which are subtly different from
/// keywords.
///
/// - **Syntax:** `keyword!("...")`
/// - **Output:** `&str`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use synom::IResult;
///
/// // Parse zero or more "bang" keywords.
/// named!(many_bangs -> Vec<&str>,
///     terminated!(
///         many0!(keyword!("bang")),
///         punct!(";")
///     )
/// );
///
/// fn main() {
///     let input = "bang bang bang;";
///     let parsed = many_bangs(input).expect("bangs");
///     assert_eq!(parsed, ["bang", "bang", "bang"]);
///
///     let input = "bangbang;";
///     let err = many_bangs(input);
///     assert_eq!(err, IResult::Error);
/// }
/// ```
#[macro_export]
macro_rules! keyword {
    ($i:expr, $keyword:expr) => {
        $crate::helper::keyword($i, $keyword)
    };
}

// Not public API.
#[doc(hidden)]
pub fn keyword<'a>(input: &'a str, token: &'static str) -> IResult<&'a str, &'a str> {
    match punct(input, token) {
        IResult::Done(rest, _) => {
            match word_break(rest) {
                IResult::Done(_, _) => IResult::Done(rest, token),
                IResult::Error => IResult::Error,
            }
        }
        IResult::Error => IResult::Error,
    }
}

/// Turn a failed parse into `None` and a successful parse into `Some`.
///
/// - **Syntax:** `option!(THING)`
/// - **Output:** `Option<THING>`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// named!(maybe_bang -> Option<&str>, option!(punct!("!")));
///
/// fn main() {
///     let input = "!";
///     let parsed = maybe_bang(input).expect("maybe bang");
///     assert_eq!(parsed, Some("!"));
///
///     let input = "";
///     let parsed = maybe_bang(input).expect("maybe bang");
///     assert_eq!(parsed, None);
/// }
/// ```
#[macro_export]
macro_rules! option {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {
        match $submac!($i, $($args)*) {
            $crate::IResult::Done(i, o) => $crate::IResult::Done(i, Some(o)),
            $crate::IResult::Error => $crate::IResult::Done($i, None),
        }
    };

    ($i:expr, $f:expr) => {
        option!($i, call!($f));
    };
}

/// Turn a failed parse into an empty vector. The argument parser must itself
/// return a vector.
///
/// This is often more convenient than `option!(...)` when the argument produces
/// a vector.
///
/// - **Syntax:** `opt_vec!(THING)`
/// - **Output:** `THING`, which must be `Vec<T>`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::{Lifetime, Ty};
/// use syn::parse::{lifetime, ty};
///
/// named!(bound_lifetimes -> (Vec<Lifetime>, Ty), tuple!(
///     opt_vec!(do_parse!(
///         keyword!("for") >>
///         punct!("<") >>
///         lifetimes: terminated_list!(punct!(","), lifetime) >>
///         punct!(">") >>
///         (lifetimes)
///     )),
///     ty
/// ));
///
/// fn main() {
///     let input = "for<'a, 'b> fn(&'a A) -> &'b B";
///     let parsed = bound_lifetimes(input).expect("bound lifetimes");
///     assert_eq!(parsed.0, [Lifetime::new("'a"), Lifetime::new("'b")]);
///     println!("{:?}", parsed);
///
///     let input = "From<String>";
///     let parsed = bound_lifetimes(input).expect("bound lifetimes");
///     assert!(parsed.0.is_empty());
///     println!("{:?}", parsed);
/// }
/// ```
#[macro_export]
macro_rules! opt_vec {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {
        match $submac!($i, $($args)*) {
            $crate::IResult::Done(i, o) => $crate::IResult::Done(i, o),
            $crate::IResult::Error => $crate::IResult::Done($i, Vec::new()),
        }
    };
}

/// Parses nothing and always succeeds.
///
/// This can be useful as a fallthrough case in `alt!`.
///
/// - **Syntax:** `epsilon!()`
/// - **Output:** `()`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::Mutability;
///
/// named!(mutability -> Mutability, alt!(
///     keyword!("mut") => { |_| Mutability::Mutable }
///     |
///     epsilon!() => { |_| Mutability::Immutable }
/// ));
///
/// fn main() {
///     let input = "mut";
///     let parsed = mutability(input).expect("mutability");
///     assert_eq!(parsed, Mutability::Mutable);
///
///     let input = "";
///     let parsed = mutability(input).expect("mutability");
///     assert_eq!(parsed, Mutability::Immutable);
/// }
/// ```
#[macro_export]
macro_rules! epsilon {
    ($i:expr,) => {
        $crate::IResult::Done($i, ())
    };
}

/// Run a parser, binding the result to a name, and then evaluating an
/// expression.
///
/// Discards the result of the expression and parser.
///
/// - **Syntax:** `tap!(NAME : THING => EXPR)`
/// - **Output:** `()`
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::{Expr, ExprKind};
/// use syn::parse::expr;
///
/// named!(expr_with_arrow_call -> Expr, do_parse!(
///     mut e: expr >>
///     many0!(tap!(arg: tuple!(punct!("=>"), expr) => {
///         e = Expr {
///             node: ExprKind::Call(Box::new(e), vec![arg.1]),
///             attrs: Vec::new(),
///         };
///     })) >>
///     (e)
/// ));
///
/// fn main() {
///     let input = "something => argument1 => argument2";
///
///     let parsed = expr_with_arrow_call(input).expect("expr with arrow call");
///
///     println!("{:?}", parsed);
/// }
/// ```
#[doc(hidden)]
#[macro_export]
macro_rules! tap {
    ($i:expr, $name:ident : $submac:ident!( $($args:tt)* ) => $e:expr) => {
        match $submac!($i, $($args)*) {
            $crate::IResult::Done(i, o) => {
                let $name = o;
                $e;
                $crate::IResult::Done(i, ())
            }
            $crate::IResult::Error => $crate::IResult::Error,
        }
    };

    ($i:expr, $name:ident : $f:expr => $e:expr) => {
        tap!($i, $name: call!($f) => $e);
    };
}

/// Zero or more values separated by some separator. Does not allow a trailing
/// seperator.
///
/// - **Syntax:** `separated_list!(punct!("..."), THING)`
/// - **Output:** `Vec<THING>`
///
/// You may also be looking for:
///
/// - `separated_nonempty_list!` - one or more values
/// - `terminated_list!` - zero or more, allows trailing separator
/// - `many0!` - zero or more, no separator
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::Expr;
/// use syn::parse::expr;
///
/// named!(expr_list -> Vec<Expr>,
///     separated_list!(punct!(","), expr)
/// );
///
/// fn main() {
///     let input = "1 + 1, things, Construct { this: thing }";
///
///     let parsed = expr_list(input).expect("expr list");
///     assert_eq!(parsed.len(), 3);
/// }
/// ```
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::Ident;
/// use syn::parse::ident;
///
/// named!(run_on -> Vec<Ident>,
///     terminated!(
///         separated_list!(keyword!("and"), preceded!(punct!("$"), ident)),
///         punct!("...")
///     )
/// );
///
/// fn main() {
///     let input = "$expr and $ident and $pat ...";
///
///     let parsed = run_on(input).expect("run-on sentence");
///     assert_eq!(parsed.len(), 3);
///     assert_eq!(parsed[0], "expr");
///     assert_eq!(parsed[1], "ident");
///     assert_eq!(parsed[2], "pat");
/// }
/// ```
#[macro_export]
macro_rules! separated_list {
    // Try to use this branch if possible - makes a difference in compile time.
    ($i:expr, punct!($sep:expr), $f:ident) => {
        $crate::helper::separated_list($i, $sep, $f, false)
    };

    ($i:expr, $sepmac:ident!( $($separgs:tt)* ), $fmac:ident!( $($fargs:tt)* )) => {{
        let mut res = ::std::vec::Vec::new();
        let mut input = $i;

        // get the first element
        match $fmac!(input, $($fargs)*) {
            $crate::IResult::Error => $crate::IResult::Done(input, res),
            $crate::IResult::Done(i, o) => {
                if i.len() == input.len() {
                    $crate::IResult::Error
                } else {
                    res.push(o);
                    input = i;

                    // get the separator first
                    while let $crate::IResult::Done(i2, _) = $sepmac!(input, $($separgs)*) {
                        if i2.len() == input.len() {
                            break;
                        }

                        // get the element next
                        if let $crate::IResult::Done(i3, o3) = $fmac!(i2, $($fargs)*) {
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

    ($i:expr, $sepmac:ident!( $($separgs:tt)* ), $f:expr) => {
        separated_list!($i, $sepmac!($($separgs)*), call!($f))
    };

    ($i:expr, $sep:expr, $fmac:ident!( $($fargs:tt)* )) => {
        separated_list!($i, call!($sep), $fmac!($($fargs)*))
    };

    ($i:expr, $sep:expr, $f:expr) => {
        separated_list!($i, call!($sep), call!($f))
    };
}

/// Zero or more values separated by some separator. A trailing separator is
/// allowed.
///
/// - **Syntax:** `terminated_list!(punct!("..."), THING)`
/// - **Output:** `Vec<THING>`
///
/// You may also be looking for:
///
/// - `separated_list!` - zero or more, allows trailing separator
/// - `separated_nonempty_list!` - one or more values
/// - `many0!` - zero or more, no separator
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::Expr;
/// use syn::parse::expr;
///
/// named!(expr_list -> Vec<Expr>,
///     terminated_list!(punct!(","), expr)
/// );
///
/// fn main() {
///     let input = "1 + 1, things, Construct { this: thing },";
///
///     let parsed = expr_list(input).expect("expr list");
///     assert_eq!(parsed.len(), 3);
/// }
/// ```
///
/// ```rust
/// extern crate syn;
/// #[macro_use] extern crate synom;
///
/// use syn::Ident;
/// use syn::parse::ident;
///
/// named!(run_on -> Vec<Ident>,
///     terminated!(
///         terminated_list!(keyword!("and"), preceded!(punct!("$"), ident)),
///         punct!("...")
///     )
/// );
///
/// fn main() {
///     let input = "$expr and $ident and $pat and ...";
///
///     let parsed = run_on(input).expect("run-on sentence");
///     assert_eq!(parsed.len(), 3);
///     assert_eq!(parsed[0], "expr");
///     assert_eq!(parsed[1], "ident");
///     assert_eq!(parsed[2], "pat");
/// }
/// ```
#[macro_export]
macro_rules! terminated_list {
    // Try to use this branch if possible - makes a difference in compile time.
    ($i:expr, punct!($sep:expr), $f:ident) => {
        $crate::helper::separated_list($i, $sep, $f, true)
    };

    ($i:expr, $sepmac:ident!( $($separgs:tt)* ), $fmac:ident!( $($fargs:tt)* )) => {{
        let mut res = ::std::vec::Vec::new();
        let mut input = $i;

        // get the first element
        match $fmac!(input, $($fargs)*) {
            $crate::IResult::Error => $crate::IResult::Done(input, res),
            $crate::IResult::Done(i, o) => {
                if i.len() == input.len() {
                    $crate::IResult::Error
                } else {
                    res.push(o);
                    input = i;

                    // get the separator first
                    while let $crate::IResult::Done(i2, _) = $sepmac!(input, $($separgs)*) {
                        if i2.len() == input.len() {
                            break;
                        }

                        // get the element next
                        if let $crate::IResult::Done(i3, o3) = $fmac!(i2, $($fargs)*) {
                            if i3.len() == i2.len() {
                                break;
                            }
                            res.push(o3);
                            input = i3;
                        } else {
                            break;
                        }
                    }
                    if let $crate::IResult::Done(after, _) = $sepmac!(input, $($separgs)*) {
                        input = after;
                    }
                    $crate::IResult::Done(input, res)
                }
            }
        }
    }};

    ($i:expr, $sepmac:ident!( $($separgs:tt)* ), $f:expr) => {
        terminated_list!($i, $sepmac!($($separgs)*), call!($f))
    };

    ($i:expr, $sep:expr, $fmac:ident!( $($fargs:tt)* )) => {
        terminated_list!($i, call!($sep), $fmac!($($fargs)*))
    };

    ($i:expr, $sep:expr, $f:expr) => {
        terminated_list!($i, call!($sep), call!($f))
    };
}

// Not public API.
#[doc(hidden)]
pub fn separated_list<'a, T>(mut input: &'a str,
                             sep: &'static str,
                             f: fn(&'a str) -> IResult<&'a str, T>,
                             terminated: bool)
                             -> IResult<&'a str, Vec<T>> {
    let mut res = Vec::new();

    // get the first element
    match f(input) {
        IResult::Error => IResult::Done(input, res),
        IResult::Done(i, o) => {
            if i.len() == input.len() {
                IResult::Error
            } else {
                res.push(o);
                input = i;

                // get the separator first
                while let IResult::Done(i2, _) = punct(input, sep) {
                    if i2.len() == input.len() {
                        break;
                    }

                    // get the element next
                    if let IResult::Done(i3, o3) = f(i2) {
                        if i3.len() == i2.len() {
                            break;
                        }
                        res.push(o3);
                        input = i3;
                    } else {
                        break;
                    }
                }
                if terminated {
                    if let IResult::Done(after, _) = punct(input, sep) {
                        input = after;
                    }
                }
                IResult::Done(input, res)
            }
        }
    }
}
