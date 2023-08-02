//! Implementations of [`fmt`]-like derive macros.
//!
//! [`fmt`]: std::fmt

#[cfg(feature = "debug")]
pub(crate) mod debug;
#[cfg(feature = "display")]
pub(crate) mod display;
mod parsing;

use proc_macro2::TokenStream;
use quote::ToTokens;
use syn::{
    parse::{Parse, ParseStream},
    punctuated::Punctuated,
    spanned::Spanned as _,
    token, Ident,
};

use crate::parsing::Expr;

/// Representation of a macro attribute expressing additional trait bounds.
#[derive(Debug, Default)]
struct BoundsAttribute(Punctuated<syn::WherePredicate, syn::token::Comma>);

impl BoundsAttribute {
    /// Errors in case legacy syntax is encountered: `bound = "..."`.
    fn check_legacy_fmt(input: ParseStream<'_>) -> syn::Result<()> {
        let fork = input.fork();

        let path = fork
            .parse::<syn::Path>()
            .and_then(|path| fork.parse::<syn::token::Eq>().map(|_| path));
        match path {
            Ok(path) if path.is_ident("bound") => fork
                .parse::<syn::Lit>()
                .ok()
                .and_then(|lit| match lit {
                    syn::Lit::Str(s) => Some(s.value()),
                    _ => None,
                })
                .map_or(Ok(()), |bound| {
                    Err(syn::Error::new(
                        input.span(),
                        format!("legacy syntax, use `bound({bound})` instead"),
                    ))
                }),
            Ok(_) | Err(_) => Ok(()),
        }
    }
}

impl Parse for BoundsAttribute {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        let _ = input.parse::<syn::Path>().and_then(|p| {
            if ["bound", "bounds", "where"]
                .into_iter()
                .any(|i| p.is_ident(i))
            {
                Ok(p)
            } else {
                Err(syn::Error::new(
                    p.span(),
                    "unknown attribute, expected `bound(...)`",
                ))
            }
        })?;

        let content;
        syn::parenthesized!(content in input);

        content
            .parse_terminated(syn::WherePredicate::parse, token::Comma)
            .map(Self)
    }
}

/// Representation of a [`fmt`]-like attribute.
///
/// [`fmt`]: std::fmt
#[derive(Debug)]
struct FmtAttribute {
    /// Interpolation [`syn::LitStr`].
    ///
    /// [`syn::LitStr`]: struct@syn::LitStr
    lit: syn::LitStr,

    /// Optional [`token::Comma`].
    comma: Option<token::Comma>,

    /// Interpolation arguments.
    args: Punctuated<FmtArgument, token::Comma>,
}

impl FmtAttribute {
    /// Returns an [`Iterator`] over bounded [`syn::Type`]s and trait names.
    fn bounded_types<'a>(
        &'a self,
        fields: &'a syn::Fields,
    ) -> impl Iterator<Item = (&'a syn::Type, &'static str)> {
        let placeholders = Placeholder::parse_fmt_string(&self.lit.value());

        // We ignore unknown fields, as compiler will produce better error messages.
        placeholders.into_iter().filter_map(move |placeholder| {
            let name = match placeholder.arg {
                Parameter::Named(name) => self
                    .args
                    .iter()
                    .find_map(|a| (a.alias()? == &name).then_some(&a.expr))
                    .map_or(Some(name), |expr| expr.ident().map(ToString::to_string))?,
                Parameter::Positional(i) => self
                    .args
                    .iter()
                    .nth(i)
                    .and_then(|a| a.expr.ident().filter(|_| a.alias.is_none()))?
                    .to_string(),
            };

            let unnamed = name.strip_prefix('_').and_then(|s| s.parse().ok());
            let ty = match (&fields, unnamed) {
                (syn::Fields::Unnamed(f), Some(i)) => {
                    f.unnamed.iter().nth(i).map(|f| &f.ty)
                }
                (syn::Fields::Named(f), None) => f.named.iter().find_map(|f| {
                    f.ident.as_ref().filter(|s| **s == name).map(|_| &f.ty)
                }),
                _ => None,
            }?;

            Some((ty, placeholder.trait_name))
        })
    }

    /// Errors in case legacy syntax is encountered: `fmt = "...", (arg),*`.
    fn check_legacy_fmt(input: ParseStream<'_>) -> syn::Result<()> {
        let fork = input.fork();

        let path = fork
            .parse::<syn::Path>()
            .and_then(|path| fork.parse::<syn::token::Eq>().map(|_| path));
        match path {
            Ok(path) if path.is_ident("fmt") => (|| {
                let args = fork
                    .parse_terminated(syn::Lit::parse, token::Comma)
                    .ok()?
                    .into_iter()
                    .enumerate()
                    .filter_map(|(i, lit)| match lit {
                        syn::Lit::Str(str) => Some(if i == 0 {
                            format!("\"{}\"", str.value())
                        } else {
                            str.value()
                        }),
                        _ => None,
                    })
                    .collect::<Vec<_>>();
                (!args.is_empty()).then_some(args)
            })()
            .map_or(Ok(()), |fmt| {
                Err(syn::Error::new(
                    input.span(),
                    format!(
                        "legacy syntax, remove `fmt =` and use `{}` instead",
                        fmt.join(", "),
                    ),
                ))
            }),
            Ok(_) | Err(_) => Ok(()),
        }
    }
}

