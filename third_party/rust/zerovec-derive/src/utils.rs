// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use quote::{quote, ToTokens};

use proc_macro2::Span;
use proc_macro2::TokenStream as TokenStream2;
use syn::parse::{Parse, ParseStream};
use syn::punctuated::Punctuated;
use syn::spanned::Spanned;
use syn::{Attribute, Error, Field, Fields, Ident, Index, Result, Token};

// Check that there are repr attributes satisfying the given predicate
pub fn has_valid_repr(attrs: &[Attribute], predicate: impl Fn(&Ident) -> bool + Copy) -> bool {
    attrs.iter().filter(|a| a.path().is_ident("repr")).any(|a| {
        a.parse_args::<IdentListAttribute>()
            .ok()
            .and_then(|s| s.idents.iter().find(|s| predicate(s)).map(|_| ()))
            .is_some()
    })
}

// An attribute that is a list of idents
struct IdentListAttribute {
    idents: Punctuated<Ident, Token![,]>,
}

impl Parse for IdentListAttribute {
    fn parse(input: ParseStream) -> Result<Self> {
        Ok(IdentListAttribute {
            idents: input.parse_terminated(Ident::parse, Token![,])?,
        })
    }
}

/// Given a set of entries for struct field definitions to go inside a `struct {}` definition,
/// wrap in a () or {} based on the type of field
pub fn wrap_field_inits(streams: &[TokenStream2], fields: &Fields) -> TokenStream2 {
    match *fields {
        Fields::Named(_) => quote!( { #(#streams),* } ),
        Fields::Unnamed(_) => quote!( ( #(#streams),* ) ),
        Fields::Unit => {
            unreachable!("#[make_(var)ule] should have already checked that there are fields")
        }
    }
}

/// Return a semicolon token if necessary after the struct definition
pub fn semi_for(f: &Fields) -> TokenStream2 {
    if let Fields::Unnamed(..) = *f {
        quote!(;)
    } else {
        quote!()
    }
}

/// Returns the repr attribute to be applied to the resultant ULE or VarULE type
pub fn repr_for(f: &Fields) -> TokenStream2 {
    if f.len() == 1 {
        quote!(transparent)
    } else {
        quote!(packed)
    }
}

fn suffixed_ident(name: &str, suffix: usize, s: Span) -> Ident {
    Ident::new(&format!("{name}_{suffix}"), s)
}

/// Given an iterator over ULE or AsULE struct fields, returns code that calculates field sizes and generates a line
/// of code per field based on the per_field_code function (whose parameters are the field, the identifier of the const
/// for the previous offset, the identifier for the const for the next offset, and the field index)
pub(crate) fn generate_per_field_offsets<'a>(
    fields: &[FieldInfo<'a>],
    // Whether the fields are ULE types or AsULE (and need conversion)
    fields_are_asule: bool,
    // (field, prev_offset_ident, size_ident)
    mut per_field_code: impl FnMut(&FieldInfo<'a>, &Ident, &Ident) -> TokenStream2, /* (code, remaining_offset) */
) -> (TokenStream2, syn::Ident) {
    let mut prev_offset_ident = Ident::new("ZERO", Span::call_site());
    let mut code = quote!(
        const ZERO: usize = 0;
    );

    for (i, field_info) in fields.iter().enumerate() {
        let field = &field_info.field;
        let ty = &field.ty;
        let ty = if fields_are_asule {
            quote!(<#ty as zerovec::ule::AsULE>::ULE)
        } else {
            quote!(#ty)
        };
        let new_offset_ident = suffixed_ident("OFFSET", i, field.span());
        let size_ident = suffixed_ident("SIZE", i, field.span());
        let pf_code = per_field_code(field_info, &prev_offset_ident, &size_ident);
        code = quote! {
            #code;
            const #size_ident: usize = ::core::mem::size_of::<#ty>();
            const #new_offset_ident: usize = #prev_offset_ident + #size_ident;
            #pf_code;
        };

        prev_offset_ident = new_offset_ident;
    }

    (code, prev_offset_ident)
}

#[derive(Clone, Debug)]
pub(crate) struct FieldInfo<'a> {
    pub accessor: TokenStream2,
    pub field: &'a Field,
    pub index: usize,
}

impl<'a> FieldInfo<'a> {
    pub fn make_list(iter: impl Iterator<Item = &'a Field>) -> Vec<Self> {
        iter.enumerate()
            .map(|(i, field)| Self::new_for_field(field, i))
            .collect()
    }

    pub fn new_for_field(f: &'a Field, index: usize) -> Self {
        if let Some(ref i) = f.ident {
            FieldInfo {
                accessor: quote!(#i),
                field: f,
                index,
            }
        } else {
            let idx = Index::from(index);
            FieldInfo {
                accessor: quote!(#idx),
                field: f,
                index,
            }
        }
    }

    /// Get the code for setting this field in struct decl/brace syntax
    ///
    /// Use self.accessor for dot-notation accesses
    pub fn setter(&self) -> TokenStream2 {
        if let Some(ref i) = self.field.ident {
            quote!(#i: )
        } else {
            quote!()
        }
    }

    /// Produce a name for a getter for the field
    pub fn getter(&self) -> TokenStream2 {
        if let Some(ref i) = self.field.ident {
            quote!(#i)
        } else {
            suffixed_ident("field", self.index, self.field.span()).into_token_stream()
        }
    }

    /// Produce a prose name for the field for use in docs
    pub fn getter_doc_name(&self) -> String {
        if let Some(ref i) = self.field.ident {
            format!("the unsized `{i}` field")
        } else {
            format!("tuple struct field #{}", self.index)
        }
    }
}

/// Extracts all `zerovec::name(..)` attribute
pub fn extract_parenthetical_zerovec_attrs(
    attrs: &mut Vec<Attribute>,
    name: &str,
) -> Result<Vec<Ident>> {
    let mut ret = vec![];
    let mut error = None;
    attrs.retain(|a| {
        // skip the "zerovec" part
        let second_segment = a.path().segments.iter().nth(1);

        if let Some(second) = second_segment {
            if second.ident == name {
                let list = match a.parse_args::<IdentListAttribute>() {
                    Ok(l) => l,
                    Err(_) => {
                        error = Some(Error::new(
                            a.span(),
                            format!("#[zerovec::{name}(..)] takes in a comma separated list of identifiers"),
                        ));
                        return false;
                    }
                };
                ret.extend(list.idents.iter().cloned());
                return false;
            }
        }

        true
    });

    if let Some(error) = error {
        return Err(error);
    }
    Ok(ret)
}

/// Removes all attributes with `zerovec` in the name and places them in a separate vector
pub fn extract_zerovec_attributes(attrs: &mut Vec<Attribute>) -> Vec<Attribute> {
    let mut ret = vec![];
    attrs.retain(|a| {
        if a.path().segments.len() == 2 && a.path().segments[0].ident == "zerovec" {
            ret.push(a.clone());
            return false;
        }
        true
    });
    ret
}

/// Extract attributes from field, and return them
///
/// Only current field attribute is `zerovec::varule(VarUleType)`
pub fn extract_field_attributes(attrs: &mut Vec<Attribute>) -> Result<Option<Ident>> {
    let mut zerovec_attrs = extract_zerovec_attributes(attrs);
    let varule = extract_parenthetical_zerovec_attrs(&mut zerovec_attrs, "varule")?;

    if varule.len() > 1 {
        return Err(Error::new(
            varule[1].span(),
            "Found multiple #[zerovec::varule()] on one field",
        ));
    }

    if !zerovec_attrs.is_empty() {
        return Err(Error::new(
            zerovec_attrs[1].span(),
            "Found unusable #[zerovec::] attrs on field, only #[zerovec::varule()] supported",
        ));
    }

    Ok(varule.get(0).cloned())
}

#[derive(Default, Copy, Clone)]
pub struct ZeroVecAttrs {
    pub skip_kv: bool,
    pub skip_ord: bool,
    pub serialize: bool,
    pub deserialize: bool,
    pub debug: bool,
    pub hash: bool,
}

/// Removes all known zerovec:: attributes from struct attrs and validates them
pub fn extract_attributes_common(
    attrs: &mut Vec<Attribute>,
    span: Span,
    is_var: bool,
) -> Result<ZeroVecAttrs> {
    let mut zerovec_attrs = extract_zerovec_attributes(attrs);

    let derive = extract_parenthetical_zerovec_attrs(&mut zerovec_attrs, "derive")?;
    let skip = extract_parenthetical_zerovec_attrs(&mut zerovec_attrs, "skip_derive")?;

    let name = if is_var { "make_varule" } else { "make_ule" };

    if let Some(attr) = zerovec_attrs.get(0) {
        return Err(Error::new(
            attr.span(),
            format!("Found unknown or duplicate attribute for #[{name}]"),
        ));
    }

    let mut attrs = ZeroVecAttrs::default();

    for ident in derive {
        if ident == "Serialize" {
            attrs.serialize = true;
        } else if ident == "Deserialize" {
            attrs.deserialize = true;
        } else if ident == "Debug" {
            attrs.debug = true;
        } else if ident == "Hash" {
            attrs.hash = true;
        } else {
            return Err(Error::new(
                ident.span(),
                format!(
                    "Found unknown derive attribute for #[{name}]: #[zerovec::derive({ident})]"
                ),
            ));
        }
    }

    for ident in skip {
        if ident == "ZeroMapKV" {
            attrs.skip_kv = true;
        } else if ident == "Ord" {
            attrs.skip_ord = true;
        } else {
            return Err(Error::new(
                ident.span(),
                format!("Found unknown derive attribute for #[{name}]: #[zerovec::skip_derive({ident})]"),
            ));
        }
    }

    if (attrs.serialize || attrs.deserialize) && !is_var {
        return Err(Error::new(
            span,
            "#[make_ule] does not support #[zerovec::derive(Serialize, Deserialize)]",
        ));
    }

    Ok(attrs)
}
