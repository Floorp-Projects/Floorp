use std::cell::RefCell;
use std::collections::hash_map::{Entry, HashMap};
use std::hash::BuildHasher;
use std::rc::Rc;
use std::sync::atomic::AtomicBool;
use std::sync::Arc;

use ident_case;
use syn::{self, Lit, Meta, NestedMeta};

use {Error, Result};

/// Create an instance from an item in an attribute declaration.
///
/// # Implementing `FromMeta`
/// * Do not take a dependency on the `ident` of the passed-in meta item. The ident will be set by the field name of the containing struct.
/// * Implement only the `from_*` methods that you intend to support. The default implementations will return useful errors.
///
/// # Provided Implementations
/// ## bool
///
/// * Word with no value specified - becomes `true`.
/// * As a boolean literal, e.g. `foo = true`.
/// * As a string literal, e.g. `foo = "true"`.
///
/// ## String
/// * As a string literal, e.g. `foo = "hello"`.
/// * As a raw string literal, e.g. `foo = r#"hello "world""#`.
///
/// ## Number
/// * As a string literal, e.g. `foo = "-25"`.
/// * As an unquoted positive value, e.g. `foo = 404`. Negative numbers must be in quotation marks.
///
/// ## ()
/// * Word with no value specified, e.g. `foo`. This is best used with `Option`.
///   See `darling::util::Flag` for a more strongly-typed alternative.
///
/// ## Option
/// * Any format produces `Some`.
///
/// ## `Result<T, darling::Error>`
/// * Allows for fallible parsing; will populate the target field with the result of the
///   parse attempt.
pub trait FromMeta: Sized {
    fn from_nested_meta(item: &NestedMeta) -> Result<Self> {
        (match *item {
            NestedMeta::Lit(ref lit) => Self::from_value(lit),
            NestedMeta::Meta(ref mi) => Self::from_meta(mi),
        })
        .map_err(|e| e.with_span(item))
    }

    /// Create an instance from a `syn::Meta` by dispatching to the format-appropriate
    /// trait function. This generally should not be overridden by implementers.
    ///
    /// # Error Spans
    /// If this method is overridden and can introduce errors that weren't passed up from
    /// other `from_meta` calls, the override must call `with_span` on the error using the
    /// `item` to make sure that the emitted diagnostic points to the correct location in
    /// source code.
    fn from_meta(item: &Meta) -> Result<Self> {
        (match *item {
            Meta::Path(_) => Self::from_word(),
            Meta::List(ref value) => Self::from_list(
                &value
                    .nested
                    .iter()
                    .cloned()
                    .collect::<Vec<syn::NestedMeta>>()[..],
            ),
            Meta::NameValue(ref value) => Self::from_value(&value.lit),
        })
        .map_err(|e| e.with_span(item))
    }

    /// Create an instance from the presence of the word in the attribute with no
    /// additional options specified.
    fn from_word() -> Result<Self> {
        Err(Error::unsupported_format("word"))
    }

    /// Create an instance from a list of nested meta items.
    #[allow(unused_variables)]
    fn from_list(items: &[NestedMeta]) -> Result<Self> {
        Err(Error::unsupported_format("list"))
    }

    /// Create an instance from a literal value of either `foo = "bar"` or `foo("bar")`.
    /// This dispatches to the appropriate method based on the type of literal encountered,
    /// and generally should not be overridden by implementers.
    ///
    /// # Error Spans
    /// If this method is overridden, the override must make sure to add `value`'s span
    /// information to the returned error by calling `with_span(value)` on the `Error` instance.
    fn from_value(value: &Lit) -> Result<Self> {
        (match *value {
            Lit::Bool(ref b) => Self::from_bool(b.value),
            Lit::Str(ref s) => Self::from_string(&s.value()),
            _ => Err(Error::unexpected_lit_type(value)),
        })
        .map_err(|e| e.with_span(value))
    }

    /// Create an instance from a char literal in a value position.
    #[allow(unused_variables)]
    fn from_char(value: char) -> Result<Self> {
        Err(Error::unexpected_type("char"))
    }

    /// Create an instance from a string literal in a value position.
    #[allow(unused_variables)]
    fn from_string(value: &str) -> Result<Self> {
        Err(Error::unexpected_type("string"))
    }

