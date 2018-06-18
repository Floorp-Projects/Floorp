use std::cell::RefCell;
use std::collections::hash_map::{Entry, HashMap};
use std::rc::Rc;
use std::sync::Arc;
use std::sync::atomic::AtomicBool;

use ident_case;
use syn::{self, Lit, Meta, NestedMeta};

use {Error, Result};

/// Create an instance from an item in an attribute declaration.
///
/// # Implementing `FromMetaItem`
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
pub trait FromMetaItem: Sized {
    fn from_nested_meta_item(item: &NestedMeta) -> Result<Self> {
        match *item {
            NestedMeta::Literal(ref lit) => Self::from_value(lit),
            NestedMeta::Meta(ref mi) => Self::from_meta_item(mi),
        }
    }

    /// Create an instance from a `syn::Meta` by dispatching to the format-appropriate
    /// trait function. This generally should not be overridden by implementers.
    fn from_meta_item(item: &Meta) -> Result<Self> {
        match *item {
            Meta::Word(_) => Self::from_word(),
            Meta::List(ref value) => Self::from_list(&value.nested.clone().into_iter().collect::<Vec<syn::NestedMeta>>()[..]),
            Meta::NameValue(ref value) => Self::from_value(&value.lit),
        }
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
    fn from_value(value: &Lit) -> Result<Self> {
        match *value {
            Lit::Bool(ref b) => Self::from_bool(b.value),
            Lit::Str(ref s) => Self::from_string(&s.value()),
            ref _other => Err(Error::unexpected_type("other"))
        }
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

// FromMetaItem impls for std and syn types.

impl FromMetaItem for () {
    fn from_word() -> Result<Self> {
        Ok(())
    }
}

impl FromMetaItem for bool {
    fn from_word() -> Result<Self> {
        Ok(true)
    }

    fn from_bool(value: bool) -> Result<Self> {
        Ok(value)
    }

    fn from_string(value: &str) -> Result<Self> {
        value.parse().or_else(|_| Err(Error::unknown_value(value)))
    }
}

impl FromMetaItem for AtomicBool {
    fn from_meta_item(mi: &Meta) -> Result<Self> {
        Ok(AtomicBool::new(FromMetaItem::from_meta_item(mi)?))
    }
}

impl FromMetaItem for String {
    fn from_string(s: &str) -> Result<Self> {
        Ok(s.to_string())
    }
}

impl FromMetaItem for u8 {
    fn from_string(s: &str) -> Result<Self> {
        s.parse().or_else(|_| Err(Error::unknown_value(s)))
    }
}

impl FromMetaItem for u16 {
    fn from_string(s: &str) -> Result<Self> {
        s.parse().or_else(|_| Err(Error::unknown_value(s)))
    }
}

impl FromMetaItem for u32 {
    fn from_string(s: &str) -> Result<Self> {
        s.parse().or_else(|_| Err(Error::unknown_value(s)))
    }
}

impl FromMetaItem for u64 {
    fn from_string(s: &str) -> Result<Self> {
        s.parse().or_else(|_| Err(Error::unknown_value(s)))
    }
}

impl FromMetaItem for usize {
    fn from_string(s: &str) -> Result<Self> {
        s.parse().or_else(|_| Err(Error::unknown_value(s)))
    }
}

impl FromMetaItem for i8 {
    fn from_string(s: &str) -> Result<Self> {
        s.parse().or_else(|_| Err(Error::unknown_value(s)))
    }
}

impl FromMetaItem for i16 {
    fn from_string(s: &str) -> Result<Self> {
        s.parse().or_else(|_| Err(Error::unknown_value(s)))
    }
}

impl FromMetaItem for i32 {
    fn from_string(s: &str) -> Result<Self> {
        s.parse().or_else(|_| Err(Error::unknown_value(s)))
    }
}

impl FromMetaItem for i64 {
    fn from_string(s: &str) -> Result<Self> {
        s.parse().or_else(|_| Err(Error::unknown_value(s)))
    }
}

impl FromMetaItem for isize {
    fn from_string(s: &str) -> Result<Self> {
        s.parse().or_else(|_| Err(Error::unknown_value(s)))
    }
}

impl FromMetaItem for syn::Ident {
    fn from_string(value: &str) -> Result<Self> {
        Ok(syn::Ident::new(value, ::proc_macro2::Span::call_site()))
    }
}

impl FromMetaItem for syn::Path {
    fn from_string(value: &str) -> Result<Self> {
        Ok(syn::parse_str::<syn::Path>(value).unwrap())
    }
}
/*
impl FromMetaItem for syn::TypeParamBound {
    fn from_string(value: &str) -> Result<Self> {
        Ok(syn::TypeParamBound::from(value))
    }
}
*/

impl FromMetaItem for syn::Meta {
    fn from_meta_item(value: &syn::Meta) -> Result<Self> {
        Ok(value.clone())
    }
}

impl FromMetaItem for syn::WhereClause {
    fn from_string(value: &str) -> Result<Self> {
        let ret: syn::WhereClause = syn::parse_str(value).unwrap();
        Ok(ret)
    }
}

impl FromMetaItem for Vec<syn::WherePredicate> {
    fn from_string(value: &str) -> Result<Self> {
        syn::WhereClause::from_string(&format!("where {}", value)).map(|c| c.predicates.into_iter().collect())
    }
}

impl FromMetaItem for ident_case::RenameRule {
    fn from_string(value: &str) -> Result<Self> {
        value.parse().or_else(|_| Err(Error::unknown_value(value)))
    }
}

impl<T: FromMetaItem> FromMetaItem for Option<T> {
    fn from_meta_item(item: &Meta) -> Result<Self> {
        Ok(Some(FromMetaItem::from_meta_item(item)?))
    }
}

impl<T: FromMetaItem> FromMetaItem for Box<T> {
    fn from_meta_item(item: &Meta) -> Result<Self> {
        Ok(Box::new(FromMetaItem::from_meta_item(item)?))
    }
}

impl<T: FromMetaItem> FromMetaItem for Result<T> {
    fn from_meta_item(item: &Meta) -> Result<Self> {
        Ok(FromMetaItem::from_meta_item(item))
    }
}

/// Parses the meta-item, and in case of error preserves a copy of the input for
/// later analysis.
impl<T: FromMetaItem> FromMetaItem for ::std::result::Result<T, Meta> {
    fn from_meta_item(item: &Meta) -> Result<Self> {
        T::from_meta_item(item).map(Ok).or_else(|_| Ok(Err(item.clone())))
    }
}

impl<T: FromMetaItem> FromMetaItem for Rc<T> {
    fn from_meta_item(item: &Meta) -> Result<Self> {
        Ok(Rc::new(FromMetaItem::from_meta_item(item)?))
    }
}

impl<T: FromMetaItem> FromMetaItem for Arc<T> {
    fn from_meta_item(item: &Meta) -> Result<Self> {
        Ok(Arc::new(FromMetaItem::from_meta_item(item)?))
    }
}

impl<T: FromMetaItem> FromMetaItem for RefCell<T> {
    fn from_meta_item(item: &Meta) -> Result<Self> {
        Ok(RefCell::new(FromMetaItem::from_meta_item(item)?))
    }
}

impl<V: FromMetaItem> FromMetaItem for HashMap<String, V> {
    fn from_list(nested: &[syn::NestedMeta]) -> Result<Self> {
        let mut map = HashMap::with_capacity(nested.len());
        for item in nested {
            if let syn::NestedMeta::Meta(ref inner) = *item {
                match map.entry(inner.name().to_string()) {
                    Entry::Occupied(_) => return Err(Error::duplicate_field(inner.name().as_ref())),
                    Entry::Vacant(entry) => {
                        entry.insert(
                            FromMetaItem::from_meta_item(inner).map_err(|e| e.at(inner.name()))?
                        );
                    }
                }
            }
        }

        Ok(map)
    }
}

/// Tests for `FromMetaItem` implementations. Wherever the word `ignore` appears in test input,
/// it should not be considered by the parsing.
#[cfg(test)]
mod tests {
    use syn;
    use quote::Tokens;

    use {FromMetaItem, Result};

    /// parse a string as a syn::Meta instance.
    fn pmi(tokens: Tokens) -> ::std::result::Result<syn::Meta, String> {
        let attribute: syn::Attribute = parse_quote!(#[#tokens]);
        attribute.interpret_meta().ok_or("Unable to parse".into())
    }

    fn fmi<T: FromMetaItem>(tokens: Tokens) -> T {
        FromMetaItem::from_meta_item(&pmi(tokens).expect("Tests should pass well-formed input"))
            .expect("Tests should pass valid input")
    }

    #[test]
    fn unit_succeeds() {
        assert_eq!(fmi::<()>(quote!(ignore)), ());
    }

    #[test]
    fn bool_succeeds() {
        // word format
        assert_eq!(fmi::<bool>(quote!(ignore)), true);

        // bool literal
        assert_eq!(fmi::<bool>(quote!(ignore = true)), true);
        assert_eq!(fmi::<bool>(quote!(ignore = false)), false);

        // string literals
        assert_eq!(fmi::<bool>(quote!(ignore = "true")), true);
        assert_eq!(fmi::<bool>(quote!(ignore = "false")), false);
    }

    #[test]
    fn string_succeeds() {
        // cooked form
        assert_eq!(&fmi::<String>(quote!(ignore = "world")), "world");

        // raw form
        assert_eq!(&fmi::<String>(quote!(ignore = r#"world"#)), "world");
    }

    #[test]
    fn number_succeeds() {
        assert_eq!(fmi::<u8>(quote!(ignore = "2")), 2u8);
        assert_eq!(fmi::<i16>(quote!(ignore="-25")), -25i16);
    }

    #[test]
    fn meta_item_succeeds() {
        use syn::Meta;

        assert_eq!(fmi::<Meta>(quote!(hello(world,today))), pmi(quote!(hello(world,today))).unwrap());
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

        assert_eq!(fmi::<HashMap<String, bool>>(quote!(ignore(hello, world = false, there = "true"))), comparison);
    }

    /// Tests that fallible parsing will always produce an outer `Ok` (from `fmi`),
    /// and will accurately preserve the inner contents.
    #[test]
    fn darling_result_succeeds() {
        fmi::<Result<()>>(quote!(ignore)).unwrap();
        fmi::<Result<()>>(quote!(ignore(world))).unwrap_err();
    }
}
