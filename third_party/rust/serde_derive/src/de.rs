use proc_macro2::{Literal, Span, TokenStream};
use quote::ToTokens;
use syn::punctuated::Punctuated;
use syn::spanned::Spanned;
use syn::{self, Ident, Index, Member};

use bound;
use dummy;
use fragment::{Expr, Fragment, Match, Stmts};
use internals::ast::{Container, Data, Field, Style, Variant};
use internals::{attr, replace_receiver, ungroup, Ctxt, Derive};
use pretend;

use std::collections::BTreeSet;
use std::ptr;

pub fn expand_derive_deserialize(
    input: &mut syn::DeriveInput,
) -> Result<TokenStream, Vec<syn::Error>> {
    replace_receiver(input);

    let ctxt = Ctxt::new();
    let cont = match Container::from_ast(&ctxt, input, Derive::Deserialize) {
        Some(cont) => cont,
        None => return Err(ctxt.check().unwrap_err()),
    };
    precondition(&ctxt, &cont);
    ctxt.check()?;

    let ident = &cont.ident;
    let params = Parameters::new(&cont);
    let (de_impl_generics, _, ty_generics, where_clause) = split_with_de_lifetime(&params);
    let body = Stmts(deserialize_body(&cont, &params));
    let delife = params.borrowed.de_lifetime();
    let serde = cont.attrs.serde_path();

    let impl_block = if let Some(remote) = cont.attrs.remote() {
        let vis = &input.vis;
        let used = pretend::pretend_used(&cont, params.is_packed);
        quote! {
            impl #de_impl_generics #ident #ty_generics #where_clause {
                #vis fn deserialize<__D>(__deserializer: __D) -> #serde::__private::Result<#remote #ty_generics, __D::Error>
                where
                    __D: #serde::Deserializer<#delife>,
                {
                    #used
                    #body
                }
            }
        }
    } else {
        let fn_deserialize_in_place = deserialize_in_place_body(&cont, &params);

        quote! {
            #[automatically_derived]
            impl #de_impl_generics #serde::Deserialize<#delife> for #ident #ty_generics #where_clause {
                fn deserialize<__D>(__deserializer: __D) -> #serde::__private::Result<Self, __D::Error>
                where
                    __D: #serde::Deserializer<#delife>,
                {
                    #body
                }

                #fn_deserialize_in_place
            }
        }
    };

    Ok(dummy::wrap_in_const(
        cont.attrs.custom_serde_path(),
        "DESERIALIZE",
        ident,
        impl_block,
    ))
}

fn precondition(cx: &Ctxt, cont: &Container) {
    precondition_sized(cx, cont);
    precondition_no_de_lifetime(cx, cont);
}

fn precondition_sized(cx: &Ctxt, cont: &Container) {
    if let Data::Struct(_, fields) = &cont.data {
        if let Some(last) = fields.last() {
            if let syn::Type::Slice(_) = ungroup(last.ty) {
                cx.error_spanned_by(
                    cont.original,
                    "cannot deserialize a dynamically sized struct",
                );
            }
        }
    }
}

fn precondition_no_de_lifetime(cx: &Ctxt, cont: &Container) {
    if let BorrowedLifetimes::Borrowed(_) = borrowed_lifetimes(cont) {
        for param in cont.generics.lifetimes() {
            if param.lifetime.to_string() == "'de" {
                cx.error_spanned_by(
                    &param.lifetime,
                    "cannot deserialize when there is a lifetime parameter called 'de",
                );
                return;
            }
        }
    }
}

struct Parameters {
    /// Name of the type the `derive` is on.
    local: syn::Ident,

    /// Path to the type the impl is for. Either a single `Ident` for local
    /// types or `some::remote::Ident` for remote types. Does not include
    /// generic parameters.
    this: syn::Path,

    /// Generics including any explicit and inferred bounds for the impl.
    generics: syn::Generics,

    /// Lifetimes borrowed from the deserializer. These will become bounds on
    /// the `'de` lifetime of the deserializer.
    borrowed: BorrowedLifetimes,

    /// At least one field has a serde(getter) attribute, implying that the
    /// remote type has a private field.
    has_getter: bool,

    /// Type has a repr(packed) attribute.
    is_packed: bool,
}

impl Parameters {
    fn new(cont: &Container) -> Self {
        let local = cont.ident.clone();
        let this = match cont.attrs.remote() {
            Some(remote) => remote.clone(),
            None => cont.ident.clone().into(),
        };
        let borrowed = borrowed_lifetimes(cont);
        let generics = build_generics(cont, &borrowed);
        let has_getter = cont.data.has_getter();
        let is_packed = cont.attrs.is_packed();

        Parameters {
            local,
            this,
            generics,
            borrowed,
            has_getter,
            is_packed,
        }
    }

    /// Type name to use in error messages and `&'static str` arguments to
    /// various Deserializer methods.
    fn type_name(&self) -> String {
        self.this.segments.last().unwrap().ident.to_string()
    }
}