    /// Create an instance from a bool literal in a value position.
    #[allow(unused_variables)]
    fn from_bool(value: bool) -> Result<Self> {
        Err(Error::unexpected_type("bool"))
    }
}

// FromMeta impls for std and syn types.

impl FromMeta for () {
    fn from_word() -> Result<Self> {
        Ok(())
    }
}

impl FromMeta for bool {
    fn from_word() -> Result<Self> {
        Ok(true)
    }

    fn from_bool(value: bool) -> Result<Self> {
        Ok(value)
    }

    fn from_string(value: &str) -> Result<Self> {
        value.parse().map_err(|_| Error::unknown_value(value))
    }
}

impl FromMeta for AtomicBool {
    fn from_meta(mi: &Meta) -> Result<Self> {
        FromMeta::from_meta(mi)
            .map(AtomicBool::new)
            .map_err(|e| e.with_span(mi))
    }
}

impl FromMeta for String {
    fn from_string(s: &str) -> Result<Self> {
        Ok(s.to_string())
    }
}

/// Generate an impl of `FromMeta` that will accept strings which parse to numbers or
/// integer literals.
macro_rules! from_meta_num {
    ($ty:ident) => {
        impl FromMeta for $ty {
            fn from_string(s: &str) -> Result<Self> {
                s.parse().map_err(|_| Error::unknown_value(s))
            }

            fn from_value(value: &Lit) -> Result<Self> {
                (match *value {
                    Lit::Str(ref s) => Self::from_string(&s.value()),
                    Lit::Int(ref s) => Ok(s.base10_parse::<$ty>().unwrap()),
                    _ => Err(Error::unexpected_lit_type(value)),
                })
                .map_err(|e| e.with_span(value))
            }
        }
    };
}

from_meta_num!(u8);
from_meta_num!(u16);
from_meta_num!(u32);
from_meta_num!(u64);
from_meta_num!(usize);
from_meta_num!(i8);
from_meta_num!(i16);
from_meta_num!(i32);
from_meta_num!(i64);
from_meta_num!(isize);

/// Generate an impl of `FromMeta` that will accept strings which parse to floats or
/// float literals.
macro_rules! from_meta_float {
    ($ty:ident) => {
        impl FromMeta for $ty {
            fn from_string(s: &str) -> Result<Self> {
                s.parse().map_err(|_| Error::unknown_value(s))
            }

            fn from_value(value: &Lit) -> Result<Self> {
                (match *value {
                    Lit::Str(ref s) => Self::from_string(&s.value()),
                    Lit::Float(ref s) => Ok(s.base10_parse::<$ty>().unwrap()),
                    _ => Err(Error::unexpected_lit_type(value)),
                })
                .map_err(|e| e.with_span(value))
            }
        }
    };
}

from_meta_float!(f32);
from_meta_float!(f64);

/// Parsing support for identifiers. This attempts to preserve span information
/// when available, but also supports parsing strings with the call site as the
/// emitted span.
impl FromMeta for syn::Ident {
    fn from_string(value: &str) -> Result<Self> {
        Ok(syn::Ident::new(value, ::proc_macro2::Span::call_site()))
    }

    fn from_value(value: &Lit) -> Result<Self> {
        if let Lit::Str(ref ident) = *value {
            ident
                .parse()
                .map_err(|_| Error::unknown_lit_str_value(ident))
        } else {
            Err(Error::unexpected_lit_type(value))
        }
    }
}

/// Parsing support for paths. This attempts to preserve span information when available,
/// but also supports parsing strings with the call site as the emitted span.
impl FromMeta for syn::Path {
    fn from_string(value: &str) -> Result<Self> {
        syn::parse_str(value).map_err(|_| Error::unknown_value(value))
    }

    fn from_value(value: &Lit) -> Result<Self> {
        if let Lit::Str(ref path_str) = *value {
            path_str
                .parse()
                .map_err(|_| Error::unknown_lit_str_value(path_str))
        } else {
            Err(Error::unexpected_lit_type(value))
        }
    }
}

impl FromMeta for syn::Lit {
    fn from_value(value: &Lit) -> Result<Self> {
        Ok(value.clone())
    }
}

