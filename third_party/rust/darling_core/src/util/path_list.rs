use std::ops::Deref;
use std::string::ToString;

use syn::{Meta, NestedMeta, Path};

use {Error, FromMeta, Result};

/// A list of `syn::Path` instances. This type is used to extract a list of paths from an
/// attribute.
///
/// # Usage
/// An `PathList` field on a struct implementing `FromMeta` will turn `#[builder(derive(serde::Debug, Clone))]` into:
///
/// ```rust,ignore
/// StructOptions {
///     derive: PathList(vec![syn::Path::new("serde::Debug"), syn::Path::new("Clone")])
/// }
/// ```
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct PathList(Vec<Path>);

impl PathList {
    /// Create a new list.
    pub fn new<T: Into<Path>>(vals: Vec<T>) -> Self {
        PathList(vals.into_iter().map(T::into).collect())
    }

    /// Create a new `Vec` containing the string representation of each path.
    pub fn to_strings(&self) -> Vec<String> {
        self.0.iter().map(|p| p.segments.iter().map(|s| s.ident.to_string()).collect::<Vec<String>>().join("::")).collect()
    }
}

impl Deref for PathList {
    type Target = Vec<Path>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl From<Vec<Path>> for PathList {
    fn from(v: Vec<Path>) -> Self {
        PathList(v)
    }
}

impl FromMeta for PathList {
    fn from_list(v: &[NestedMeta]) -> Result<Self> {
        let mut paths = Vec::with_capacity(v.len());
        for nmi in v {
            if let NestedMeta::Meta(Meta::Path(ref path)) = *nmi {
                paths.push(path.clone());
            } else {
                return Err(Error::unexpected_type("non-word").with_span(nmi));
            }
        }

        Ok(PathList(paths))
    }
}

#[cfg(test)]
mod tests {
    use super::PathList;
    use proc_macro2::TokenStream;
    use syn::{Attribute, Meta};
    use FromMeta;

    /// parse a string as a syn::Meta instance.
    fn pm(tokens: TokenStream) -> ::std::result::Result<Meta, String> {
        let attribute: Attribute = parse_quote!(#[#tokens]);
        attribute.parse_meta().map_err(|_| "Unable to parse".into())
    }

    fn fm<T: FromMeta>(tokens: TokenStream) -> T {
        FromMeta::from_meta(&pm(tokens).expect("Tests should pass well-formed input"))
            .expect("Tests should pass valid input")
    }

    #[test]
    fn succeeds() {
        let paths = fm::<PathList>(quote!(ignore(Debug, Clone, Eq)));
        assert_eq!(
            paths.to_strings(),
            vec![
                String::from("Debug"),
                String::from("Clone"),
                String::from("Eq")
            ]
        );
    }

    /// Check that the parser rejects non-word members of the list, and that the error
    /// has an associated span.
    #[test]
    fn fails_non_word() {
        let input = PathList::from_meta(&pm(quote!(ignore(Debug, Clone = false))).unwrap());
        let err = input.unwrap_err();
        assert!(err.has_span());
    }
}
