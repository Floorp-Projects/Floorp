use super::*;

use std::iter;

/// Doc-comments are promoted to attributes that have `is_sugared_doc` = true
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct Attribute {
    pub style: AttrStyle,
    pub value: MetaItem,
    pub is_sugared_doc: bool,
}

impl Attribute {
    pub fn name(&self) -> &str {
        self.value.name()
    }
}

/// Distinguishes between Attributes that decorate items and Attributes that
/// are contained as statements within items. These two cases need to be
/// distinguished for pretty-printing.
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum AttrStyle {
    Outer,
    Inner,
}

/// A compile-time attribute item.
///
/// E.g. `#[test]`, `#[derive(..)]` or `#[feature = "foo"]`
#[derive(Debug, Clone, Eq, PartialEq)]
pub enum MetaItem {
    /// Word meta item.
    ///
    /// E.g. `test` as in `#[test]`
    Word(Ident),
    /// List meta item.
    ///
    /// E.g. `derive(..)` as in `#[derive(..)]`
    List(Ident, Vec<MetaItem>),
    /// Name value meta item.
    ///
    /// E.g. `feature = "foo"` as in `#[feature = "foo"]`
    NameValue(Ident, Lit),
}

impl MetaItem {
    pub fn name(&self) -> &str {
        match *self {
            MetaItem::Word(ref name) |
            MetaItem::List(ref name, _) |
            MetaItem::NameValue(ref name, _) => name.as_ref(),
        }
    }
}

pub trait FilterAttrs<'a> {
    type Ret: Iterator<Item = &'a Attribute>;

    fn outer(self) -> Self::Ret;
    fn inner(self) -> Self::Ret;
}

impl<'a, T> FilterAttrs<'a> for T
    where T: IntoIterator<Item = &'a Attribute>
{
    type Ret = iter::Filter<T::IntoIter, fn(&&Attribute) -> bool>;

    fn outer(self) -> Self::Ret {
        fn is_outer(attr: &&Attribute) -> bool {
            attr.style == AttrStyle::Outer
        }
        self.into_iter().filter(is_outer)
    }

    fn inner(self) -> Self::Ret {
        fn is_inner(attr: &&Attribute) -> bool {
            attr.style == AttrStyle::Inner
        }
        self.into_iter().filter(is_inner)
    }
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use ident::parsing::ident;
    use lit::{Lit, StrStyle};
    use lit::parsing::lit;
    use space::{block_comment, whitespace};

    #[cfg(feature = "full")]
    named!(pub inner_attr -> Attribute, alt!(
        do_parse!(
            punct!("#") >>
            punct!("!") >>
            punct!("[") >>
            meta_item: meta_item >>
            punct!("]") >>
            (Attribute {
                style: AttrStyle::Inner,
                value: meta_item,
                is_sugared_doc: false,
            })
        )
        |
        do_parse!(
            punct!("//!") >>
            content: take_until!("\n") >>
            (Attribute {
                style: AttrStyle::Inner,
                value: MetaItem::NameValue(
                    "doc".into(),
                    Lit::Str(
                        format!("//!{}", content),
                        StrStyle::Cooked,
                    ),
                ),
                is_sugared_doc: true,
            })
        )
        |
        do_parse!(
            option!(whitespace) >>
            peek!(tag!("/*!")) >>
            com: block_comment >>
            (Attribute {
                style: AttrStyle::Inner,
                value: MetaItem::NameValue(
                    "doc".into(),
                    Lit::Str(com.to_owned(), StrStyle::Cooked),
                ),
                is_sugared_doc: true,
            })
        )
    ));

    named!(pub outer_attr -> Attribute, alt!(
        do_parse!(
            punct!("#") >>
            punct!("[") >>
            meta_item: meta_item >>
            punct!("]") >>
            (Attribute {
                style: AttrStyle::Outer,
                value: meta_item,
                is_sugared_doc: false,
            })
        )
        |
        do_parse!(
            punct!("///") >>
            not!(peek!(tag!("/"))) >>
            content: take_until!("\n") >>
            (Attribute {
                style: AttrStyle::Outer,
                value: MetaItem::NameValue(
                    "doc".into(),
                    Lit::Str(
                        format!("///{}", content),
                        StrStyle::Cooked,
                    ),
                ),
                is_sugared_doc: true,
            })
        )
        |
        do_parse!(
            option!(whitespace) >>
            peek!(tuple!(tag!("/**"), not!(tag!("*")))) >>
            com: block_comment >>
            (Attribute {
                style: AttrStyle::Outer,
                value: MetaItem::NameValue(
                    "doc".into(),
                    Lit::Str(com.to_owned(), StrStyle::Cooked),
                ),
                is_sugared_doc: true,
            })
        )
    ));

    named!(meta_item -> MetaItem, alt!(
        do_parse!(
            id: ident >>
            punct!("(") >>
            inner: terminated_list!(punct!(","), meta_item) >>
            punct!(")") >>
            (MetaItem::List(id, inner))
        )
        |
        do_parse!(
            name: ident >>
            punct!("=") >>
            value: lit >>
            (MetaItem::NameValue(name, value))
        )
        |
        map!(ident, MetaItem::Word)
    ));
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use lit::{Lit, StrStyle};
    use quote::{Tokens, ToTokens};

    impl ToTokens for Attribute {
        fn to_tokens(&self, tokens: &mut Tokens) {
            if let Attribute { style,
                               value: MetaItem::NameValue(ref name,
                                                   Lit::Str(ref value, StrStyle::Cooked)),
                               is_sugared_doc: true } = *self {
                if name == "doc" {
                    match style {
                        AttrStyle::Inner if value.starts_with("//!") => {
                            tokens.append(&format!("{}\n", value));
                            return;
                        }
                        AttrStyle::Inner if value.starts_with("/*!") => {
                            tokens.append(value);
                            return;
                        }
                        AttrStyle::Outer if value.starts_with("///") => {
                            tokens.append(&format!("{}\n", value));
                            return;
                        }
                        AttrStyle::Outer if value.starts_with("/**") => {
                            tokens.append(value);
                            return;
                        }
                        _ => {}
                    }
                }
            }

            tokens.append("#");
            if let AttrStyle::Inner = self.style {
                tokens.append("!");
            }
            tokens.append("[");
            self.value.to_tokens(tokens);
            tokens.append("]");
        }
    }

    impl ToTokens for MetaItem {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                MetaItem::Word(ref ident) => {
                    ident.to_tokens(tokens);
                }
                MetaItem::List(ref ident, ref inner) => {
                    ident.to_tokens(tokens);
                    tokens.append("(");
                    tokens.append_separated(inner, ",");
                    tokens.append(")");
                }
                MetaItem::NameValue(ref name, ref value) => {
                    name.to_tokens(tokens);
                    tokens.append("=");
                    value.to_tokens(tokens);
                }
            }
        }
    }
}