macro_rules! from_meta_lit {
    ($impl_ty:path, $lit_variant:path) => {
        impl FromMeta for $impl_ty {
            fn from_value(value: &Lit) -> Result<Self> {
                if let $lit_variant(ref value) = *value {
                    Ok(value.clone())
                } else {
                    Err(Error::unexpected_lit_type(value))
                }
            }
        }
    };
}

from_meta_lit!(syn::LitInt, Lit::Int);
from_meta_lit!(syn::LitFloat, Lit::Float);
from_meta_lit!(syn::LitStr, Lit::Str);
from_meta_lit!(syn::LitByte, Lit::Byte);
from_meta_lit!(syn::LitByteStr, Lit::ByteStr);
from_meta_lit!(syn::LitChar, Lit::Char);
from_meta_lit!(syn::LitBool, Lit::Bool);
from_meta_lit!(proc_macro2::Literal, Lit::Verbatim);

impl FromMeta for syn::Meta {
    fn from_meta(value: &syn::Meta) -> Result<Self> {
        Ok(value.clone())
    }
}

impl FromMeta for syn::WhereClause {
    fn from_string(value: &str) -> Result<Self> {
        syn::parse_str(value).map_err(|_| Error::unknown_value(value))
    }
}

impl FromMeta for Vec<syn::WherePredicate> {
    fn from_string(value: &str) -> Result<Self> {
        syn::WhereClause::from_string(&format!("where {}", value))
            .map(|c| c.predicates.into_iter().collect())
    }
}

impl FromMeta for ident_case::RenameRule {
    fn from_string(value: &str) -> Result<Self> {
        value.parse().map_err(|_| Error::unknown_value(value))
    }
}

impl<T: FromMeta> FromMeta for Option<T> {
    fn from_meta(item: &Meta) -> Result<Self> {
        FromMeta::from_meta(item).map(Some)
    }
}

impl<T: FromMeta> FromMeta for Box<T> {
    fn from_meta(item: &Meta) -> Result<Self> {
        FromMeta::from_meta(item).map(Box::new)
    }
}

impl<T: FromMeta> FromMeta for Result<T> {
    fn from_meta(item: &Meta) -> Result<Self> {
        Ok(FromMeta::from_meta(item))
    }
}

/// Parses the meta-item, and in case of error preserves a copy of the input for
/// later analysis.
impl<T: FromMeta> FromMeta for ::std::result::Result<T, Meta> {
    fn from_meta(item: &Meta) -> Result<Self> {
        T::from_meta(item)
            .map(Ok)
            .or_else(|_| Ok(Err(item.clone())))
    }
}

impl<T: FromMeta> FromMeta for Rc<T> {
    fn from_meta(item: &Meta) -> Result<Self> {
        FromMeta::from_meta(item).map(Rc::new)
    }
}

impl<T: FromMeta> FromMeta for Arc<T> {
    fn from_meta(item: &Meta) -> Result<Self> {
        FromMeta::from_meta(item).map(Arc::new)
    }
}

impl<T: FromMeta> FromMeta for RefCell<T> {
    fn from_meta(item: &Meta) -> Result<Self> {
        FromMeta::from_meta(item).map(RefCell::new)
    }
}

impl<V: FromMeta, S: BuildHasher + Default> FromMeta for HashMap<String, V, S> {
    fn from_list(nested: &[syn::NestedMeta]) -> Result<Self> {
        let mut map = HashMap::with_capacity_and_hasher(nested.len(), Default::default());
        for item in nested {
            if let syn::NestedMeta::Meta(ref inner) = *item {
                let path = inner.path();
                let name = path.segments.iter().map(|s| s.ident.to_string()).collect::<Vec<String>>().join("::");
                match map.entry(name) {
                    Entry::Occupied(_) => {
                        return Err(
                            Error::duplicate_field_path(&path).with_span(inner)
                        );
                    }
                    Entry::Vacant(entry) => {
                        // In the error case, extend the error's path, but assume the inner `from_meta`
                        // set the span, and that subsequently we don't have to.
                        entry.insert(FromMeta::from_meta(inner).map_err(|e| e.at_path(&path))?);
                    }
                }
            }
        }

        Ok(map)
    }
}

