// Copyright 2017 Serde Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use syn;
use attr;
use check;
use Ctxt;

pub struct Container<'a> {
    pub ident: syn::Ident,
    pub attrs: attr::Container,
    pub body: Body<'a>,
    pub generics: &'a syn::Generics,
}

pub enum Body<'a> {
    Enum(Repr, Vec<Variant<'a>>),
    Struct(Style, Vec<Field<'a>>),
}

pub struct Variant<'a> {
    pub ident: syn::Ident,
    pub attrs: attr::Variant,
    pub style: Style,
    pub fields: Vec<Field<'a>>,
}

pub struct Field<'a> {
    pub ident: Option<syn::Ident>,
    pub attrs: attr::Field,
    pub ty: &'a syn::Ty,
}

pub struct Repr {
    pub int_repr: Option<&'static str>,
    pub c_repr: bool,
    pub other_repr: bool,
}

#[derive(Copy, Clone)]
pub enum Style {
    Struct,
    Tuple,
    Newtype,
    Unit,
}

impl<'a> Container<'a> {
    pub fn from_ast(cx: &Ctxt, item: &'a syn::DeriveInput) -> Container<'a> {
        let attrs = attr::Container::from_ast(cx, item);

        let mut body = match item.body {
            syn::Body::Enum(ref variants) => {
                let (repr, variants) = enum_from_ast(cx, item, variants, &attrs.default());
                Body::Enum(repr, variants)
            }
            syn::Body::Struct(ref variant_data) => {
                let (style, fields) = struct_from_ast(cx, variant_data, None, &attrs.default());
                Body::Struct(style, fields)
            }
        };

        match body {
            Body::Enum(_, ref mut variants) => for ref mut variant in variants {
                variant.attrs.rename_by_rule(attrs.rename_all());
                for ref mut field in &mut variant.fields {
                    field.attrs.rename_by_rule(variant.attrs.rename_all());
                }
            },
            Body::Struct(_, ref mut fields) => for field in fields {
                field.attrs.rename_by_rule(attrs.rename_all());
            },
        }

        let item = Container {
            ident: item.ident.clone(),
            attrs: attrs,
            body: body,
            generics: &item.generics,
        };
        check::check(cx, &item);
        item
    }
}

impl<'a> Body<'a> {
    pub fn all_fields(&'a self) -> Box<Iterator<Item = &'a Field<'a>> + 'a> {
        match *self {
            Body::Enum(_, ref variants) => {
                Box::new(variants.iter().flat_map(|variant| variant.fields.iter()))
            }
            Body::Struct(_, ref fields) => Box::new(fields.iter()),
        }
    }

    pub fn has_getter(&self) -> bool {
        self.all_fields().any(|f| f.attrs.getter().is_some())
    }
}

impl Repr {
    /// Gives the int type to use for the `repr(int)` enum layout
    pub fn get_stable_rust_enum_layout(&self) -> Option<&'static str> {
        if self.c_repr || self.other_repr {
            None
        } else {
            self.int_repr
        }
    }

    /// Gives the int type to use for the `repr(C, int)` enum layout
    pub fn get_stable_c_enum_layout(&self) -> Option<&'static str> {
        if !self.c_repr && self.other_repr {
            None
        } else {
            self.int_repr
        }
    }
}

fn enum_from_ast<'a>(
    cx: &Ctxt, 
    item: &'a syn::DeriveInput, 
    variants: &'a [syn::Variant], 
    container_default: &attr::Default
) -> (Repr, Vec<Variant<'a>>) {
    let variants = variants
        .iter()
        .map(
            |variant| {
                let attrs = attr::Variant::from_ast(cx, variant);
                let (style, fields) = 
                    struct_from_ast(cx, &variant.data, Some(&attrs), container_default);
                Variant {
                    ident: variant.ident.clone(),
                    attrs: attrs,
                    style: style,
                    fields: fields,
                }
            },
        )
        .collect();

    // Compute repr info for enum optimizations
    static INT_TYPES: [&'static str; 12] = [
        "u8", "u16", "u32", "u64", "u128", "usize",
        "i8", "i16", "i32", "i64", "i128", "isize",
    ];

    let mut int_repr = None;
    let mut c_repr = false;
    let mut other_repr = false;

    for attr in &item.attrs {
        if let syn::MetaItem::List(ref ident, ref vals) = attr.value {
            if *ident == "repr" {
                // has_repr = true;
                for repr in vals {
                    if let syn::NestedMetaItem::MetaItem(syn::MetaItem::Word(ref repr)) = *repr {
                        if repr == "C" {
                            c_repr = true;
                        } else if let Some(int_type) = INT_TYPES.iter().cloned().find(|int_type| repr == int_type) {
                            if int_repr.is_some() {
                                // This shouldn't happen, but we shouldn't crash if we see it.
                                // So just treat the enum as having a mysterious other repr,
                                // which makes us discard any attempt to optimize based on layout.
                                other_repr = true;
                            }
                            int_repr = Some(int_type);
                        } else {
                            other_repr = true;
                        }
                    } else {
                        panic!("impossible repr? {:?}", repr);
                    }
                }
            }
        }
    }

    let repr = Repr { int_repr, c_repr, other_repr };

    (repr, variants)
}

fn struct_from_ast<'a>(
    cx: &Ctxt,
    data: &'a syn::VariantData,
    attrs: Option<&attr::Variant>,
    container_default: &attr::Default,
) -> (Style, Vec<Field<'a>>) {
    match *data {
        syn::VariantData::Struct(ref fields) => (
            Style::Struct,
            fields_from_ast(cx, fields, attrs, container_default),
        ),
        syn::VariantData::Tuple(ref fields) if fields.len() == 1 => (
            Style::Newtype,
            fields_from_ast(cx, fields, attrs, container_default),
        ),
        syn::VariantData::Tuple(ref fields) => (
            Style::Tuple,
            fields_from_ast(cx, fields, attrs, container_default),
        ),
        syn::VariantData::Unit => (Style::Unit, Vec::new()),
    }
}

fn fields_from_ast<'a>(
    cx: &Ctxt,
    fields: &'a [syn::Field],
    attrs: Option<&attr::Variant>,
    container_default: &attr::Default,
) -> Vec<Field<'a>> {
    fields
        .iter()
        .enumerate()
        .map(|(i, field)| Field {
            ident: field.ident.clone(),
            attrs: attr::Field::from_ast(cx, i, field, attrs, container_default),
            ty: &field.ty,
        })
        .collect()
}