impl Parse for FmtAttribute {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        Ok(Self {
            lit: input.parse()?,
            comma: input
                .peek(syn::token::Comma)
                .then(|| input.parse())
                .transpose()?,
            args: input.parse_terminated(FmtArgument::parse, token::Comma)?,
        })
    }
}

impl ToTokens for FmtAttribute {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        self.lit.to_tokens(tokens);
        self.comma.to_tokens(tokens);
        self.args.to_tokens(tokens);
    }
}

/// Representation of a [named parameter][1] (`identifier '=' expression`) in
/// in a [`FmtAttribute`].
///
/// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#named-parameters
#[derive(Debug)]
struct FmtArgument {
    /// `identifier =` [`Ident`].
    alias: Option<(Ident, token::Eq)>,

    /// `expression` [`Expr`].
    expr: Expr,
}

impl FmtArgument {
    /// Returns an `identifier` of the [named parameter][1].
    ///
    /// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#named-parameters
    fn alias(&self) -> Option<&Ident> {
        self.alias.as_ref().map(|(ident, _)| ident)
    }
}

impl Parse for FmtArgument {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        Ok(Self {
            alias: (input.peek(Ident) && input.peek2(token::Eq))
                .then(|| Ok::<_, syn::Error>((input.parse()?, input.parse()?)))
                .transpose()?,
            expr: input.parse()?,
        })
    }
}

impl ToTokens for FmtArgument {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        if let Some((ident, eq)) = &self.alias {
            ident.to_tokens(tokens);
            eq.to_tokens(tokens);
        }
        self.expr.to_tokens(tokens);
    }
}

/// Representation of a [parameter][1] used in a [`Placeholder`].
///
/// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#formatting-parameters
#[derive(Debug, Eq, PartialEq)]
enum Parameter {
    /// [Positional parameter][1].
    ///
    /// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#positional-parameters
    Positional(usize),

    /// [Named parameter][1].
    ///
    /// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#named-parameters
    Named(String),
}

impl<'a> From<parsing::Argument<'a>> for Parameter {
    fn from(arg: parsing::Argument<'a>) -> Self {
        match arg {
            parsing::Argument::Integer(i) => Self::Positional(i),
            parsing::Argument::Identifier(i) => Self::Named(i.to_owned()),
        }
    }
}

/// Representation of a formatting placeholder.
#[derive(Debug, PartialEq, Eq)]
struct Placeholder {
    /// Formatting argument (either named or positional) to be used by this placeholder.
    arg: Parameter,

    /// [Width parameter][1], if present.
    ///
    /// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#width
    width: Option<Parameter>,

    /// [Precision parameter][1], if present.
    ///
    /// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#precision
    precision: Option<Parameter>,

    /// Name of [`std::fmt`] trait to be used for rendering this placeholder.
    trait_name: &'static str,
}