/// Tests for `FromMeta` implementations. Wherever the word `ignore` appears in test input,
/// it should not be considered by the parsing.
#[cfg(test)]
mod tests {
    use proc_macro2::TokenStream;
    use syn;

    use {Error, FromMeta, Result};

    /// parse a string as a syn::Meta instance.
    fn pm(tokens: TokenStream) -> ::std::result::Result<syn::Meta, String> {
        let attribute: syn::Attribute = parse_quote!(#[#tokens]);
        attribute.parse_meta().map_err(|_| "Unable to parse".into())
    }

    fn fm<T: FromMeta>(tokens: TokenStream) -> T {
        FromMeta::from_meta(&pm(tokens).expect("Tests should pass well-formed input"))
            .expect("Tests should pass valid input")
    }

    #[test]
    fn unit_succeeds() {
        assert_eq!(fm::<()>(quote!(ignore)), ());
    }

    #[test]
    fn bool_succeeds() {
        // word format
        assert_eq!(fm::<bool>(quote!(ignore)), true);

        // bool literal
        assert_eq!(fm::<bool>(quote!(ignore = true)), true);
        assert_eq!(fm::<bool>(quote!(ignore = false)), false);

        // string literals
        assert_eq!(fm::<bool>(quote!(ignore = "true")), true);
        assert_eq!(fm::<bool>(quote!(ignore = "false")), false);
    }

    #[test]
    fn string_succeeds() {
        // cooked form
        assert_eq!(&fm::<String>(quote!(ignore = "world")), "world");

        // raw form
        assert_eq!(&fm::<String>(quote!(ignore = r#"world"#)), "world");
    }

    #[test]
    fn number_succeeds() {
        assert_eq!(fm::<u8>(quote!(ignore = "2")), 2u8);
        assert_eq!(fm::<i16>(quote!(ignore = "-25")), -25i16);
        assert_eq!(fm::<f64>(quote!(ignore = "1.4e10")), 1.4e10);
    }

    #[test]
    fn int_without_quotes() {
        assert_eq!(fm::<u8>(quote!(ignore = 2)), 2u8);
        assert_eq!(fm::<u16>(quote!(ignore = 255)), 255u16);
        assert_eq!(fm::<u32>(quote!(ignore = 5000)), 5000u32);

        // Check that we aren't tripped up by incorrect suffixes
        assert_eq!(fm::<u32>(quote!(ignore = 5000i32)), 5000u32);
    }

    #[test]
    fn float_without_quotes() {
        assert_eq!(fm::<f32>(quote!(ignore = 2.)), 2.0f32);
        assert_eq!(fm::<f32>(quote!(ignore = 2.0)), 2.0f32);
        assert_eq!(fm::<f64>(quote!(ignore = 1.4e10)), 1.4e10f64);
    }

    #[test]
    fn meta_succeeds() {
        use syn::Meta;

        assert_eq!(
            fm::<Meta>(quote!(hello(world, today))),
            pm(quote!(hello(world, today))).unwrap()
        );
    }

    #[test]
    fn hash_map_succeeds() {
        use std::collections::HashMap;

        let comparison = {
            let mut c = HashMap::new();
            c.insert("hello".to_string(), true);
            c.insert("world".to_string(), false);
            c.insert("there".to_string(), true);
            c
        };

        assert_eq!(
            fm::<HashMap<String, bool>>(quote!(ignore(hello, world = false, there = "true"))),
            comparison
        );
    }

    /// Check that a `HashMap` cannot have duplicate keys, and that the generated error
    /// is assigned a span to correctly target the diagnostic message.
    #[test]
    fn hash_map_duplicate() {
        use std::collections::HashMap;

        let err: Result<HashMap<String, bool>> =
            FromMeta::from_meta(&pm(quote!(ignore(hello, hello = false))).unwrap());

        let err = err.expect_err("Duplicate keys in HashMap should error");

        assert!(err.has_span());
        assert_eq!(err.to_string(), Error::duplicate_field("hello").to_string());
    }

    /// Tests that fallible parsing will always produce an outer `Ok` (from `fm`),
    /// and will accurately preserve the inner contents.
    #[test]
    fn darling_result_succeeds() {
        fm::<Result<()>>(quote!(ignore)).unwrap();
        fm::<Result<()>>(quote!(ignore(world))).unwrap_err();
    }
}
