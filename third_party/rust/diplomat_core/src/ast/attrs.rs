//! This module contains utilities for dealing with Rust attributes

use serde::ser::{SerializeStruct, Serializer};
use serde::Serialize;
use syn::parse::{Error as ParseError, Parse, ParseStream};
use syn::{Attribute, Ident, LitStr, Meta, Token};

/// The list of attributes on a type
#[derive(Clone, PartialEq, Eq, Hash, Debug, Default)]
#[non_exhaustive]
pub struct Attrs {
    pub cfg: Vec<Attribute>,
    pub attrs: Vec<DiplomatBackendAttr>,
}

impl Attrs {
    fn add_attr(&mut self, attr: Attr) {
        match attr {
            Attr::Cfg(attr) => self.cfg.push(attr),
            Attr::DiplomatBackendAttr(attr) => self.attrs.push(attr),
        }
    }

    /// Merge attributes that should be inherited from the parent
    pub(crate) fn merge_parent_attrs(&mut self, other: &Attrs) {
        self.cfg.extend(other.cfg.iter().cloned())
    }
    pub(crate) fn add_attrs(&mut self, attrs: &[Attribute]) {
        for attr in syn_attr_to_ast_attr(attrs) {
            self.add_attr(attr)
        }
    }
    pub(crate) fn from_attrs(attrs: &[Attribute]) -> Self {
        let mut this = Self::default();
        this.add_attrs(attrs);
        this
    }
}

impl From<&[Attribute]> for Attrs {
    fn from(other: &[Attribute]) -> Self {
        Self::from_attrs(other)
    }
}

enum Attr {
    Cfg(Attribute),
    DiplomatBackendAttr(DiplomatBackendAttr),
    // More goes here
}

fn syn_attr_to_ast_attr(attrs: &[Attribute]) -> impl Iterator<Item = Attr> + '_ {
    let cfg_path: syn::Path = syn::parse_str("cfg").unwrap();
    let dattr_path: syn::Path = syn::parse_str("diplomat::attr").unwrap();
    attrs.iter().filter_map(move |a| {
        if a.path() == &cfg_path {
            Some(Attr::Cfg(a.clone()))
        } else if a.path() == &dattr_path {
            Some(Attr::DiplomatBackendAttr(
                a.parse_args()
                    .expect("Failed to parse malformed diplomat::attr"),
            ))
        } else {
            None
        }
    })
}

impl Serialize for Attrs {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        // 1 is the number of fields in the struct.
        let mut state = serializer.serialize_struct("Attrs", 1)?;
        let cfg: Vec<_> = self
            .cfg
            .iter()
            .map(|a| quote::quote!(#a).to_string())
            .collect();
        state.serialize_field("cfg", &cfg)?;
        state.end()
    }
}

/// A `#[diplomat::attr(...)]` attribute
///
/// Its contents must start with single element that is a CFG-expression
/// (so it may contain `foo = bar`, `foo = "bar"`, `ident`, `*` atoms,
/// and `all()`, `not()`, and `any()` combiners), and then be followed by one
/// or more backend-specific attributes, which can be any valid meta-item
#[derive(Clone, PartialEq, Eq, Hash, Debug, Serialize)]
#[non_exhaustive]
pub struct DiplomatBackendAttr {
    pub cfg: DiplomatBackendAttrCfg,
    #[serde(serialize_with = "serialize_meta")]
    pub meta: Meta,
}

fn serialize_meta<S>(m: &Meta, s: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    quote::quote!(#m).to_string().serialize(s)
}

#[derive(Clone, PartialEq, Eq, Hash, Debug, Serialize)]
#[non_exhaustive]
pub enum DiplomatBackendAttrCfg {
    Not(Box<DiplomatBackendAttrCfg>),
    Any(Vec<DiplomatBackendAttrCfg>),
    All(Vec<DiplomatBackendAttrCfg>),
    Star,
    BackendName(String),
    NameValue(String, String),
}

impl Parse for DiplomatBackendAttrCfg {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        let lookahead = input.lookahead1();
        if lookahead.peek(Ident) {
            let name: Ident = input.parse()?;
            if name == "not" {
                let content;
                let _paren = syn::parenthesized!(content in input);
                Ok(DiplomatBackendAttrCfg::Not(Box::new(content.parse()?)))
            } else if name == "any" || name == "all" {
                let content;
                let _paren = syn::parenthesized!(content in input);
                let list = content.parse_terminated(Self::parse, Token![,])?;
                let vec = list.into_iter().collect();
                if name == "any" {
                    Ok(DiplomatBackendAttrCfg::Any(vec))
                } else {
                    Ok(DiplomatBackendAttrCfg::All(vec))
                }
            } else if input.peek(Token![=]) {
                let _t: Token![=] = input.parse()?;
                if input.peek(Ident) {
                    let value: Ident = input.parse()?;
                    Ok(DiplomatBackendAttrCfg::NameValue(
                        name.to_string(),
                        value.to_string(),
                    ))
                } else {
                    let value: LitStr = input.parse()?;
                    Ok(DiplomatBackendAttrCfg::NameValue(
                        name.to_string(),
                        value.value(),
                    ))
                }
            } else {
                Ok(DiplomatBackendAttrCfg::BackendName(name.to_string()))
            }
        } else if lookahead.peek(Token![*]) {
            let _t: Token![*] = input.parse()?;
            Ok(DiplomatBackendAttrCfg::Star)
        } else {
            Err(ParseError::new(
                input.span(),
                "CFG portion of #[diplomat::attr] fails to parse",
            ))
        }
    }
}

/// Meant to be used with Attribute::parse_args()
impl Parse for DiplomatBackendAttr {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        let cfg = input.parse()?;
        let _comma: Token![,] = input.parse()?;
        let meta = input.parse()?;
        Ok(Self { cfg, meta })
    }
}

#[cfg(test)]
mod tests {
    use insta;

    use syn;

    use super::{DiplomatBackendAttr, DiplomatBackendAttrCfg};

    #[test]
    fn test_cfgs() {
        let attr_cfg: DiplomatBackendAttrCfg = syn::parse_quote!(*);
        insta::assert_yaml_snapshot!(attr_cfg);
        let attr_cfg: DiplomatBackendAttrCfg = syn::parse_quote!(cpp);
        insta::assert_yaml_snapshot!(attr_cfg);
        let attr_cfg: DiplomatBackendAttrCfg = syn::parse_quote!(has = overloading);
        insta::assert_yaml_snapshot!(attr_cfg);
        let attr_cfg: DiplomatBackendAttrCfg = syn::parse_quote!(has = "overloading");
        insta::assert_yaml_snapshot!(attr_cfg);
        let attr_cfg: DiplomatBackendAttrCfg =
            syn::parse_quote!(any(all(*, cpp, has="overloading"), not(js)));
        insta::assert_yaml_snapshot!(attr_cfg);
    }

    #[test]
    fn test_attr() {
        let attr: syn::Attribute =
            syn::parse_quote!(#[diplomat::attr(any(cpp, has = "overloading"), namespacing)]);
        let attr: DiplomatBackendAttr = attr.parse_args().unwrap();
        insta::assert_yaml_snapshot!(attr);
    }
}