impl Placeholder {
    /// Parses [`Placeholder`]s from the provided formatting string.
    fn parse_fmt_string(s: &str) -> Vec<Self> {
        let mut n = 0;
        parsing::format_string(s)
            .into_iter()
            .flat_map(|f| f.formats)
            .map(|format| {
                let (maybe_arg, ty) = (
                    format.arg,
                    format.spec.map(|s| s.ty).unwrap_or(parsing::Type::Display),
                );
                let position = maybe_arg.map(Into::into).unwrap_or_else(|| {
                    // Assign "the next argument".
                    // https://doc.rust-lang.org/stable/std/fmt/index.html#positional-parameters
                    n += 1;
                    Parameter::Positional(n - 1)
                });

                Self {
                    arg: position,
                    width: format.spec.and_then(|s| match s.width {
                        Some(parsing::Count::Parameter(arg)) => Some(arg.into()),
                        _ => None,
                    }),
                    precision: format.spec.and_then(|s| match s.precision {
                        Some(parsing::Precision::Count(parsing::Count::Parameter(
                            arg,
                        ))) => Some(arg.into()),
                        _ => None,
                    }),
                    trait_name: ty.trait_name(),
                }
            })
            .collect()
    }
}

#[cfg(test)]
mod fmt_attribute_spec {
    use itertools::Itertools as _;
    use quote::ToTokens;
    use syn;

    use super::FmtAttribute;

    fn assert<'a>(input: &'a str, parsed: impl AsRef<[&'a str]>) {
        let parsed = parsed.as_ref();
        let attr = syn::parse_str::<FmtAttribute>(&format!("\"\", {}", input)).unwrap();
        let fmt_args = attr
            .args
            .into_iter()
            .map(|arg| arg.into_token_stream().to_string())
            .collect::<Vec<String>>();
        fmt_args.iter().zip_eq(parsed).enumerate().for_each(
            |(i, (found, expected))| {
                assert_eq!(
                    *expected, found,
                    "Mismatch at index {i}\n\
                     Expected: {parsed:?}\n\
                     Found: {fmt_args:?}",
                );
            },
        );
    }

    #[test]
    fn cases() {
        let cases = [
            "ident",
            "alias = ident",
            "[a , b , c , d]",
            "counter += 1",
            "async { fut . await }",
            "a < b",
            "a > b",
            "{ let x = (a , b) ; }",
            "invoke (a , b)",
            "foo as f64",
            "| a , b | a + b",
            "obj . k",
            "for pat in expr { break pat ; }",
            "if expr { true } else { false }",
            "vector [2]",
            "1",
            "\"foo\"",
            "loop { break i ; }",
            "format ! (\"{}\" , q)",
            "match n { Some (n) => { } , None => { } }",
            "x . foo ::< T > (a , b)",
            "x . foo ::< T < [T < T >; if a < b { 1 } else { 2 }] >, { a < b } > (a , b)",
            "(a + b)",
            "i32 :: MAX",
            "1 .. 2",
            "& a",
            "[0u8 ; N]",
            "(a , b , c , d)",
            "< Ty as Trait > :: T",
            "< Ty < Ty < T >, { a < b } > as Trait < T > > :: T",
        ];

        assert("", []);
        for i in 1..4 {
            for permutations in cases.into_iter().permutations(i) {
                let mut input = permutations.clone().join(",");
                assert(&input, &permutations);
                input.push(',');
                assert(&input, &permutations);
            }
        }
    }
}

#[cfg(test)]
mod placeholder_parse_fmt_string_spec {
    use super::{Parameter, Placeholder};

    #[test]
    fn indicates_position_and_trait_name_for_each_fmt_placeholder() {
        let fmt_string = "{},{:?},{{}},{{{1:0$}}}-{2:.1$x}{par:#?}{:width$}";
        assert_eq!(
            Placeholder::parse_fmt_string(&fmt_string),
            vec![
                Placeholder {
                    arg: Parameter::Positional(0),
                    width: None,
                    precision: None,
                    trait_name: "Display",
                },
                Placeholder {
                    arg: Parameter::Positional(1),
                    width: None,
                    precision: None,
                    trait_name: "Debug",
                },
                Placeholder {
                    arg: Parameter::Positional(1),
                    width: Some(Parameter::Positional(0)),
                    precision: None,
                    trait_name: "Display",
                },
                Placeholder {
                    arg: Parameter::Positional(2),
                    width: None,
                    precision: Some(Parameter::Positional(1)),
                    trait_name: "LowerHex",
                },
                Placeholder {
                    arg: Parameter::Named("par".to_owned()),
                    width: None,
                    precision: None,
                    trait_name: "Debug",
                },
                Placeholder {
                    arg: Parameter::Positional(2),
                    width: Some(Parameter::Named("width".to_owned())),
                    precision: None,
                    trait_name: "Display",
                },
            ],
        );
    }
}