// All the generics in the input, plus a bound `T: Deserialize` for each generic
// field type that will be deserialized by us, plus a bound `T: Default` for
// each generic field type that will be set to a default value.
fn build_generics(cont: &Container, borrowed: &BorrowedLifetimes) -> syn::Generics {
    let generics = bound::without_defaults(cont.generics);

    let generics = bound::with_where_predicates_from_fields(cont, &generics, attr::Field::de_bound);

    let generics =
        bound::with_where_predicates_from_variants(cont, &generics, attr::Variant::de_bound);

    match cont.attrs.de_bound() {
        Some(predicates) => bound::with_where_predicates(&generics, predicates),
        None => {
            let generics = match *cont.attrs.default() {
                attr::Default::Default => bound::with_self_bound(
                    cont,
                    &generics,
                    &parse_quote!(_serde::__private::Default),
                ),
                attr::Default::None | attr::Default::Path(_) => generics,
            };

            let delife = borrowed.de_lifetime();
            let generics = bound::with_bound(
                cont,
                &generics,
                needs_deserialize_bound,
                &parse_quote!(_serde::Deserialize<#delife>),
            );

            bound::with_bound(
                cont,
                &generics,
                requires_default,
                &parse_quote!(_serde::__private::Default),
            )
        }
    }
}

// Fields with a `skip_deserializing` or `deserialize_with` attribute, or which
// belong to a variant with a `skip_deserializing` or `deserialize_with`
// attribute, are not deserialized by us so we do not generate a bound. Fields
// with a `bound` attribute specify their own bound so we do not generate one.
// All other fields may need a `T: Deserialize` bound where T is the type of the
// field.
fn needs_deserialize_bound(field: &attr::Field, variant: Option<&attr::Variant>) -> bool {
    !field.skip_deserializing()
        && field.deserialize_with().is_none()
        && field.de_bound().is_none()
        && variant.map_or(true, |variant| {
            !variant.skip_deserializing()
                && variant.deserialize_with().is_none()
                && variant.de_bound().is_none()
        })
}

// Fields with a `default` attribute (not `default=...`), and fields with a
// `skip_deserializing` attribute that do not also have `default=...`.
fn requires_default(field: &attr::Field, _variant: Option<&attr::Variant>) -> bool {
    if let attr::Default::Default = *field.default() {
        true
    } else {
        false
    }
}

enum BorrowedLifetimes {
    Borrowed(BTreeSet<syn::Lifetime>),
    Static,
}

impl BorrowedLifetimes {
    fn de_lifetime(&self) -> syn::Lifetime {
        match *self {
            BorrowedLifetimes::Borrowed(_) => syn::Lifetime::new("'de", Span::call_site()),
            BorrowedLifetimes::Static => syn::Lifetime::new("'static", Span::call_site()),
        }
    }

    fn de_lifetime_def(&self) -> Option<syn::LifetimeDef> {
        match self {
            BorrowedLifetimes::Borrowed(bounds) => Some(syn::LifetimeDef {
                attrs: Vec::new(),
                lifetime: syn::Lifetime::new("'de", Span::call_site()),
                colon_token: None,
                bounds: bounds.iter().cloned().collect(),
            }),
            BorrowedLifetimes::Static => None,
        }
    }
}

// The union of lifetimes borrowed by each field of the container.
//
// These turn into bounds on the `'de` lifetime of the Deserialize impl. If
// lifetimes `'a` and `'b` are borrowed but `'c` is not, the impl is:
//
//     impl<'de: 'a + 'b, 'a, 'b, 'c> Deserialize<'de> for S<'a, 'b, 'c>
//
// If any borrowed lifetime is `'static`, then `'de: 'static` would be redundant
// and we use plain `'static` instead of `'de`.
fn borrowed_lifetimes(cont: &Container) -> BorrowedLifetimes {
    let mut lifetimes = BTreeSet::new();
    for field in cont.data.all_fields() {
        if !field.attrs.skip_deserializing() {
            lifetimes.extend(field.attrs.borrowed_lifetimes().iter().cloned());
        }
    }
    if lifetimes.iter().any(|b| b.to_string() == "'static") {
        BorrowedLifetimes::Static
    } else {
        BorrowedLifetimes::Borrowed(lifetimes)
    }
}

fn deserialize_body(cont: &Container, params: &Parameters) -> Fragment {
    if cont.attrs.transparent() {
        deserialize_transparent(cont, params)
    } else if let Some(type_from) = cont.attrs.type_from() {
        deserialize_from(type_from)
    } else if let Some(type_try_from) = cont.attrs.type_try_from() {
        deserialize_try_from(type_try_from)
    } else if let attr::Identifier::No = cont.attrs.identifier() {
        match &cont.data {
            Data::Enum(variants) => deserialize_enum(params, variants, &cont.attrs),
            Data::Struct(Style::Struct, fields) => {
                deserialize_struct(None, params, fields, &cont.attrs, None, &Untagged::No)
            }
            Data::Struct(Style::Tuple, fields) | Data::Struct(Style::Newtype, fields) => {
                deserialize_tuple(None, params, fields, &cont.attrs, None)
            }
            Data::Struct(Style::Unit, _) => deserialize_unit_struct(params, &cont.attrs),
        }
    } else {
        match &cont.data {
            Data::Enum(variants) => deserialize_custom_identifier(params, variants, &cont.attrs),
            Data::Struct(_, _) => unreachable!("checked in serde_derive_internals"),
        }
    }
}

#[cfg(feature = "deserialize_in_place")]
fn deserialize_in_place_body(cont: &Container, params: &Parameters) -> Option<Stmts> {
    // Only remote derives have getters, and we do not generate
    // deserialize_in_place for remote derives.
    assert!(!params.has_getter);

    if cont.attrs.transparent()
        || cont.attrs.type_from().is_some()
        || cont.attrs.type_try_from().is_some()
        || cont.attrs.identifier().is_some()
        || cont
            .data
            .all_fields()
            .all(|f| f.attrs.deserialize_with().is_some())
    {
        return None;
    }

    let code = match &cont.data {
        Data::Struct(Style::Struct, fields) => {
            deserialize_struct_in_place(None, params, fields, &cont.attrs, None)?
        }
        Data::Struct(Style::Tuple, fields) | Data::Struct(Style::Newtype, fields) => {
            deserialize_tuple_in_place(None, params, fields, &cont.attrs, None)
        }
        Data::Enum(_) | Data::Struct(Style::Unit, _) => {
            return None;
        }
    };

    let delife = params.borrowed.de_lifetime();
    let stmts = Stmts(code);

    let fn_deserialize_in_place = quote_block! {
        fn deserialize_in_place<__D>(__deserializer: __D, __place: &mut Self) -> _serde::__private::Result<(), __D::Error>
        where
            __D: _serde::Deserializer<#delife>,
        {
            #stmts
        }
    };

    Some(Stmts(fn_deserialize_in_place))
}

#[cfg(not(feature = "deserialize_in_place"))]
fn deserialize_in_place_body(_cont: &Container, _params: &Parameters) -> Option<Stmts> {
    None
}

fn deserialize_transparent(cont: &Container, params: &Parameters) -> Fragment {
    let fields = match &cont.data {
        Data::Struct(_, fields) => fields,
        Data::Enum(_) => unreachable!(),
    };

    let this = &params.this;
    let transparent_field = fields.iter().find(|f| f.attrs.transparent()).unwrap();

    let path = match transparent_field.attrs.deserialize_with() {
        Some(path) => quote!(#path),
        None => {
            let span = transparent_field.original.span();
            quote_spanned!(span=> _serde::Deserialize::deserialize)
        }
    };

    let assign = fields.iter().map(|field| {
        let member = &field.member;
        if ptr::eq(field, transparent_field) {
            quote!(#member: __transparent)
        } else {
            let value = match field.attrs.default() {
                attr::Default::Default => quote!(_serde::__private::Default::default()),
                attr::Default::Path(path) => quote!(#path()),
                attr::Default::None => quote!(_serde::__private::PhantomData),
            };
            quote!(#member: #value)
        }
    });

    quote_block! {
        _serde::__private::Result::map(
            #path(__deserializer),
            |__transparent| #this { #(#assign),* })
    }
}

fn deserialize_from(type_from: &syn::Type) -> Fragment {
    quote_block! {
        _serde::__private::Result::map(
            <#type_from as _serde::Deserialize>::deserialize(__deserializer),
            _serde::__private::From::from)
    }
}

fn deserialize_try_from(type_try_from: &syn::Type) -> Fragment {
    quote_block! {
        _serde::__private::Result::and_then(
            <#type_try_from as _serde::Deserialize>::deserialize(__deserializer),
            |v| _serde::__private::TryFrom::try_from(v).map_err(_serde::de::Error::custom))
    }
}

fn deserialize_unit_struct(params: &Parameters, cattrs: &attr::Container) -> Fragment {
    let this = &params.this;
    let type_name = cattrs.name().deserialize_name();

    let expecting = format!("unit struct {}", params.type_name());
    let expecting = cattrs.expecting().unwrap_or(&expecting);

    quote_block! {
        struct __Visitor;

        impl<'de> _serde::de::Visitor<'de> for __Visitor {
            type Value = #this;

            fn expecting(&self, __formatter: &mut _serde::__private::Formatter) -> _serde::__private::fmt::Result {
                _serde::__private::Formatter::write_str(__formatter, #expecting)
            }

            #[inline]
            fn visit_unit<__E>(self) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                _serde::__private::Ok(#this)
            }
        }

        _serde::Deserializer::deserialize_unit_struct(__deserializer, #type_name, __Visitor)
    }
}

fn deserialize_tuple(
    variant_ident: Option<&syn::Ident>,
    params: &Parameters,
    fields: &[Field],
    cattrs: &attr::Container,
    deserializer: Option<TokenStream>,
) -> Fragment {
    let this = &params.this;
    let (de_impl_generics, de_ty_generics, ty_generics, where_clause) =
        split_with_de_lifetime(params);
    let delife = params.borrowed.de_lifetime();

    assert!(!cattrs.has_flatten());

    // If there are getters (implying private fields), construct the local type
    // and use an `Into` conversion to get the remote type. If there are no
    // getters then construct the target type directly.
    let construct = if params.has_getter {
        let local = &params.local;
        quote!(#local)
    } else {
        quote!(#this)
    };

    let is_enum = variant_ident.is_some();
    let type_path = match variant_ident {
        Some(variant_ident) => quote!(#construct::#variant_ident),
        None => construct,
    };
    let expecting = match variant_ident {
        Some(variant_ident) => format!("tuple variant {}::{}", params.type_name(), variant_ident),
        None => format!("tuple struct {}", params.type_name()),
    };
    let expecting = cattrs.expecting().unwrap_or(&expecting);

    let nfields = fields.len();

    let visit_newtype_struct = if !is_enum && nfields == 1 {
        Some(deserialize_newtype_struct(&type_path, params, &fields[0]))
    } else {
        None
    };

    let visit_seq = Stmts(deserialize_seq(
        &type_path, params, fields, false, cattrs, expecting,
    ));

    let visitor_expr = quote! {
        __Visitor {
            marker: _serde::__private::PhantomData::<#this #ty_generics>,
            lifetime: _serde::__private::PhantomData,
        }
    };
    let dispatch = if let Some(deserializer) = deserializer {
        quote!(_serde::Deserializer::deserialize_tuple(#deserializer, #nfields, #visitor_expr))
    } else if is_enum {
        quote!(_serde::de::VariantAccess::tuple_variant(__variant, #nfields, #visitor_expr))
    } else if nfields == 1 {
        let type_name = cattrs.name().deserialize_name();
        quote!(_serde::Deserializer::deserialize_newtype_struct(__deserializer, #type_name, #visitor_expr))
    } else {
        let type_name = cattrs.name().deserialize_name();
        quote!(_serde::Deserializer::deserialize_tuple_struct(__deserializer, #type_name, #nfields, #visitor_expr))
    };

    let all_skipped = fields.iter().all(|field| field.attrs.skip_deserializing());
    let visitor_var = if all_skipped {
        quote!(_)
    } else {
        quote!(mut __seq)
    };

    quote_block! {
        struct __Visitor #de_impl_generics #where_clause {
            marker: _serde::__private::PhantomData<#this #ty_generics>,
            lifetime: _serde::__private::PhantomData<&#delife ()>,
        }

        impl #de_impl_generics _serde::de::Visitor<#delife> for __Visitor #de_ty_generics #where_clause {
            type Value = #this #ty_generics;

            fn expecting(&self, __formatter: &mut _serde::__private::Formatter) -> _serde::__private::fmt::Result {
                _serde::__private::Formatter::write_str(__formatter, #expecting)
            }

            #visit_newtype_struct

            #[inline]
            fn visit_seq<__A>(self, #visitor_var: __A) -> _serde::__private::Result<Self::Value, __A::Error>
            where
                __A: _serde::de::SeqAccess<#delife>,
            {
                #visit_seq
            }
        }

        #dispatch
    }
}

#[cfg(feature = "deserialize_in_place")]
fn deserialize_tuple_in_place(
    variant_ident: Option<syn::Ident>,
    params: &Parameters,
    fields: &[Field],
    cattrs: &attr::Container,
    deserializer: Option<TokenStream>,
) -> Fragment {
    let this = &params.this;
    let (de_impl_generics, de_ty_generics, ty_generics, where_clause) =
        split_with_de_lifetime(params);
    let delife = params.borrowed.de_lifetime();

    assert!(!cattrs.has_flatten());

    let is_enum = variant_ident.is_some();
    let expecting = match variant_ident {
        Some(variant_ident) => format!("tuple variant {}::{}", params.type_name(), variant_ident),
        None => format!("tuple struct {}", params.type_name()),
    };
    let expecting = cattrs.expecting().unwrap_or(&expecting);

    let nfields = fields.len();

    let visit_newtype_struct = if !is_enum && nfields == 1 {
        Some(deserialize_newtype_struct_in_place(params, &fields[0]))
    } else {
        None
    };

    let visit_seq = Stmts(deserialize_seq_in_place(params, fields, cattrs, expecting));

    let visitor_expr = quote! {
        __Visitor {
            place: __place,
            lifetime: _serde::__private::PhantomData,
        }
    };

    let dispatch = if let Some(deserializer) = deserializer {
        quote!(_serde::Deserializer::deserialize_tuple(#deserializer, #nfields, #visitor_expr))
    } else if is_enum {
        quote!(_serde::de::VariantAccess::tuple_variant(__variant, #nfields, #visitor_expr))
    } else if nfields == 1 {
        let type_name = cattrs.name().deserialize_name();
        quote!(_serde::Deserializer::deserialize_newtype_struct(__deserializer, #type_name, #visitor_expr))
    } else {
        let type_name = cattrs.name().deserialize_name();
        quote!(_serde::Deserializer::deserialize_tuple_struct(__deserializer, #type_name, #nfields, #visitor_expr))
    };

    let all_skipped = fields.iter().all(|field| field.attrs.skip_deserializing());
    let visitor_var = if all_skipped {
        quote!(_)
    } else {
        quote!(mut __seq)
    };

    let in_place_impl_generics = de_impl_generics.in_place();
    let in_place_ty_generics = de_ty_generics.in_place();
    let place_life = place_lifetime();

    quote_block! {
        struct __Visitor #in_place_impl_generics #where_clause {
            place: &#place_life mut #this #ty_generics,
            lifetime: _serde::__private::PhantomData<&#delife ()>,
        }

        impl #in_place_impl_generics _serde::de::Visitor<#delife> for __Visitor #in_place_ty_generics #where_clause {
            type Value = ();

            fn expecting(&self, __formatter: &mut _serde::__private::Formatter) -> _serde::__private::fmt::Result {
                _serde::__private::Formatter::write_str(__formatter, #expecting)
            }

            #visit_newtype_struct

            #[inline]
            fn visit_seq<__A>(self, #visitor_var: __A) -> _serde::__private::Result<Self::Value, __A::Error>
            where
                __A: _serde::de::SeqAccess<#delife>,
            {
                #visit_seq
            }
        }

        #dispatch
    }
}

fn deserialize_seq(
    type_path: &TokenStream,
    params: &Parameters,
    fields: &[Field],
    is_struct: bool,
    cattrs: &attr::Container,
    expecting: &str,
) -> Fragment {
    let vars = (0..fields.len()).map(field_i as fn(_) -> _);

    let deserialized_count = fields
        .iter()
        .filter(|field| !field.attrs.skip_deserializing())
        .count();
    let expecting = if deserialized_count == 1 {
        format!("{} with 1 element", expecting)
    } else {
        format!("{} with {} elements", expecting, deserialized_count)
    };
    let expecting = cattrs.expecting().unwrap_or(&expecting);

    let mut index_in_seq = 0_usize;
    let let_values = vars.clone().zip(fields).map(|(var, field)| {
        if field.attrs.skip_deserializing() {
            let default = Expr(expr_is_missing(field, cattrs));
            quote! {
                let #var = #default;
            }
        } else {
            let visit = match field.attrs.deserialize_with() {
                None => {
                    let field_ty = field.ty;
                    let span = field.original.span();
                    let func =
                        quote_spanned!(span=> _serde::de::SeqAccess::next_element::<#field_ty>);
                    quote!(try!(#func(&mut __seq)))
                }
                Some(path) => {
                    let (wrapper, wrapper_ty) = wrap_deserialize_field_with(params, field.ty, path);
                    quote!({
                        #wrapper
                        _serde::__private::Option::map(
                            try!(_serde::de::SeqAccess::next_element::<#wrapper_ty>(&mut __seq)),
                            |__wrap| __wrap.value)
                    })
                }
            };
            let value_if_none = match field.attrs.default() {
                attr::Default::Default => quote!(_serde::__private::Default::default()),
                attr::Default::Path(path) => quote!(#path()),
                attr::Default::None => quote!(
                    return _serde::__private::Err(_serde::de::Error::invalid_length(#index_in_seq, &#expecting));
                ),
            };
            let assign = quote! {
                let #var = match #visit {
                    _serde::__private::Some(__value) => __value,
                    _serde::__private::None => {
                        #value_if_none
                    }
                };
            };
            index_in_seq += 1;
            assign
        }
    });

    let mut result = if is_struct {
        let names = fields.iter().map(|f| &f.member);
        quote! {
            #type_path { #( #names: #vars ),* }
        }
    } else {
        quote! {
            #type_path ( #(#vars),* )
        }
    };

    if params.has_getter {
        let this = &params.this;
        result = quote! {
            _serde::__private::Into::<#this>::into(#result)
        };
    }

    let let_default = match cattrs.default() {
        attr::Default::Default => Some(quote!(
            let __default: Self::Value = _serde::__private::Default::default();
        )),
        attr::Default::Path(path) => Some(quote!(
            let __default: Self::Value = #path();
        )),
        attr::Default::None => {
            // We don't need the default value, to prevent an unused variable warning
            // we'll leave the line empty.
            None
        }
    };

    quote_block! {
        #let_default
        #(#let_values)*
        _serde::__private::Ok(#result)
    }
}

#[cfg(feature = "deserialize_in_place")]
fn deserialize_seq_in_place(
    params: &Parameters,
    fields: &[Field],
    cattrs: &attr::Container,
    expecting: &str,
) -> Fragment {
    let deserialized_count = fields
        .iter()
        .filter(|field| !field.attrs.skip_deserializing())
        .count();
    let expecting = if deserialized_count == 1 {
        format!("{} with 1 element", expecting)
    } else {
        format!("{} with {} elements", expecting, deserialized_count)
    };
    let expecting = cattrs.expecting().unwrap_or(&expecting);

    let mut index_in_seq = 0usize;
    let write_values = fields.iter().map(|field| {
        let member = &field.member;

        if field.attrs.skip_deserializing() {
            let default = Expr(expr_is_missing(field, cattrs));
            quote! {
                self.place.#member = #default;
            }
        } else {
            let value_if_none = match field.attrs.default() {
                attr::Default::Default => quote!(
                    self.place.#member = _serde::__private::Default::default();
                ),
                attr::Default::Path(path) => quote!(
                    self.place.#member = #path();
                ),
                attr::Default::None => quote!(
                    return _serde::__private::Err(_serde::de::Error::invalid_length(#index_in_seq, &#expecting));
                ),
            };
            let write = match field.attrs.deserialize_with() {
                None => {
                    quote! {
                        if let _serde::__private::None = try!(_serde::de::SeqAccess::next_element_seed(&mut __seq,
                            _serde::__private::de::InPlaceSeed(&mut self.place.#member)))
                        {
                            #value_if_none
                        }
                    }
                }
                Some(path) => {
                    let (wrapper, wrapper_ty) = wrap_deserialize_field_with(params, field.ty, path);
                    quote!({
                        #wrapper
                        match try!(_serde::de::SeqAccess::next_element::<#wrapper_ty>(&mut __seq)) {
                            _serde::__private::Some(__wrap) => {
                                self.place.#member = __wrap.value;
                            }
                            _serde::__private::None => {
                                #value_if_none
                            }
                        }
                    })
                }
            };
            index_in_seq += 1;
            write
        }
    });

    let this = &params.this;
    let (_, ty_generics, _) = params.generics.split_for_impl();
    let let_default = match cattrs.default() {
        attr::Default::Default => Some(quote!(
            let __default: #this #ty_generics  = _serde::__private::Default::default();
        )),
        attr::Default::Path(path) => Some(quote!(
            let __default: #this #ty_generics  = #path();
        )),
        attr::Default::None => {
            // We don't need the default value, to prevent an unused variable warning
            // we'll leave the line empty.
            None
        }
    };

    quote_block! {
        #let_default
        #(#write_values)*
        _serde::__private::Ok(())
    }
}

fn deserialize_newtype_struct(
    type_path: &TokenStream,
    params: &Parameters,
    field: &Field,
) -> TokenStream {
    let delife = params.borrowed.de_lifetime();
    let field_ty = field.ty;

    let value = match field.attrs.deserialize_with() {
        None => {
            let span = field.original.span();
            let func = quote_spanned!(span=> <#field_ty as _serde::Deserialize>::deserialize);
            quote! {
                try!(#func(__e))
            }
        }
        Some(path) => {
            quote! {
                try!(#path(__e))
            }
        }
    };

    let mut result = quote!(#type_path(__field0));
    if params.has_getter {
        let this = &params.this;
        result = quote! {
            _serde::__private::Into::<#this>::into(#result)
        };
    }

    quote! {
        #[inline]
        fn visit_newtype_struct<__E>(self, __e: __E) -> _serde::__private::Result<Self::Value, __E::Error>
        where
            __E: _serde::Deserializer<#delife>,
        {
            let __field0: #field_ty = #value;
            _serde::__private::Ok(#result)
        }
    }
}

#[cfg(feature = "deserialize_in_place")]
fn deserialize_newtype_struct_in_place(params: &Parameters, field: &Field) -> TokenStream {
    // We do not generate deserialize_in_place if every field has a
    // deserialize_with.
    assert!(field.attrs.deserialize_with().is_none());

    let delife = params.borrowed.de_lifetime();

    quote! {
        #[inline]
        fn visit_newtype_struct<__E>(self, __e: __E) -> _serde::__private::Result<Self::Value, __E::Error>
        where
            __E: _serde::Deserializer<#delife>,
        {
            _serde::Deserialize::deserialize_in_place(__e, &mut self.place.0)
        }
    }
}

enum Untagged {
    Yes,
    No,
}

fn deserialize_struct(
    variant_ident: Option<&syn::Ident>,
    params: &Parameters,
    fields: &[Field],
    cattrs: &attr::Container,
    deserializer: Option<TokenStream>,
    untagged: &Untagged,
) -> Fragment {
    let is_enum = variant_ident.is_some();

    let this = &params.this;
    let (de_impl_generics, de_ty_generics, ty_generics, where_clause) =
        split_with_de_lifetime(params);
    let delife = params.borrowed.de_lifetime();

    // If there are getters (implying private fields), construct the local type
    // and use an `Into` conversion to get the remote type. If there are no
    // getters then construct the target type directly.
    let construct = if params.has_getter {
        let local = &params.local;
        quote!(#local)
    } else {
        quote!(#this)
    };

    let type_path = match variant_ident {
        Some(variant_ident) => quote!(#construct::#variant_ident),
        None => construct,
    };
    let expecting = match variant_ident {
        Some(variant_ident) => format!("struct variant {}::{}", params.type_name(), variant_ident),
        None => format!("struct {}", params.type_name()),
    };
    let expecting = cattrs.expecting().unwrap_or(&expecting);

    let visit_seq = Stmts(deserialize_seq(
        &type_path, params, fields, true, cattrs, expecting,
    ));

    let (field_visitor, fields_stmt, visit_map) = if cattrs.has_flatten() {
        deserialize_struct_as_map_visitor(&type_path, params, fields, cattrs)
    } else {
        deserialize_struct_as_struct_visitor(&type_path, params, fields, cattrs)
    };
    let field_visitor = Stmts(field_visitor);
    let fields_stmt = fields_stmt.map(Stmts);
    let visit_map = Stmts(visit_map);

    let visitor_expr = quote! {
        __Visitor {
            marker: _serde::__private::PhantomData::<#this #ty_generics>,
            lifetime: _serde::__private::PhantomData,
        }
    };
    let dispatch = if let Some(deserializer) = deserializer {
        quote! {
            _serde::Deserializer::deserialize_any(#deserializer, #visitor_expr)
        }
    } else if is_enum && cattrs.has_flatten() {
        quote! {
            _serde::de::VariantAccess::newtype_variant_seed(__variant, #visitor_expr)
        }
    } else if is_enum {
        quote! {
            _serde::de::VariantAccess::struct_variant(__variant, FIELDS, #visitor_expr)
        }
    } else if cattrs.has_flatten() {
        quote! {
            _serde::Deserializer::deserialize_map(__deserializer, #visitor_expr)
        }
    } else {
        let type_name = cattrs.name().deserialize_name();
        quote! {
            _serde::Deserializer::deserialize_struct(__deserializer, #type_name, FIELDS, #visitor_expr)
        }
    };

    let all_skipped = fields.iter().all(|field| field.attrs.skip_deserializing());
    let visitor_var = if all_skipped {
        quote!(_)
    } else {
        quote!(mut __seq)
    };

    // untagged struct variants do not get a visit_seq method. The same applies to
    // structs that only have a map representation.
    let visit_seq = match *untagged {
        Untagged::No if !cattrs.has_flatten() => Some(quote! {
            #[inline]
            fn visit_seq<__A>(self, #visitor_var: __A) -> _serde::__private::Result<Self::Value, __A::Error>
            where
                __A: _serde::de::SeqAccess<#delife>,
            {
                #visit_seq
            }
        }),
        _ => None,
    };

    let visitor_seed = if is_enum && cattrs.has_flatten() {
        Some(quote! {
            impl #de_impl_generics _serde::de::DeserializeSeed<#delife> for __Visitor #de_ty_generics #where_clause {
                type Value = #this #ty_generics;

                fn deserialize<__D>(self, __deserializer: __D) -> _serde::__private::Result<Self::Value, __D::Error>
                where
                    __D: _serde::Deserializer<'de>,
                {
                    _serde::Deserializer::deserialize_map(__deserializer, self)
                }
            }
        })
    } else {
        None
    };

    quote_block! {
        #field_visitor

        struct __Visitor #de_impl_generics #where_clause {
            marker: _serde::__private::PhantomData<#this #ty_generics>,
            lifetime: _serde::__private::PhantomData<&#delife ()>,
        }

        impl #de_impl_generics _serde::de::Visitor<#delife> for __Visitor #de_ty_generics #where_clause {
            type Value = #this #ty_generics;

            fn expecting(&self, __formatter: &mut _serde::__private::Formatter) -> _serde::__private::fmt::Result {
                _serde::__private::Formatter::write_str(__formatter, #expecting)
            }

            #visit_seq

            #[inline]
            fn visit_map<__A>(self, mut __map: __A) -> _serde::__private::Result<Self::Value, __A::Error>
            where
                __A: _serde::de::MapAccess<#delife>,
            {
                #visit_map
            }
        }

        #visitor_seed

        #fields_stmt

        #dispatch
    }
}

#[cfg(feature = "deserialize_in_place")]
fn deserialize_struct_in_place(
    variant_ident: Option<syn::Ident>,
    params: &Parameters,
    fields: &[Field],
    cattrs: &attr::Container,
    deserializer: Option<TokenStream>,
) -> Option<Fragment> {
    let is_enum = variant_ident.is_some();

    // for now we do not support in_place deserialization for structs that
    // are represented as map.
    if cattrs.has_flatten() {
        return None;
    }

    let this = &params.this;
    let (de_impl_generics, de_ty_generics, ty_generics, where_clause) =
        split_with_de_lifetime(params);
    let delife = params.borrowed.de_lifetime();

    let expecting = match variant_ident {
        Some(variant_ident) => format!("struct variant {}::{}", params.type_name(), variant_ident),
        None => format!("struct {}", params.type_name()),
    };
    let expecting = cattrs.expecting().unwrap_or(&expecting);

    let visit_seq = Stmts(deserialize_seq_in_place(params, fields, cattrs, expecting));

    let (field_visitor, fields_stmt, visit_map) =
        deserialize_struct_as_struct_in_place_visitor(params, fields, cattrs);

    let field_visitor = Stmts(field_visitor);
    let fields_stmt = Stmts(fields_stmt);
    let visit_map = Stmts(visit_map);

    let visitor_expr = quote! {
        __Visitor {
            place: __place,
            lifetime: _serde::__private::PhantomData,
        }
    };
    let dispatch = if let Some(deserializer) = deserializer {
        quote! {
            _serde::Deserializer::deserialize_any(#deserializer, #visitor_expr)
        }
    } else if is_enum {
        quote! {
            _serde::de::VariantAccess::struct_variant(__variant, FIELDS, #visitor_expr)
        }
    } else {
        let type_name = cattrs.name().deserialize_name();
        quote! {
            _serde::Deserializer::deserialize_struct(__deserializer, #type_name, FIELDS, #visitor_expr)
        }
    };

    let all_skipped = fields.iter().all(|field| field.attrs.skip_deserializing());
    let visitor_var = if all_skipped {
        quote!(_)
    } else {
        quote!(mut __seq)
    };

    let visit_seq = quote! {
        #[inline]
        fn visit_seq<__A>(self, #visitor_var: __A) -> _serde::__private::Result<Self::Value, __A::Error>
        where
            __A: _serde::de::SeqAccess<#delife>,
        {
            #visit_seq
        }
    };

    let in_place_impl_generics = de_impl_generics.in_place();
    let in_place_ty_generics = de_ty_generics.in_place();
    let place_life = place_lifetime();

    Some(quote_block! {
        #field_visitor

        struct __Visitor #in_place_impl_generics #where_clause {
            place: &#place_life mut #this #ty_generics,
            lifetime: _serde::__private::PhantomData<&#delife ()>,
        }

        impl #in_place_impl_generics _serde::de::Visitor<#delife> for __Visitor #in_place_ty_generics #where_clause {
            type Value = ();

            fn expecting(&self, __formatter: &mut _serde::__private::Formatter) -> _serde::__private::fmt::Result {
                _serde::__private::Formatter::write_str(__formatter, #expecting)
            }

            #visit_seq

            #[inline]
            fn visit_map<__A>(self, mut __map: __A) -> _serde::__private::Result<Self::Value, __A::Error>
            where
                __A: _serde::de::MapAccess<#delife>,
            {
                #visit_map
            }
        }

        #fields_stmt

        #dispatch
    })
}

fn deserialize_enum(
    params: &Parameters,
    variants: &[Variant],
    cattrs: &attr::Container,
) -> Fragment {
    match cattrs.tag() {
        attr::TagType::External => deserialize_externally_tagged_enum(params, variants, cattrs),
        attr::TagType::Internal { tag } => {
            deserialize_internally_tagged_enum(params, variants, cattrs, tag)
        }
        attr::TagType::Adjacent { tag, content } => {
            deserialize_adjacently_tagged_enum(params, variants, cattrs, tag, content)
        }
        attr::TagType::None => deserialize_untagged_enum(params, variants, cattrs),
    }
}

fn prepare_enum_variant_enum(
    variants: &[Variant],
    cattrs: &attr::Container,
) -> (TokenStream, Stmts) {
    let mut deserialized_variants = variants
        .iter()
        .enumerate()
        .filter(|&(_, variant)| !variant.attrs.skip_deserializing());

    let variant_names_idents: Vec<_> = deserialized_variants
        .clone()
        .map(|(i, variant)| {
            (
                variant.attrs.name().deserialize_name(),
                field_i(i),
                variant.attrs.aliases(),
            )
        })
        .collect();

    let other_idx = deserialized_variants.position(|(_, variant)| variant.attrs.other());

    let variants_stmt = {
        let variant_names = variant_names_idents.iter().map(|(name, _, _)| name);
        quote! {
            const VARIANTS: &'static [&'static str] = &[ #(#variant_names),* ];
        }
    };

    let variant_visitor = Stmts(deserialize_generated_identifier(
        &variant_names_idents,
        cattrs,
        true,
        other_idx,
    ));

    (variants_stmt, variant_visitor)
}

fn deserialize_externally_tagged_enum(
    params: &Parameters,
    variants: &[Variant],
    cattrs: &attr::Container,
) -> Fragment {
    let this = &params.this;
    let (de_impl_generics, de_ty_generics, ty_generics, where_clause) =
        split_with_de_lifetime(params);
    let delife = params.borrowed.de_lifetime();

    let type_name = cattrs.name().deserialize_name();
    let expecting = format!("enum {}", params.type_name());
    let expecting = cattrs.expecting().unwrap_or(&expecting);

    let (variants_stmt, variant_visitor) = prepare_enum_variant_enum(variants, cattrs);

    // Match arms to extract a variant from a string
    let variant_arms = variants
        .iter()
        .enumerate()
        .filter(|&(_, variant)| !variant.attrs.skip_deserializing())
        .map(|(i, variant)| {
            let variant_name = field_i(i);

            let block = Match(deserialize_externally_tagged_variant(
                params, variant, cattrs,
            ));

            quote! {
                (__Field::#variant_name, __variant) => #block
            }
        });

    let all_skipped = variants
        .iter()
        .all(|variant| variant.attrs.skip_deserializing());
    let match_variant = if all_skipped {
        // This is an empty enum like `enum Impossible {}` or an enum in which
        // all variants have `#[serde(skip_deserializing)]`.
        quote! {
            // FIXME: Once we drop support for Rust 1.15:
            // let _serde::__private::Err(__err) = _serde::de::EnumAccess::variant::<__Field>(__data);
            // _serde::__private::Err(__err)
            _serde::__private::Result::map(
                _serde::de::EnumAccess::variant::<__Field>(__data),
                |(__impossible, _)| match __impossible {})
        }
    } else {
        quote! {
            match try!(_serde::de::EnumAccess::variant(__data)) {
                #(#variant_arms)*
            }
        }
    };

    quote_block! {
        #variant_visitor

        struct __Visitor #de_impl_generics #where_clause {
            marker: _serde::__private::PhantomData<#this #ty_generics>,
            lifetime: _serde::__private::PhantomData<&#delife ()>,
        }

        impl #de_impl_generics _serde::de::Visitor<#delife> for __Visitor #de_ty_generics #where_clause {
            type Value = #this #ty_generics;

            fn expecting(&self, __formatter: &mut _serde::__private::Formatter) -> _serde::__private::fmt::Result {
                _serde::__private::Formatter::write_str(__formatter, #expecting)
            }

            fn visit_enum<__A>(self, __data: __A) -> _serde::__private::Result<Self::Value, __A::Error>
            where
                __A: _serde::de::EnumAccess<#delife>,
            {
                #match_variant
            }
        }

        #variants_stmt

        _serde::Deserializer::deserialize_enum(
            __deserializer,
            #type_name,
            VARIANTS,
            __Visitor {
                marker: _serde::__private::PhantomData::<#this #ty_generics>,
                lifetime: _serde::__private::PhantomData,
            },
        )
    }
}

fn deserialize_internally_tagged_enum(
    params: &Parameters,
    variants: &[Variant],
    cattrs: &attr::Container,
    tag: &str,
) -> Fragment {
    let (variants_stmt, variant_visitor) = prepare_enum_variant_enum(variants, cattrs);

    // Match arms to extract a variant from a string
    let variant_arms = variants
        .iter()
        .enumerate()
        .filter(|&(_, variant)| !variant.attrs.skip_deserializing())
        .map(|(i, variant)| {
            let variant_name = field_i(i);

            let block = Match(deserialize_internally_tagged_variant(
                params,
                variant,
                cattrs,
                quote! {
                    _serde::__private::de::ContentDeserializer::<__D::Error>::new(__tagged.content)
                },
            ));

            quote! {
                __Field::#variant_name => #block
            }
        });

    let expecting = format!("internally tagged enum {}", params.type_name());
    let expecting = cattrs.expecting().unwrap_or(&expecting);

    quote_block! {
        #variant_visitor

        #variants_stmt

        let __tagged = try!(_serde::Deserializer::deserialize_any(
            __deserializer,
            _serde::__private::de::TaggedContentVisitor::<__Field>::new(#tag, #expecting)));

        match __tagged.tag {
            #(#variant_arms)*
        }
    }
}

fn deserialize_adjacently_tagged_enum(
    params: &Parameters,
    variants: &[Variant],
    cattrs: &attr::Container,
    tag: &str,
    content: &str,
) -> Fragment {
    let this = &params.this;
    let (de_impl_generics, de_ty_generics, ty_generics, where_clause) =
        split_with_de_lifetime(params);
    let delife = params.borrowed.de_lifetime();

    let (variants_stmt, variant_visitor) = prepare_enum_variant_enum(variants, cattrs);

    let variant_arms: &Vec<_> = &variants
        .iter()
        .enumerate()
        .filter(|&(_, variant)| !variant.attrs.skip_deserializing())
        .map(|(i, variant)| {
            let variant_index = field_i(i);

            let block = Match(deserialize_untagged_variant(
                params,
                variant,
                cattrs,
                quote!(__deserializer),
            ));

            quote! {
                __Field::#variant_index => #block
            }
        })
        .collect();

    let expecting = format!("adjacently tagged enum {}", params.type_name());
    let expecting = cattrs.expecting().unwrap_or(&expecting);
    let type_name = cattrs.name().deserialize_name();
    let deny_unknown_fields = cattrs.deny_unknown_fields();

    // If unknown fields are allowed, we pick the visitor that can step over
    // those. Otherwise we pick the visitor that fails on unknown keys.
    let field_visitor_ty = if deny_unknown_fields {
        quote! { _serde::__private::de::TagOrContentFieldVisitor }
    } else {
        quote! { _serde::__private::de::TagContentOtherFieldVisitor }
    };

    let tag_or_content = quote! {
        #field_visitor_ty {
            tag: #tag,
            content: #content,
        }
    };

    let mut missing_content = quote! {
        _serde::__private::Err(<__A::Error as _serde::de::Error>::missing_field(#content))
    };
    let mut missing_content_fallthrough = quote!();
    let missing_content_arms = variants
        .iter()
        .enumerate()
        .filter(|&(_, variant)| !variant.attrs.skip_deserializing())
        .filter_map(|(i, variant)| {
            let variant_index = field_i(i);
            let variant_ident = &variant.ident;

            let arm = match variant.style {
                Style::Unit => quote! {
                    _serde::__private::Ok(#this::#variant_ident)
                },
                Style::Newtype if variant.attrs.deserialize_with().is_none() => {
                    let span = variant.original.span();
                    let func = quote_spanned!(span=> _serde::__private::de::missing_field);
                    quote! {
                        #func(#content).map(#this::#variant_ident)
                    }
                }
                _ => {
                    missing_content_fallthrough = quote!(_ => #missing_content);
                    return None;
                }
            };
            Some(quote! {
                __Field::#variant_index => #arm,
            })
        })
        .collect::<Vec<_>>();
    if !missing_content_arms.is_empty() {
        missing_content = quote! {
            match __field {
                #(#missing_content_arms)*
                #missing_content_fallthrough
            }
        };
    }

    // Advance the map by one key, returning early in case of error.
    let next_key = quote! {
        try!(_serde::de::MapAccess::next_key_seed(&mut __map, #tag_or_content))
    };

    // When allowing unknown fields, we want to transparently step through keys
    // we don't care about until we find `tag`, `content`, or run out of keys.
    let next_relevant_key = if deny_unknown_fields {
        next_key
    } else {
        quote!({
            let mut __rk : _serde::__private::Option<_serde::__private::de::TagOrContentField> = _serde::__private::None;
            while let _serde::__private::Some(__k) = #next_key {
                match __k {
                    _serde::__private::de::TagContentOtherField::Other => {
                        try!(_serde::de::MapAccess::next_value::<_serde::de::IgnoredAny>(&mut __map));
                        continue;
                    },
                    _serde::__private::de::TagContentOtherField::Tag => {
                        __rk = _serde::__private::Some(_serde::__private::de::TagOrContentField::Tag);
                        break;
                    }
                    _serde::__private::de::TagContentOtherField::Content => {
                        __rk = _serde::__private::Some(_serde::__private::de::TagOrContentField::Content);
                        break;
                    }
                }
            }

            __rk
        })
    };

    // Step through remaining keys, looking for duplicates of previously-seen
    // keys. When unknown fields are denied, any key that isn't a duplicate will
    // at this point immediately produce an error.
    let visit_remaining_keys = quote! {
        match #next_relevant_key {
            _serde::__private::Some(_serde::__private::de::TagOrContentField::Tag) => {
                _serde::__private::Err(<__A::Error as _serde::de::Error>::duplicate_field(#tag))
            }
            _serde::__private::Some(_serde::__private::de::TagOrContentField::Content) => {
                _serde::__private::Err(<__A::Error as _serde::de::Error>::duplicate_field(#content))
            }
            _serde::__private::None => _serde::__private::Ok(__ret),
        }
    };

    let finish_content_then_tag = if variant_arms.is_empty() {
        quote! {
            match try!(_serde::de::MapAccess::next_value::<__Field>(&mut __map)) {}
        }
    } else {
        quote! {
            let __ret = try!(match try!(_serde::de::MapAccess::next_value(&mut __map)) {
                // Deserialize the buffered content now that we know the variant.
                #(#variant_arms)*
            });
            // Visit remaining keys, looking for duplicates.
            #visit_remaining_keys
        }
    };

    quote_block! {
        #variant_visitor

        #variants_stmt

        struct __Seed #de_impl_generics #where_clause {
            field: __Field,
            marker: _serde::__private::PhantomData<#this #ty_generics>,
            lifetime: _serde::__private::PhantomData<&#delife ()>,
        }

        impl #de_impl_generics _serde::de::DeserializeSeed<#delife> for __Seed #de_ty_generics #where_clause {
            type Value = #this #ty_generics;

            fn deserialize<__D>(self, __deserializer: __D) -> _serde::__private::Result<Self::Value, __D::Error>
            where
                __D: _serde::Deserializer<#delife>,
            {
                match self.field {
                    #(#variant_arms)*
                }
            }
        }

        struct __Visitor #de_impl_generics #where_clause {
            marker: _serde::__private::PhantomData<#this #ty_generics>,
            lifetime: _serde::__private::PhantomData<&#delife ()>,
        }

        impl #de_impl_generics _serde::de::Visitor<#delife> for __Visitor #de_ty_generics #where_clause {
            type Value = #this #ty_generics;

            fn expecting(&self, __formatter: &mut _serde::__private::Formatter) -> _serde::__private::fmt::Result {
                _serde::__private::Formatter::write_str(__formatter, #expecting)
            }

            fn visit_map<__A>(self, mut __map: __A) -> _serde::__private::Result<Self::Value, __A::Error>
            where
                __A: _serde::de::MapAccess<#delife>,
            {
                // Visit the first relevant key.
                match #next_relevant_key {
                    // First key is the tag.
                    _serde::__private::Some(_serde::__private::de::TagOrContentField::Tag) => {
                        // Parse the tag.
                        let __field = try!(_serde::de::MapAccess::next_value(&mut __map));
                        // Visit the second key.
                        match #next_relevant_key {
                            // Second key is a duplicate of the tag.
                            _serde::__private::Some(_serde::__private::de::TagOrContentField::Tag) => {
                                _serde::__private::Err(<__A::Error as _serde::de::Error>::duplicate_field(#tag))
                            }
                            // Second key is the content.
                            _serde::__private::Some(_serde::__private::de::TagOrContentField::Content) => {
                                let __ret = try!(_serde::de::MapAccess::next_value_seed(&mut __map,
                                    __Seed {
                                        field: __field,
                                        marker: _serde::__private::PhantomData,
                                        lifetime: _serde::__private::PhantomData,
                                    }));
                                // Visit remaining keys, looking for duplicates.
                                #visit_remaining_keys
                            }
                            // There is no second key; might be okay if the we have a unit variant.
                            _serde::__private::None => #missing_content
                        }
                    }
                    // First key is the content.
                    _serde::__private::Some(_serde::__private::de::TagOrContentField::Content) => {
                        // Buffer up the content.
                        let __content = try!(_serde::de::MapAccess::next_value::<_serde::__private::de::Content>(&mut __map));
                        // Visit the second key.
                        match #next_relevant_key {
                            // Second key is the tag.
                            _serde::__private::Some(_serde::__private::de::TagOrContentField::Tag) => {
                                let __deserializer = _serde::__private::de::ContentDeserializer::<__A::Error>::new(__content);
                                #finish_content_then_tag
                            }
                            // Second key is a duplicate of the content.
                            _serde::__private::Some(_serde::__private::de::TagOrContentField::Content) => {
                                _serde::__private::Err(<__A::Error as _serde::de::Error>::duplicate_field(#content))
                            }
                            // There is no second key.
                            _serde::__private::None => {
                                _serde::__private::Err(<__A::Error as _serde::de::Error>::missing_field(#tag))
                            }
                        }
                    }
                    // There is no first key.
                    _serde::__private::None => {
                        _serde::__private::Err(<__A::Error as _serde::de::Error>::missing_field(#tag))
                    }
                }
            }

            fn visit_seq<__A>(self, mut __seq: __A) -> _serde::__private::Result<Self::Value, __A::Error>
            where
                __A: _serde::de::SeqAccess<#delife>,
            {
                // Visit the first element - the tag.
                match try!(_serde::de::SeqAccess::next_element(&mut __seq)) {
                    _serde::__private::Some(__field) => {
                        // Visit the second element - the content.
                        match try!(_serde::de::SeqAccess::next_element_seed(
                            &mut __seq,
                            __Seed {
                                field: __field,
                                marker: _serde::__private::PhantomData,
                                lifetime: _serde::__private::PhantomData,
                            },
                        )) {
                            _serde::__private::Some(__ret) => _serde::__private::Ok(__ret),
                            // There is no second element.
                            _serde::__private::None => {
                                _serde::__private::Err(_serde::de::Error::invalid_length(1, &self))
                            }
                        }
                    }
                    // There is no first element.
                    _serde::__private::None => {
                        _serde::__private::Err(_serde::de::Error::invalid_length(0, &self))
                    }
                }
            }
        }

        const FIELDS: &'static [&'static str] = &[#tag, #content];
        _serde::Deserializer::deserialize_struct(
            __deserializer,
            #type_name,
            FIELDS,
            __Visitor {
                marker: _serde::__private::PhantomData::<#this #ty_generics>,
                lifetime: _serde::__private::PhantomData,
            },
        )
    }
}

fn deserialize_untagged_enum(
    params: &Parameters,
    variants: &[Variant],
    cattrs: &attr::Container,
) -> Fragment {
    let attempts = variants
        .iter()
        .filter(|variant| !variant.attrs.skip_deserializing())
        .map(|variant| {
            Expr(deserialize_untagged_variant(
                params,
                variant,
                cattrs,
                quote!(
                    _serde::__private::de::ContentRefDeserializer::<__D::Error>::new(&__content)
                ),
            ))
        });

    // TODO this message could be better by saving the errors from the failed
    // attempts. The heuristic used by TOML was to count the number of fields
    // processed before an error, and use the error that happened after the
    // largest number of fields. I'm not sure I like that. Maybe it would be
    // better to save all the errors and combine them into one message that
    // explains why none of the variants matched.
    let fallthrough_msg = format!(
        "data did not match any variant of untagged enum {}",
        params.type_name()
    );
    let fallthrough_msg = cattrs.expecting().unwrap_or(&fallthrough_msg);

    quote_block! {
        let __content = try!(<_serde::__private::de::Content as _serde::Deserialize>::deserialize(__deserializer));

        #(
            if let _serde::__private::Ok(__ok) = #attempts {
                return _serde::__private::Ok(__ok);
            }
        )*

        _serde::__private::Err(_serde::de::Error::custom(#fallthrough_msg))
    }
}

fn deserialize_externally_tagged_variant(
    params: &Parameters,
    variant: &Variant,
    cattrs: &attr::Container,
) -> Fragment {
    if let Some(path) = variant.attrs.deserialize_with() {
        let (wrapper, wrapper_ty, unwrap_fn) = wrap_deserialize_variant_with(params, variant, path);
        return quote_block! {
            #wrapper
            _serde::__private::Result::map(
                _serde::de::VariantAccess::newtype_variant::<#wrapper_ty>(__variant), #unwrap_fn)
        };
    }

    let variant_ident = &variant.ident;

    match variant.style {
        Style::Unit => {
            let this = &params.this;
            quote_block! {
                try!(_serde::de::VariantAccess::unit_variant(__variant));
                _serde::__private::Ok(#this::#variant_ident)
            }
        }
        Style::Newtype => deserialize_externally_tagged_newtype_variant(
            variant_ident,
            params,
            &variant.fields[0],
            cattrs,
        ),
        Style::Tuple => {
            deserialize_tuple(Some(variant_ident), params, &variant.fields, cattrs, None)
        }
        Style::Struct => deserialize_struct(
            Some(variant_ident),
            params,
            &variant.fields,
            cattrs,
            None,
            &Untagged::No,
        ),
    }
}

// Generates significant part of the visit_seq and visit_map bodies of visitors
// for the variants of internally tagged enum.
fn deserialize_internally_tagged_variant(
    params: &Parameters,
    variant: &Variant,
    cattrs: &attr::Container,
    deserializer: TokenStream,
) -> Fragment {
    if variant.attrs.deserialize_with().is_some() {
        return deserialize_untagged_variant(params, variant, cattrs, deserializer);
    }

    let variant_ident = &variant.ident;

    match effective_style(variant) {
        Style::Unit => {
            let this = &params.this;
            let type_name = params.type_name();
            let variant_name = variant.ident.to_string();
            let default = variant.fields.get(0).map(|field| {
                let default = Expr(expr_is_missing(field, cattrs));
                quote!((#default))
            });
            quote_block! {
                try!(_serde::Deserializer::deserialize_any(#deserializer, _serde::__private::de::InternallyTaggedUnitVisitor::new(#type_name, #variant_name)));
                _serde::__private::Ok(#this::#variant_ident #default)
            }
        }
        Style::Newtype => deserialize_untagged_newtype_variant(
            variant_ident,
            params,
            &variant.fields[0],
            &deserializer,
        ),
        Style::Struct => deserialize_struct(
            Some(variant_ident),
            params,
            &variant.fields,
            cattrs,
            Some(deserializer),
            &Untagged::No,
        ),
        Style::Tuple => unreachable!("checked in serde_derive_internals"),
    }
}

fn deserialize_untagged_variant(
    params: &Parameters,
    variant: &Variant,
    cattrs: &attr::Container,
    deserializer: TokenStream,
) -> Fragment {
    if let Some(path) = variant.attrs.deserialize_with() {
        let unwrap_fn = unwrap_to_variant_closure(params, variant, false);
        return quote_block! {
            _serde::__private::Result::map(#path(#deserializer), #unwrap_fn)
        };
    }

    let variant_ident = &variant.ident;

    match effective_style(variant) {
        Style::Unit => {
            let this = &params.this;
            let type_name = params.type_name();
            let variant_name = variant.ident.to_string();
            let default = variant.fields.get(0).map(|field| {
                let default = Expr(expr_is_missing(field, cattrs));
                quote!((#default))
            });
            quote_expr! {
                match _serde::Deserializer::deserialize_any(
                    #deserializer,
                    _serde::__private::de::UntaggedUnitVisitor::new(#type_name, #variant_name)
                ) {
                    _serde::__private::Ok(()) => _serde::__private::Ok(#this::#variant_ident #default),
                    _serde::__private::Err(__err) => _serde::__private::Err(__err),
                }
            }
        }
        Style::Newtype => deserialize_untagged_newtype_variant(
            variant_ident,
            params,
            &variant.fields[0],
            &deserializer,
        ),
        Style::Tuple => deserialize_tuple(
            Some(variant_ident),
            params,
            &variant.fields,
            cattrs,
            Some(deserializer),
        ),
        Style::Struct => deserialize_struct(
            Some(variant_ident),
            params,
            &variant.fields,
            cattrs,
            Some(deserializer),
            &Untagged::Yes,
        ),
    }
}

fn deserialize_externally_tagged_newtype_variant(
    variant_ident: &syn::Ident,
    params: &Parameters,
    field: &Field,
    cattrs: &attr::Container,
) -> Fragment {
    let this = &params.this;

    if field.attrs.skip_deserializing() {
        let this = &params.this;
        let default = Expr(expr_is_missing(field, cattrs));
        return quote_block! {
            try!(_serde::de::VariantAccess::unit_variant(__variant));
            _serde::__private::Ok(#this::#variant_ident(#default))
        };
    }

    match field.attrs.deserialize_with() {
        None => {
            let field_ty = field.ty;
            let span = field.original.span();
            let func =
                quote_spanned!(span=> _serde::de::VariantAccess::newtype_variant::<#field_ty>);
            quote_expr! {
                _serde::__private::Result::map(#func(__variant), #this::#variant_ident)
            }
        }
        Some(path) => {
            let (wrapper, wrapper_ty) = wrap_deserialize_field_with(params, field.ty, path);
            quote_block! {
                #wrapper
                _serde::__private::Result::map(
                    _serde::de::VariantAccess::newtype_variant::<#wrapper_ty>(__variant),
                    |__wrapper| #this::#variant_ident(__wrapper.value))
            }
        }
    }
}

fn deserialize_untagged_newtype_variant(
    variant_ident: &syn::Ident,
    params: &Parameters,
    field: &Field,
    deserializer: &TokenStream,
) -> Fragment {
    let this = &params.this;
    let field_ty = field.ty;
    match field.attrs.deserialize_with() {
        None => {
            let span = field.original.span();
            let func = quote_spanned!(span=> <#field_ty as _serde::Deserialize>::deserialize);
            quote_expr! {
                _serde::__private::Result::map(#func(#deserializer), #this::#variant_ident)
            }
        }
        Some(path) => {
            quote_block! {
                let __value: _serde::__private::Result<#field_ty, _> = #path(#deserializer);
                _serde::__private::Result::map(__value, #this::#variant_ident)
            }
        }
    }
}

fn deserialize_generated_identifier(
    fields: &[(String, Ident, Vec<String>)],
    cattrs: &attr::Container,
    is_variant: bool,
    other_idx: Option<usize>,
) -> Fragment {
    let this = quote!(__Field);
    let field_idents: &Vec<_> = &fields.iter().map(|(_, ident, _)| ident).collect();

    let (ignore_variant, fallthrough) = if !is_variant && cattrs.has_flatten() {
        let ignore_variant = quote!(__other(_serde::__private::de::Content<'de>),);
        let fallthrough = quote!(_serde::__private::Ok(__Field::__other(__value)));
        (Some(ignore_variant), Some(fallthrough))
    } else if let Some(other_idx) = other_idx {
        let ignore_variant = fields[other_idx].1.clone();
        let fallthrough = quote!(_serde::__private::Ok(__Field::#ignore_variant));
        (None, Some(fallthrough))
    } else if is_variant || cattrs.deny_unknown_fields() {
        (None, None)
    } else {
        let ignore_variant = quote!(__ignore,);
        let fallthrough = quote!(_serde::__private::Ok(__Field::__ignore));
        (Some(ignore_variant), Some(fallthrough))
    };

    let visitor_impl = Stmts(deserialize_identifier(
        &this,
        fields,
        is_variant,
        fallthrough,
        None,
        !is_variant && cattrs.has_flatten(),
        None,
    ));

    let lifetime = if !is_variant && cattrs.has_flatten() {
        Some(quote!(<'de>))
    } else {
        None
    };

    quote_block! {
        #[allow(non_camel_case_types)]
        enum __Field #lifetime {
            #(#field_idents,)*
            #ignore_variant
        }

        struct __FieldVisitor;

        impl<'de> _serde::de::Visitor<'de> for __FieldVisitor {
            type Value = __Field #lifetime;

            #visitor_impl
        }

        impl<'de> _serde::Deserialize<'de> for __Field #lifetime {
            #[inline]
            fn deserialize<__D>(__deserializer: __D) -> _serde::__private::Result<Self, __D::Error>
            where
                __D: _serde::Deserializer<'de>,
            {
                _serde::Deserializer::deserialize_identifier(__deserializer, __FieldVisitor)
            }
        }
    }
}

// Generates `Deserialize::deserialize` body for an enum with
// `serde(field_identifier)` or `serde(variant_identifier)` attribute.
fn deserialize_custom_identifier(
    params: &Parameters,
    variants: &[Variant],
    cattrs: &attr::Container,
) -> Fragment {
    let is_variant = match cattrs.identifier() {
        attr::Identifier::Variant => true,
        attr::Identifier::Field => false,
        attr::Identifier::No => unreachable!(),
    };

    let this = &params.this;
    let this = quote!(#this);

    let (ordinary, fallthrough, fallthrough_borrowed) = if let Some(last) = variants.last() {
        let last_ident = &last.ident;
        if last.attrs.other() {
            // Process `serde(other)` attribute. It would always be found on the
            // last variant (checked in `check_identifier`), so all preceding
            // are ordinary variants.
            let ordinary = &variants[..variants.len() - 1];
            let fallthrough = quote!(_serde::__private::Ok(#this::#last_ident));
            (ordinary, Some(fallthrough), None)
        } else if let Style::Newtype = last.style {
            let ordinary = &variants[..variants.len() - 1];
            let fallthrough = |value| {
                quote! {
                    _serde::__private::Result::map(
                        _serde::Deserialize::deserialize(
                            _serde::__private::de::IdentifierDeserializer::from(#value)
                        ),
                        #this::#last_ident)
                }
            };
            (
                ordinary,
                Some(fallthrough(quote!(__value))),
                Some(fallthrough(quote!(_serde::__private::de::Borrowed(
                    __value
                )))),
            )
        } else {
            (variants, None, None)
        }
    } else {
        (variants, None, None)
    };

    let names_idents: Vec<_> = ordinary
        .iter()
        .map(|variant| {
            (
                variant.attrs.name().deserialize_name(),
                variant.ident.clone(),
                variant.attrs.aliases(),
            )
        })
        .collect();

    let names = names_idents.iter().map(|(name, _, _)| name);

    let names_const = if fallthrough.is_some() {
        None
    } else if is_variant {
        let variants = quote! {
            const VARIANTS: &'static [&'static str] = &[ #(#names),* ];
        };
        Some(variants)
    } else {
        let fields = quote! {
            const FIELDS: &'static [&'static str] = &[ #(#names),* ];
        };
        Some(fields)
    };

    let (de_impl_generics, de_ty_generics, ty_generics, where_clause) =
        split_with_de_lifetime(params);
    let delife = params.borrowed.de_lifetime();
    let visitor_impl = Stmts(deserialize_identifier(
        &this,
        &names_idents,
        is_variant,
        fallthrough,
        fallthrough_borrowed,
        false,
        cattrs.expecting(),
    ));

    quote_block! {
        #names_const

        struct __FieldVisitor #de_impl_generics #where_clause {
            marker: _serde::__private::PhantomData<#this #ty_generics>,
            lifetime: _serde::__private::PhantomData<&#delife ()>,
        }

        impl #de_impl_generics _serde::de::Visitor<#delife> for __FieldVisitor #de_ty_generics #where_clause {
            type Value = #this #ty_generics;

            #visitor_impl
        }

        let __visitor = __FieldVisitor {
            marker: _serde::__private::PhantomData::<#this #ty_generics>,
            lifetime: _serde::__private::PhantomData,
        };
        _serde::Deserializer::deserialize_identifier(__deserializer, __visitor)
    }
}

fn deserialize_identifier(
    this: &TokenStream,
    fields: &[(String, Ident, Vec<String>)],
    is_variant: bool,
    fallthrough: Option<TokenStream>,
    fallthrough_borrowed: Option<TokenStream>,
    collect_other_fields: bool,
    expecting: Option<&str>,
) -> Fragment {
    let mut flat_fields = Vec::new();
    for (_, ident, aliases) in fields {
        flat_fields.extend(aliases.iter().map(|alias| (alias, ident)));
    }

    let field_strs: &Vec<_> = &flat_fields.iter().map(|(name, _)| name).collect();
    let field_bytes: &Vec<_> = &flat_fields
        .iter()
        .map(|(name, _)| Literal::byte_string(name.as_bytes()))
        .collect();

    let constructors: &Vec<_> = &flat_fields
        .iter()
        .map(|(_, ident)| quote!(#this::#ident))
        .collect();
    let main_constructors: &Vec<_> = &fields
        .iter()
        .map(|(_, ident, _)| quote!(#this::#ident))
        .collect();

    let expecting = expecting.unwrap_or(if is_variant {
        "variant identifier"
    } else {
        "field identifier"
    });

    let index_expecting = if is_variant { "variant" } else { "field" };

    let bytes_to_str = if fallthrough.is_some() || collect_other_fields {
        None
    } else {
        Some(quote! {
            let __value = &_serde::__private::from_utf8_lossy(__value);
        })
    };

    let (
        value_as_str_content,
        value_as_borrowed_str_content,
        value_as_bytes_content,
        value_as_borrowed_bytes_content,
    ) = if collect_other_fields {
        (
            Some(quote! {
                let __value = _serde::__private::de::Content::String(_serde::__private::ToString::to_string(__value));
            }),
            Some(quote! {
                let __value = _serde::__private::de::Content::Str(__value);
            }),
            Some(quote! {
                let __value = _serde::__private::de::Content::ByteBuf(__value.to_vec());
            }),
            Some(quote! {
                let __value = _serde::__private::de::Content::Bytes(__value);
            }),
        )
    } else {
        (None, None, None, None)
    };

    let fallthrough_arm_tokens;
    let fallthrough_arm = if let Some(fallthrough) = &fallthrough {
        fallthrough
    } else if is_variant {
        fallthrough_arm_tokens = quote! {
            _serde::__private::Err(_serde::de::Error::unknown_variant(__value, VARIANTS))
        };
        &fallthrough_arm_tokens
    } else {
        fallthrough_arm_tokens = quote! {
            _serde::__private::Err(_serde::de::Error::unknown_field(__value, FIELDS))
        };
        &fallthrough_arm_tokens
    };

    let u64_fallthrough_arm_tokens;
    let u64_fallthrough_arm = if let Some(fallthrough) = &fallthrough {
        fallthrough
    } else {
        let fallthrough_msg = format!("{} index 0 <= i < {}", index_expecting, fields.len());
        u64_fallthrough_arm_tokens = quote! {
            _serde::__private::Err(_serde::de::Error::invalid_value(
                _serde::de::Unexpected::Unsigned(__value),
                &#fallthrough_msg,
            ))
        };
        &u64_fallthrough_arm_tokens
    };

    let variant_indices = 0_u64..;
    let visit_other = if collect_other_fields {
        quote! {
            fn visit_bool<__E>(self, __value: bool) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                _serde::__private::Ok(__Field::__other(_serde::__private::de::Content::Bool(__value)))
            }

            fn visit_i8<__E>(self, __value: i8) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                _serde::__private::Ok(__Field::__other(_serde::__private::de::Content::I8(__value)))
            }

            fn visit_i16<__E>(self, __value: i16) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                _serde::__private::Ok(__Field::__other(_serde::__private::de::Content::I16(__value)))
            }

            fn visit_i32<__E>(self, __value: i32) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                _serde::__private::Ok(__Field::__other(_serde::__private::de::Content::I32(__value)))
            }

            fn visit_i64<__E>(self, __value: i64) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                _serde::__private::Ok(__Field::__other(_serde::__private::de::Content::I64(__value)))
            }

            fn visit_u8<__E>(self, __value: u8) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                _serde::__private::Ok(__Field::__other(_serde::__private::de::Content::U8(__value)))
            }

            fn visit_u16<__E>(self, __value: u16) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                _serde::__private::Ok(__Field::__other(_serde::__private::de::Content::U16(__value)))
            }

            fn visit_u32<__E>(self, __value: u32) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                _serde::__private::Ok(__Field::__other(_serde::__private::de::Content::U32(__value)))
            }

            fn visit_u64<__E>(self, __value: u64) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                _serde::__private::Ok(__Field::__other(_serde::__private::de::Content::U64(__value)))
            }

            fn visit_f32<__E>(self, __value: f32) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                _serde::__private::Ok(__Field::__other(_serde::__private::de::Content::F32(__value)))
            }

            fn visit_f64<__E>(self, __value: f64) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                _serde::__private::Ok(__Field::__other(_serde::__private::de::Content::F64(__value)))
            }

            fn visit_char<__E>(self, __value: char) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                _serde::__private::Ok(__Field::__other(_serde::__private::de::Content::Char(__value)))
            }

            fn visit_unit<__E>(self) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                _serde::__private::Ok(__Field::__other(_serde::__private::de::Content::Unit))
            }
        }
    } else {
        quote! {
            fn visit_u64<__E>(self, __value: u64) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                match __value {
                    #(
                        #variant_indices => _serde::__private::Ok(#main_constructors),
                    )*
                    _ => #u64_fallthrough_arm,
                }
            }
        }
    };

    let visit_borrowed = if fallthrough_borrowed.is_some() || collect_other_fields {
        let fallthrough_borrowed_arm = fallthrough_borrowed.as_ref().unwrap_or(fallthrough_arm);
        Some(quote! {
            fn visit_borrowed_str<__E>(self, __value: &'de str) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                match __value {
                    #(
                        #field_strs => _serde::__private::Ok(#constructors),
                    )*
                    _ => {
                        #value_as_borrowed_str_content
                        #fallthrough_borrowed_arm
                    }
                }
            }

            fn visit_borrowed_bytes<__E>(self, __value: &'de [u8]) -> _serde::__private::Result<Self::Value, __E>
            where
                __E: _serde::de::Error,
            {
                match __value {
                    #(
                        #field_bytes => _serde::__private::Ok(#constructors),
                    )*
                    _ => {
                        #bytes_to_str
                        #value_as_borrowed_bytes_content
                        #fallthrough_borrowed_arm
                    }
                }
            }
        })
    } else {
        None
    };

    quote_block! {
        fn expecting(&self, __formatter: &mut _serde::__private::Formatter) -> _serde::__private::fmt::Result {
            _serde::__private::Formatter::write_str(__formatter, #expecting)
        }

        #visit_other

        fn visit_str<__E>(self, __value: &str) -> _serde::__private::Result<Self::Value, __E>
        where
            __E: _serde::de::Error,
        {
            match __value {
                #(
                    #field_strs => _serde::__private::Ok(#constructors),
                )*
                _ => {
                    #value_as_str_content
                    #fallthrough_arm
                }
            }
        }

        fn visit_bytes<__E>(self, __value: &[u8]) -> _serde::__private::Result<Self::Value, __E>
        where
            __E: _serde::de::Error,
        {
            match __value {
                #(
                    #field_bytes => _serde::__private::Ok(#constructors),
                )*
                _ => {
                    #bytes_to_str
                    #value_as_bytes_content
                    #fallthrough_arm
                }
            }
        }

        #visit_borrowed
    }
}

fn deserialize_struct_as_struct_visitor(
    struct_path: &TokenStream,
    params: &Parameters,
    fields: &[Field],
    cattrs: &attr::Container,
) -> (Fragment, Option<Fragment>, Fragment) {
    assert!(!cattrs.has_flatten());

    let field_names_idents: Vec<_> = fields
        .iter()
        .enumerate()
        .filter(|&(_, field)| !field.attrs.skip_deserializing())
        .map(|(i, field)| {
            (
                field.attrs.name().deserialize_name(),
                field_i(i),
                field.attrs.aliases(),
            )
        })
        .collect();

    let fields_stmt = {
        let field_names = field_names_idents.iter().map(|(name, _, _)| name);
        quote_block! {
            const FIELDS: &'static [&'static str] = &[ #(#field_names),* ];
        }
    };

    let field_visitor = deserialize_generated_identifier(&field_names_idents, cattrs, false, None);

    let visit_map = deserialize_map(struct_path, params, fields, cattrs);

    (field_visitor, Some(fields_stmt), visit_map)
}

fn deserialize_struct_as_map_visitor(
    struct_path: &TokenStream,
    params: &Parameters,
    fields: &[Field],
    cattrs: &attr::Container,
) -> (Fragment, Option<Fragment>, Fragment) {
    let field_names_idents: Vec<_> = fields
        .iter()
        .enumerate()
        .filter(|&(_, field)| !field.attrs.skip_deserializing() && !field.attrs.flatten())
        .map(|(i, field)| {
            (
                field.attrs.name().deserialize_name(),
                field_i(i),
                field.attrs.aliases(),
            )
        })
        .collect();

    let field_visitor = deserialize_generated_identifier(&field_names_idents, cattrs, false, None);

    let visit_map = deserialize_map(struct_path, params, fields, cattrs);

    (field_visitor, None, visit_map)
}

fn deserialize_map(
    struct_path: &TokenStream,
    params: &Parameters,
    fields: &[Field],
    cattrs: &attr::Container,
) -> Fragment {
    // Create the field names for the fields.
    let fields_names: Vec<_> = fields
        .iter()
        .enumerate()
        .map(|(i, field)| (field, field_i(i)))
        .collect();

    // Declare each field that will be deserialized.
    let let_values = fields_names
        .iter()
        .filter(|&&(field, _)| !field.attrs.skip_deserializing() && !field.attrs.flatten())
        .map(|(field, name)| {
            let field_ty = field.ty;
            quote! {
                let mut #name: _serde::__private::Option<#field_ty> = _serde::__private::None;
            }
        });

    // Collect contents for flatten fields into a buffer
    let let_collect = if cattrs.has_flatten() {
        Some(quote! {
            let mut __collect = _serde::__private::Vec::<_serde::__private::Option<(
                _serde::__private::de::Content,
                _serde::__private::de::Content
            )>>::new();
        })
    } else {
        None
    };

    // Match arms to extract a value for a field.
    let value_arms = fields_names
        .iter()
        .filter(|&&(field, _)| !field.attrs.skip_deserializing() && !field.attrs.flatten())
        .map(|(field, name)| {
            let deser_name = field.attrs.name().deserialize_name();

            let visit = match field.attrs.deserialize_with() {
                None => {
                    let field_ty = field.ty;
                    let span = field.original.span();
                    let func =
                        quote_spanned!(span=> _serde::de::MapAccess::next_value::<#field_ty>);
                    quote! {
                        try!(#func(&mut __map))
                    }
                }
                Some(path) => {
                    let (wrapper, wrapper_ty) = wrap_deserialize_field_with(params, field.ty, path);
                    quote!({
                        #wrapper
                        match _serde::de::MapAccess::next_value::<#wrapper_ty>(&mut __map) {
                            _serde::__private::Ok(__wrapper) => __wrapper.value,
                            _serde::__private::Err(__err) => {
                                return _serde::__private::Err(__err);
                            }
                        }
                    })
                }
            };
            quote! {
                __Field::#name => {
                    if _serde::__private::Option::is_some(&#name) {
                        return _serde::__private::Err(<__A::Error as _serde::de::Error>::duplicate_field(#deser_name));
                    }
                    #name = _serde::__private::Some(#visit);
                }
            }
        });

    // Visit ignored values to consume them
    let ignored_arm = if cattrs.has_flatten() {
        Some(quote! {
            __Field::__other(__name) => {
                __collect.push(_serde::__private::Some((
                    __name,
                    try!(_serde::de::MapAccess::next_value(&mut __map)))));
            }
        })
    } else if cattrs.deny_unknown_fields() {
        None
    } else {
        Some(quote! {
            _ => { let _ = try!(_serde::de::MapAccess::next_value::<_serde::de::IgnoredAny>(&mut __map)); }
        })
    };

    let all_skipped = fields.iter().all(|field| field.attrs.skip_deserializing());
    let match_keys = if cattrs.deny_unknown_fields() && all_skipped {
        quote! {
            // FIXME: Once we drop support for Rust 1.15:
            // let _serde::__private::None::<__Field> = try!(_serde::de::MapAccess::next_key(&mut __map));
            _serde::__private::Option::map(
                try!(_serde::de::MapAccess::next_key::<__Field>(&mut __map)),
                |__impossible| match __impossible {});
        }
    } else {
        quote! {
            while let _serde::__private::Some(__key) = try!(_serde::de::MapAccess::next_key::<__Field>(&mut __map)) {
                match __key {
                    #(#value_arms)*
                    #ignored_arm
                }
            }
        }
    };

    let extract_values = fields_names
        .iter()
        .filter(|&&(field, _)| !field.attrs.skip_deserializing() && !field.attrs.flatten())
        .map(|(field, name)| {
            let missing_expr = Match(expr_is_missing(field, cattrs));

            quote! {
                let #name = match #name {
                    _serde::__private::Some(#name) => #name,
                    _serde::__private::None => #missing_expr
                };
            }
        });

    let extract_collected = fields_names
        .iter()
        .filter(|&&(field, _)| field.attrs.flatten() && !field.attrs.skip_deserializing())
        .map(|(field, name)| {
            let field_ty = field.ty;
            let func = match field.attrs.deserialize_with() {
                None => {
                    let span = field.original.span();
                    quote_spanned!(span=> _serde::de::Deserialize::deserialize)
                }
                Some(path) => quote!(#path),
            };
            quote! {
                let #name: #field_ty = try!(#func(
                    _serde::__private::de::FlatMapDeserializer(
                        &mut __collect,
                        _serde::__private::PhantomData)));
            }
        });

    let collected_deny_unknown_fields = if cattrs.has_flatten() && cattrs.deny_unknown_fields() {
        Some(quote! {
            if let _serde::__private::Some(_serde::__private::Some((__key, _))) =
                __collect.into_iter().filter(_serde::__private::Option::is_some).next()
            {
                if let _serde::__private::Some(__key) = __key.as_str() {
                    return _serde::__private::Err(
                        _serde::de::Error::custom(format_args!("unknown field `{}`", &__key)));
                } else {
                    return _serde::__private::Err(
                        _serde::de::Error::custom(format_args!("unexpected map key")));
                }
            }
        })
    } else {
        None
    };

    let result = fields_names.iter().map(|(field, name)| {
        let member = &field.member;
        if field.attrs.skip_deserializing() {
            let value = Expr(expr_is_missing(field, cattrs));
            quote!(#member: #value)
        } else {
            quote!(#member: #name)
        }
    });

    let let_default = match cattrs.default() {
        attr::Default::Default => Some(quote!(
            let __default: Self::Value = _serde::__private::Default::default();
        )),
        attr::Default::Path(path) => Some(quote!(
            let __default: Self::Value = #path();
        )),
        attr::Default::None => {
            // We don't need the default value, to prevent an unused variable warning
            // we'll leave the line empty.
            None
        }
    };

    let mut result = quote!(#struct_path { #(#result),* });
    if params.has_getter {
        let this = &params.this;
        result = quote! {
            _serde::__private::Into::<#this>::into(#result)
        };
    }

    quote_block! {
        #(#let_values)*

        #let_collect

        #match_keys

        #let_default

        #(#extract_values)*

        #(#extract_collected)*

        #collected_deny_unknown_fields

        _serde::__private::Ok(#result)
    }
}

#[cfg(feature = "deserialize_in_place")]
fn deserialize_struct_as_struct_in_place_visitor(
    params: &Parameters,
    fields: &[Field],
    cattrs: &attr::Container,
) -> (Fragment, Fragment, Fragment) {
    assert!(!cattrs.has_flatten());

    let field_names_idents: Vec<_> = fields
        .iter()
        .enumerate()
        .filter(|&(_, field)| !field.attrs.skip_deserializing())
        .map(|(i, field)| {
            (
                field.attrs.name().deserialize_name(),
                field_i(i),
                field.attrs.aliases(),
            )
        })
        .collect();

    let fields_stmt = {
        let field_names = field_names_idents.iter().map(|(name, _, _)| name);
        quote_block! {
            const FIELDS: &'static [&'static str] = &[ #(#field_names),* ];
        }
    };

    let field_visitor = deserialize_generated_identifier(&field_names_idents, cattrs, false, None);

    let visit_map = deserialize_map_in_place(params, fields, cattrs);

    (field_visitor, fields_stmt, visit_map)
}

#[cfg(feature = "deserialize_in_place")]
fn deserialize_map_in_place(
    params: &Parameters,
    fields: &[Field],
    cattrs: &attr::Container,
) -> Fragment {
    assert!(!cattrs.has_flatten());

    // Create the field names for the fields.
    let fields_names: Vec<_> = fields
        .iter()
        .enumerate()
        .map(|(i, field)| (field, field_i(i)))
        .collect();

    // For deserialize_in_place, declare booleans for each field that will be
    // deserialized.
    let let_flags = fields_names
        .iter()
        .filter(|&&(field, _)| !field.attrs.skip_deserializing())
        .map(|(_, name)| {
            quote! {
                let mut #name: bool = false;
            }
        });

    // Match arms to extract a value for a field.
    let value_arms_from = fields_names
        .iter()
        .filter(|&&(field, _)| !field.attrs.skip_deserializing())
        .map(|(field, name)| {
            let deser_name = field.attrs.name().deserialize_name();
            let member = &field.member;

            let visit = match field.attrs.deserialize_with() {
                None => {
                    quote! {
                        try!(_serde::de::MapAccess::next_value_seed(&mut __map, _serde::__private::de::InPlaceSeed(&mut self.place.#member)))
                    }
                }
                Some(path) => {
                    let (wrapper, wrapper_ty) = wrap_deserialize_field_with(params, field.ty, path);
                    quote!({
                        #wrapper
                        self.place.#member = match _serde::de::MapAccess::next_value::<#wrapper_ty>(&mut __map) {
                            _serde::__private::Ok(__wrapper) => __wrapper.value,
                            _serde::__private::Err(__err) => {
                                return _serde::__private::Err(__err);
                            }
                        };
                    })
                }
            };
            quote! {
                __Field::#name => {
                    if #name {
                        return _serde::__private::Err(<__A::Error as _serde::de::Error>::duplicate_field(#deser_name));
                    }
                    #visit;
                    #name = true;
                }
            }
        });

    // Visit ignored values to consume them
    let ignored_arm = if cattrs.deny_unknown_fields() {
        None
    } else {
        Some(quote! {
            _ => { let _ = try!(_serde::de::MapAccess::next_value::<_serde::de::IgnoredAny>(&mut __map)); }
        })
    };

    let all_skipped = fields.iter().all(|field| field.attrs.skip_deserializing());

    let match_keys = if cattrs.deny_unknown_fields() && all_skipped {
        quote! {
            // FIXME: Once we drop support for Rust 1.15:
            // let _serde::__private::None::<__Field> = try!(_serde::de::MapAccess::next_key(&mut __map));
            _serde::__private::Option::map(
                try!(_serde::de::MapAccess::next_key::<__Field>(&mut __map)),
                |__impossible| match __impossible {});
        }
    } else {
        quote! {
            while let _serde::__private::Some(__key) = try!(_serde::de::MapAccess::next_key::<__Field>(&mut __map)) {
                match __key {
                    #(#value_arms_from)*
                    #ignored_arm
                }
            }
        }
    };

    let check_flags = fields_names
        .iter()
        .filter(|&&(field, _)| !field.attrs.skip_deserializing())
        .map(|(field, name)| {
            let missing_expr = expr_is_missing(field, cattrs);
            // If missing_expr unconditionally returns an error, don't try
            // to assign its value to self.place.
            if field.attrs.default().is_none()
                && cattrs.default().is_none()
                && field.attrs.deserialize_with().is_some()
            {
                let missing_expr = Stmts(missing_expr);
                quote! {
                    if !#name {
                        #missing_expr;
                    }
                }
            } else {
                let member = &field.member;
                let missing_expr = Expr(missing_expr);
                quote! {
                    if !#name {
                        self.place.#member = #missing_expr;
                    };
                }
            }
        });

    let this = &params.this;
    let (_, _, ty_generics, _) = split_with_de_lifetime(params);

    let let_default = match cattrs.default() {
        attr::Default::Default => Some(quote!(
            let __default: #this #ty_generics = _serde::__private::Default::default();
        )),
        attr::Default::Path(path) => Some(quote!(
            let __default: #this #ty_generics = #path();
        )),
        attr::Default::None => {
            // We don't need the default value, to prevent an unused variable warning
            // we'll leave the line empty.
            None
        }
    };

    quote_block! {
        #(#let_flags)*

        #match_keys

        #let_default

        #(#check_flags)*

        _serde::__private::Ok(())
    }
}

fn field_i(i: usize) -> Ident {
    Ident::new(&format!("__field{}", i), Span::call_site())
}

/// This function wraps the expression in `#[serde(deserialize_with = "...")]`
/// in a trait to prevent it from accessing the internal `Deserialize` state.
fn wrap_deserialize_with(
    params: &Parameters,
    value_ty: &TokenStream,
    deserialize_with: &syn::ExprPath,
) -> (TokenStream, TokenStream) {
    let this = &params.this;
    let (de_impl_generics, de_ty_generics, ty_generics, where_clause) =
        split_with_de_lifetime(params);
    let delife = params.borrowed.de_lifetime();

    let wrapper = quote! {
        struct __DeserializeWith #de_impl_generics #where_clause {
            value: #value_ty,
            phantom: _serde::__private::PhantomData<#this #ty_generics>,
            lifetime: _serde::__private::PhantomData<&#delife ()>,
        }

        impl #de_impl_generics _serde::Deserialize<#delife> for __DeserializeWith #de_ty_generics #where_clause {
            fn deserialize<__D>(__deserializer: __D) -> _serde::__private::Result<Self, __D::Error>
            where
                __D: _serde::Deserializer<#delife>,
            {
                _serde::__private::Ok(__DeserializeWith {
                    value: try!(#deserialize_with(__deserializer)),
                    phantom: _serde::__private::PhantomData,
                    lifetime: _serde::__private::PhantomData,
                })
            }
        }
    };

    let wrapper_ty = quote!(__DeserializeWith #de_ty_generics);

    (wrapper, wrapper_ty)
}

fn wrap_deserialize_field_with(
    params: &Parameters,
    field_ty: &syn::Type,
    deserialize_with: &syn::ExprPath,
) -> (TokenStream, TokenStream) {
    wrap_deserialize_with(params, &quote!(#field_ty), deserialize_with)
}

fn wrap_deserialize_variant_with(
    params: &Parameters,
    variant: &Variant,
    deserialize_with: &syn::ExprPath,
) -> (TokenStream, TokenStream, TokenStream) {
    let field_tys = variant.fields.iter().map(|field| field.ty);
    let (wrapper, wrapper_ty) =
        wrap_deserialize_with(params, &quote!((#(#field_tys),*)), deserialize_with);

    let unwrap_fn = unwrap_to_variant_closure(params, variant, true);

    (wrapper, wrapper_ty, unwrap_fn)
}

// Generates closure that converts single input parameter to the final value.
fn unwrap_to_variant_closure(
    params: &Parameters,
    variant: &Variant,
    with_wrapper: bool,
) -> TokenStream {
    let this = &params.this;
    let variant_ident = &variant.ident;

    let (arg, wrapper) = if with_wrapper {
        (quote! { __wrap }, quote! { __wrap.value })
    } else {
        let field_tys = variant.fields.iter().map(|field| field.ty);
        (quote! { __wrap: (#(#field_tys),*) }, quote! { __wrap })
    };

    let field_access = (0..variant.fields.len()).map(|n| {
        Member::Unnamed(Index {
            index: n as u32,
            span: Span::call_site(),
        })
    });

    match variant.style {
        Style::Struct if variant.fields.len() == 1 => {
            let member = &variant.fields[0].member;
            quote! {
                |#arg| #this::#variant_ident { #member: #wrapper }
            }
        }
        Style::Struct => {
            let members = variant.fields.iter().map(|field| &field.member);
            quote! {
                |#arg| #this::#variant_ident { #(#members: #wrapper.#field_access),* }
            }
        }
        Style::Tuple => quote! {
            |#arg| #this::#variant_ident(#(#wrapper.#field_access),*)
        },
        Style::Newtype => quote! {
            |#arg| #this::#variant_ident(#wrapper)
        },
        Style::Unit => quote! {
            |#arg| #this::#variant_ident
        },
    }
}

fn expr_is_missing(field: &Field, cattrs: &attr::Container) -> Fragment {
    match field.attrs.default() {
        attr::Default::Default => {
            let span = field.original.span();
            let func = quote_spanned!(span=> _serde::__private::Default::default);
            return quote_expr!(#func());
        }
        attr::Default::Path(path) => {
            return quote_expr!(#path());
        }
        attr::Default::None => { /* below */ }
    }

    match *cattrs.default() {
        attr::Default::Default | attr::Default::Path(_) => {
            let member = &field.member;
            return quote_expr!(__default.#member);
        }
        attr::Default::None => { /* below */ }
    }

    let name = field.attrs.name().deserialize_name();
    match field.attrs.deserialize_with() {
        None => {
            let span = field.original.span();
            let func = quote_spanned!(span=> _serde::__private::de::missing_field);
            quote_expr! {
                try!(#func(#name))
            }
        }
        Some(_) => {
            quote_expr! {
                return _serde::__private::Err(<__A::Error as _serde::de::Error>::missing_field(#name))
            }
        }
    }
}

fn effective_style(variant: &Variant) -> Style {
    match variant.style {
        Style::Newtype if variant.fields[0].attrs.skip_deserializing() => Style::Unit,
        other => other,
    }
}

struct DeImplGenerics<'a>(&'a Parameters);
#[cfg(feature = "deserialize_in_place")]
struct InPlaceImplGenerics<'a>(&'a Parameters);

impl<'a> ToTokens for DeImplGenerics<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let mut generics = self.0.generics.clone();
        if let Some(de_lifetime) = self.0.borrowed.de_lifetime_def() {
            generics.params = Some(syn::GenericParam::Lifetime(de_lifetime))
                .into_iter()
                .chain(generics.params)
                .collect();
        }
        let (impl_generics, _, _) = generics.split_for_impl();
        impl_generics.to_tokens(tokens);
    }
}

#[cfg(feature = "deserialize_in_place")]
impl<'a> ToTokens for InPlaceImplGenerics<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let place_lifetime = place_lifetime();
        let mut generics = self.0.generics.clone();

        // Add lifetime for `&'place mut Self, and `'a: 'place`
        for param in &mut generics.params {
            match param {
                syn::GenericParam::Lifetime(param) => {
                    param.bounds.push(place_lifetime.lifetime.clone());
                }
                syn::GenericParam::Type(param) => {
                    param.bounds.push(syn::TypeParamBound::Lifetime(
                        place_lifetime.lifetime.clone(),
                    ));
                }
                syn::GenericParam::Const(_) => {}
            }
        }
        generics.params = Some(syn::GenericParam::Lifetime(place_lifetime))
            .into_iter()
            .chain(generics.params)
            .collect();
        if let Some(de_lifetime) = self.0.borrowed.de_lifetime_def() {
            generics.params = Some(syn::GenericParam::Lifetime(de_lifetime))
                .into_iter()
                .chain(generics.params)
                .collect();
        }
        let (impl_generics, _, _) = generics.split_for_impl();
        impl_generics.to_tokens(tokens);
    }
}

#[cfg(feature = "deserialize_in_place")]
impl<'a> DeImplGenerics<'a> {
    fn in_place(self) -> InPlaceImplGenerics<'a> {
        InPlaceImplGenerics(self.0)
    }
}

struct DeTypeGenerics<'a>(&'a Parameters);
#[cfg(feature = "deserialize_in_place")]
struct InPlaceTypeGenerics<'a>(&'a Parameters);

impl<'a> ToTokens for DeTypeGenerics<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let mut generics = self.0.generics.clone();
        if self.0.borrowed.de_lifetime_def().is_some() {
            let def = syn::LifetimeDef {
                attrs: Vec::new(),
                lifetime: syn::Lifetime::new("'de", Span::call_site()),
                colon_token: None,
                bounds: Punctuated::new(),
            };
            generics.params = Some(syn::GenericParam::Lifetime(def))
                .into_iter()
                .chain(generics.params)
                .collect();
        }
        let (_, ty_generics, _) = generics.split_for_impl();
        ty_generics.to_tokens(tokens);
    }
}

#[cfg(feature = "deserialize_in_place")]
impl<'a> ToTokens for InPlaceTypeGenerics<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let mut generics = self.0.generics.clone();
        generics.params = Some(syn::GenericParam::Lifetime(place_lifetime()))
            .into_iter()
            .chain(generics.params)
            .collect();

        if self.0.borrowed.de_lifetime_def().is_some() {
            let def = syn::LifetimeDef {
                attrs: Vec::new(),
                lifetime: syn::Lifetime::new("'de", Span::call_site()),
                colon_token: None,
                bounds: Punctuated::new(),
            };
            generics.params = Some(syn::GenericParam::Lifetime(def))
                .into_iter()
                .chain(generics.params)
                .collect();
        }
        let (_, ty_generics, _) = generics.split_for_impl();
        ty_generics.to_tokens(tokens);
    }
}

#[cfg(feature = "deserialize_in_place")]
impl<'a> DeTypeGenerics<'a> {
    fn in_place(self) -> InPlaceTypeGenerics<'a> {
        InPlaceTypeGenerics(self.0)
    }
}

#[cfg(feature = "deserialize_in_place")]
fn place_lifetime() -> syn::LifetimeDef {
    syn::LifetimeDef {
        attrs: Vec::new(),
        lifetime: syn::Lifetime::new("'place", Span::call_site()),
        colon_token: None,
        bounds: Punctuated::new(),
    }
}

fn split_with_de_lifetime(
    params: &Parameters,
) -> (
    DeImplGenerics,
    DeTypeGenerics,
    syn::TypeGenerics,
    Option<&syn::WhereClause>,
) {
    let de_impl_generics = DeImplGenerics(params);
    let de_ty_generics = DeTypeGenerics(params);
    let (_, ty_generics, where_clause) = params.generics.split_for_impl();
    (de_impl_generics, de_ty_generics, ty_generics, where_clause)
}
