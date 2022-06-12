use std::{fmt::Display, str::FromStr as _};

use proc_macro2::{Ident, Span, TokenStream};
use quote::{quote, quote_spanned};
use syn::{
    parse::Parser as _, punctuated::Punctuated, spanned::Spanned as _, Error, Result,
};

use crate::utils;
use utils::{HashMap, HashSet};

/// Provides the hook to expand `#[derive(Display)]` into an implementation of `From`
pub fn expand(input: &syn::DeriveInput, trait_name: &str) -> Result<TokenStream> {
    let trait_name = trait_name.trim_end_matches("Custom");
    let trait_ident = syn::Ident::new(trait_name, Span::call_site());
    let trait_path = &quote!(::core::fmt::#trait_ident);
    let trait_attr = trait_name_to_attribute_name(trait_name);
    let type_params = input
        .generics
        .type_params()
        .map(|t| t.ident.clone())
        .collect();

    let ParseResult {
        arms,
        bounds,
        requires_helper,
    } = State {
        trait_path,
        trait_attr,
        input,
        type_params,
    }
    .get_match_arms_and_extra_bounds()?;

    let generics = if !bounds.is_empty() {
        let bounds: Vec<_> = bounds
            .into_iter()
            .map(|(ty, trait_names)| {
                let bounds: Vec<_> = trait_names
                    .into_iter()
                    .map(|bound| quote!(#bound))
                    .collect();
                quote!(#ty: #(#bounds)+*)
            })
            .collect();
        let where_clause = quote_spanned!(input.span()=> where #(#bounds),*);
        utils::add_extra_where_clauses(&input.generics, where_clause)
    } else {
        input.generics.clone()
    };
    let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();
    let name = &input.ident;

    let helper_struct = if requires_helper {
        display_as_helper_struct()
    } else {
        TokenStream::new()
    };

    Ok(quote! {
        impl #impl_generics #trait_path for #name #ty_generics #where_clause
        {
            #[allow(unused_variables)]
            #[inline]
            fn fmt(&self, _derive_more_display_formatter: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
                #helper_struct

                match self {
                    #arms
                    _ => Ok(()) // This is needed for empty enums
                }
            }
        }
    })
}

fn trait_name_to_attribute_name(trait_name: &str) -> &'static str {
    match trait_name {
        "Display" => "display",
        "Binary" => "binary",
        "Octal" => "octal",
        "LowerHex" => "lower_hex",
        "UpperHex" => "upper_hex",
        "LowerExp" => "lower_exp",
        "UpperExp" => "upper_exp",
        "Pointer" => "pointer",
        "Debug" => "debug",
        _ => unimplemented!(),
    }
}

fn attribute_name_to_trait_name(attribute_name: &str) -> &'static str {
    match attribute_name {
        "display" => "Display",
        "binary" => "Binary",
        "octal" => "Octal",
        "lower_hex" => "LowerHex",
        "upper_hex" => "UpperHex",
        "lower_exp" => "LowerExp",
        "upper_exp" => "UpperExp",
        "pointer" => "Pointer",
        _ => unreachable!(),
    }
}

fn trait_name_to_trait_bound(trait_name: &str) -> syn::TraitBound {
    let path_segments_iterator = vec!["core", "fmt", trait_name]
        .into_iter()
        .map(|segment| syn::PathSegment::from(Ident::new(segment, Span::call_site())));

    syn::TraitBound {
        lifetimes: None,
        modifier: syn::TraitBoundModifier::None,
        paren_token: None,
        path: syn::Path {
            leading_colon: Some(syn::Token![::](Span::call_site())),
            segments: path_segments_iterator.collect(),
        },
    }
}

/// Create a helper struct that is required by some `Display` impls.
///
/// The struct is necessary in cases where `Display` is derived for an enum
/// with an outer `#[display(fmt = "...")]` attribute and if that outer
/// format-string contains a single placeholder. In that case, we have to
/// format twice:
///
/// - we need to format each variant according to its own, optional
///   format-string,
/// - we then need to insert this formatted variant into the outer
///   format-string.
///
/// This helper struct solves this as follows:
/// - formatting the whole object inserts the helper struct into the outer
///   format string,
/// - upon being formatted, the helper struct calls an inner closure to produce
///   its formatted result,
/// - the closure in turn uses the inner, optional format-string to produce its
///   result. If there is no inner format-string, it falls back to plain
///   `$trait::fmt()`.
fn display_as_helper_struct() -> TokenStream {
    quote! {
        struct _derive_more_DisplayAs<F>(F)
        where
            F: ::core::ops::Fn(&mut ::core::fmt::Formatter) -> ::core::fmt::Result;

        const _derive_more_DisplayAs_impl: () = {
            impl<F> ::core::fmt::Display for _derive_more_DisplayAs<F>
            where
                F: ::core::ops::Fn(&mut ::core::fmt::Formatter) -> ::core::fmt::Result
            {
                fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
                    (self.0)(f)
                }
            }
        };
    }
}

/// Result type of `State::get_match_arms_and_extra_bounds()`.
#[derive(Default)]
struct ParseResult {
    /// The match arms destructuring `self`.
    arms: TokenStream,
    /// Any trait bounds that may be required.
    bounds: HashMap<syn::Type, HashSet<syn::TraitBound>>,
    /// `true` if the Display impl requires the `DisplayAs` helper struct.
    requires_helper: bool,
}

struct State<'a, 'b> {
    trait_path: &'b TokenStream,
    trait_attr: &'static str,
    input: &'a syn::DeriveInput,
    type_params: HashSet<Ident>,
}

impl<'a, 'b> State<'a, 'b> {
    fn get_proper_fmt_syntax(&self) -> impl Display {
        format!(
            r#"Proper syntax: #[{}(fmt = "My format", "arg1", "arg2")]"#,
            self.trait_attr
        )
    }
    fn get_proper_bound_syntax(&self) -> impl Display {
        format!(
            "Proper syntax: #[{}(bound = \"T, U: Trait1 + Trait2, V: Trait3\")]",
            self.trait_attr
        )
    }

    fn get_matcher(&self, fields: &syn::Fields) -> TokenStream {
        match fields {
            syn::Fields::Unit => TokenStream::new(),
            syn::Fields::Unnamed(fields) => {
                let fields: TokenStream = (0..fields.unnamed.len())
                    .map(|n| {
                        let i = Ident::new(&format!("_{}", n), Span::call_site());
                        quote!(#i,)
                    })
                    .collect();
                quote!((#fields))
            }
            syn::Fields::Named(fields) => {
                let fields: TokenStream = fields
                    .named
                    .iter()
                    .map(|f| {
                        let i = f.ident.as_ref().unwrap();
                        quote!(#i,)
                    })
                    .collect();
                quote!({#fields})
            }
        }
    }
    fn find_meta(
        &self,
        attrs: &[syn::Attribute],
        meta_key: &str,
    ) -> Result<Option<syn::Meta>> {
        let mut metas = Vec::new();
        for meta in attrs.iter().filter_map(|attr| attr.parse_meta().ok()) {
            let meta_list = match &meta {
                syn::Meta::List(meta) => meta,
                _ => continue,
            };

            if !meta_list.path.is_ident(self.trait_attr) {
                continue;
            }

            use syn::{Meta, NestedMeta};
            let meta_nv = match meta_list.nested.first() {
                Some(NestedMeta::Meta(Meta::NameValue(meta_nv))) => meta_nv,
                _ => {
                    // If the given attribute is not MetaNameValue, it most likely implies that the
                    // user is writing an incorrect format. For example:
                    // - `#[display()]`
                    // - `#[display("foo")]`
                    // - `#[display(foo)]`
                    return Err(Error::new(
                        meta.span(),
                        format!(
                            r#"The format for this attribute cannot be parsed. Correct format: `#[{}({} = "...")]`"#,
                            self.trait_attr, meta_key
                        ),
                    ));
                }
            };

            if meta_nv.path.is_ident(meta_key) {
                metas.push(meta);
            }
        }

        let mut iter = metas.into_iter();
        let meta = iter.next();
        if iter.next().is_none() {
            Ok(meta)
        } else {
            Err(Error::new(meta.span(), "Too many attributes specified"))
        }
    }
    fn parse_meta_bounds(
        &self,
        bounds: &syn::LitStr,
    ) -> Result<HashMap<syn::Type, HashSet<syn::TraitBound>>> {
        let span = bounds.span();

        let input = bounds.value();
        let tokens = TokenStream::from_str(&input)?;
        let parser = Punctuated::<syn::GenericParam, syn::Token![,]>::parse_terminated;

        let generic_params = parser
            .parse2(tokens)
            .map_err(|error| Error::new(span, error.to_string()))?;

        if generic_params.is_empty() {
            return Err(Error::new(span, "No bounds specified"));
        }

        let mut bounds = HashMap::default();

        for generic_param in generic_params {
            let type_param = match generic_param {
                syn::GenericParam::Type(type_param) => type_param,
                _ => return Err(Error::new(span, "Only trait bounds allowed")),
            };

            if !self.type_params.contains(&type_param.ident) {
                return Err(Error::new(
                    span,
                    "Unknown generic type argument specified",
                ));
            } else if !type_param.attrs.is_empty() {
                return Err(Error::new(span, "Attributes aren't allowed"));
            } else if type_param.eq_token.is_some() || type_param.default.is_some() {
                return Err(Error::new(span, "Default type parameters aren't allowed"));
            }

            let ident = type_param.ident.to_string();

            let ty = syn::Type::Path(syn::TypePath {
                qself: None,
                path: type_param.ident.into(),
            });
            let bounds = bounds.entry(ty).or_insert_with(HashSet::default);

            for bound in type_param.bounds {
                let bound = match bound {
                    syn::TypeParamBound::Trait(bound) => bound,
                    _ => return Err(Error::new(span, "Only trait bounds allowed")),
                };

                if bound.lifetimes.is_some() {
                    return Err(Error::new(
                        span,
                        "Higher-rank trait bounds aren't allowed",
                    ));
                }

                bounds.insert(bound);
            }

            if bounds.is_empty() {
                return Err(Error::new(
                    span,
                    format!("No bounds specified for type parameter {}", ident),
                ));
            }
        }

        Ok(bounds)
    }
    fn parse_meta_fmt(
        &self,
        meta: &syn::Meta,
        outer_enum: bool,
    ) -> Result<(TokenStream, bool)> {
        let list = match meta {
            syn::Meta::List(list) => list,
            _ => {
                return Err(Error::new(meta.span(), self.get_proper_fmt_syntax()));
            }
        };

        match &list.nested[0] {
            syn::NestedMeta::Meta(syn::Meta::NameValue(syn::MetaNameValue {
                path,
                lit: syn::Lit::Str(fmt),
                ..
            })) => match path {
                op if op.segments.first().expect("path shouldn't be empty").ident
                    == "fmt" =>
                {
                    let expected_affix_usage = "outer `enum` `fmt` is an affix spec that expects no args and at most 1 placeholder for inner variant display";
                    if outer_enum {
                        if list.nested.iter().skip(1).count() != 0 {
                            return Err(Error::new(
                                list.nested[1].span(),
                                expected_affix_usage,
                            ));
                        }
                        // TODO: Check for a single `Display` group?
                        let fmt_string = match &list.nested[0] {
                            syn::NestedMeta::Meta(syn::Meta::NameValue(
                                syn::MetaNameValue {
                                    path,
                                    lit: syn::Lit::Str(s),
                                    ..
                                },
                            )) if path
                                .segments
                                .first()
                                .expect("path shouldn't be empty")
                                .ident
                                == "fmt" =>
                            {
                                s.value()
                            }
                            // This one has been checked already in get_meta_fmt() method.
                            _ => unreachable!(),
                        };

                        let num_placeholders =
                            Placeholder::parse_fmt_string(&fmt_string).len();
                        if num_placeholders > 1 {
                            return Err(Error::new(
                                list.nested[1].span(),
                                expected_affix_usage,
                            ));
                        }
                        if num_placeholders == 1 {
                            return Ok((quote_spanned!(fmt.span()=> #fmt), true));
                        }
                    }
                    let args = list
                        .nested
                        .iter()
                        .skip(1) // skip fmt = "..."
                        .try_fold(TokenStream::new(), |args, arg| {
                            let arg = match arg {
                                syn::NestedMeta::Lit(syn::Lit::Str(s)) => s,
                                syn::NestedMeta::Meta(syn::Meta::Path(i)) => {
                                    return Ok(quote_spanned!(list.span()=> #args #i,));
                                }
                                _ => {
                                    return Err(Error::new(
                                        arg.span(),
                                        self.get_proper_fmt_syntax(),
                                    ))
                                }
                            };
                            let arg: TokenStream =
                                arg.parse().map_err(|e| Error::new(arg.span(), e))?;
                            Ok(quote_spanned!(list.span()=> #args #arg,))
                        })?;

                    Ok((
                        quote_spanned!(meta.span()=> write!(_derive_more_display_formatter, #fmt, #args)),
                        false,
                    ))
                }
                _ => Err(Error::new(
                    list.nested[0].span(),
                    self.get_proper_fmt_syntax(),
                )),
            },
            _ => Err(Error::new(
                list.nested[0].span(),
                self.get_proper_fmt_syntax(),
            )),
        }
    }
    fn infer_fmt(&self, fields: &syn::Fields, name: &Ident) -> Result<TokenStream> {
        let fields = match fields {
            syn::Fields::Unit => {
                return Ok(quote!(
                    _derive_more_display_formatter.write_str(stringify!(#name))
                ))
            }
            syn::Fields::Named(fields) => &fields.named,
            syn::Fields::Unnamed(fields) => &fields.unnamed,
        };
        if fields.is_empty() {
            return Ok(quote!(
                _derive_more_display_formatter.write_str(stringify!(#name))
            ));
        } else if fields.len() > 1 {
            return Err(Error::new(
                fields.span(),
                "Cannot automatically infer format for types with more than 1 field",
            ));
        }

        let trait_path = self.trait_path;
        if let Some(ident) = &fields.iter().next().as_ref().unwrap().ident {
            Ok(quote!(#trait_path::fmt(#ident, _derive_more_display_formatter)))
        } else {
            Ok(quote!(#trait_path::fmt(_0, _derive_more_display_formatter)))
        }
    }
    fn get_match_arms_and_extra_bounds(&self) -> Result<ParseResult> {
        let result: Result<_> = match &self.input.data {
            syn::Data::Enum(e) => {
                match self
                    .find_meta(&self.input.attrs, "fmt")
                    .and_then(|m| m.map(|m| self.parse_meta_fmt(&m, true)).transpose())?
                {
                    // #[display(fmt = "no placeholder")] on whole enum.
                    Some((fmt, false)) => {
                        e.variants.iter().try_for_each(|v| {
                            if let Some(meta) = self.find_meta(&v.attrs, "fmt")? {
                                Err(Error::new(
                                    meta.span(),
                                    "`fmt` cannot be used on variant when the whole enum has a format string without a placeholder, maybe you want to add a placeholder?",
                                ))
                            } else {
                                Ok(())
                            }
                        })?;

                        Ok(ParseResult {
                            arms: quote_spanned!(self.input.span()=> _ => #fmt,),
                            bounds: HashMap::default(),
                            requires_helper: false,
                        })
                    }
                    // #[display(fmt = "one placeholder: {}")] on whole enum.
                    Some((outer_fmt, true)) => {
                        let fmt: Result<TokenStream> = e.variants.iter().try_fold(TokenStream::new(), |arms, v| {
                            let matcher = self.get_matcher(&v.fields);
                            let fmt = if let Some(meta) = self.find_meta(&v.attrs, "fmt")? {
                                self.parse_meta_fmt(&meta, false)?.0
                            } else {
                                self.infer_fmt(&v.fields, &v.ident)?
                            };
                            let name = &self.input.ident;
                            let v_name = &v.ident;
                            Ok(quote_spanned!(fmt.span()=> #arms #name::#v_name #matcher => write!(
                                _derive_more_display_formatter,
                                #outer_fmt,
                                _derive_more_DisplayAs(|_derive_more_display_formatter| #fmt)
                            ),))
                        });
                        let fmt = fmt?;
                        Ok(ParseResult {
                            arms: quote_spanned!(self.input.span()=> #fmt),
                            bounds: HashMap::default(),
                            requires_helper: true,
                        })
                    }
                    // No format attribute on whole enum.
                    None => e.variants.iter().try_fold(ParseResult::default(), |result, v| {
                        let ParseResult{ arms, mut bounds, requires_helper } = result;
                        let matcher = self.get_matcher(&v.fields);
                        let name = &self.input.ident;
                        let v_name = &v.ident;
                        let fmt: TokenStream;
                        let these_bounds: HashMap<_, _>;

                        if let Some(meta) = self.find_meta(&v.attrs, "fmt")? {
                            fmt = self.parse_meta_fmt(&meta, false)?.0;
                            these_bounds = self.get_used_type_params_bounds(&v.fields, &meta);
                        } else {
                            fmt = self.infer_fmt(&v.fields, v_name)?;
                            these_bounds = self.infer_type_params_bounds(&v.fields);
                        };
                        these_bounds.into_iter().for_each(|(ty, trait_names)| {
                            bounds.entry(ty).or_default().extend(trait_names)
                        });
                        let arms = quote_spanned!(self.input.span()=> #arms #name::#v_name #matcher => #fmt,);

                        Ok(ParseResult{ arms, bounds, requires_helper })
                    }),
                }
            }
            syn::Data::Struct(s) => {
                let matcher = self.get_matcher(&s.fields);
                let name = &self.input.ident;
                let fmt: TokenStream;
                let bounds: HashMap<_, _>;

                if let Some(meta) = self.find_meta(&self.input.attrs, "fmt")? {
                    fmt = self.parse_meta_fmt(&meta, false)?.0;
                    bounds = self.get_used_type_params_bounds(&s.fields, &meta);
                } else {
                    fmt = self.infer_fmt(&s.fields, name)?;
                    bounds = self.infer_type_params_bounds(&s.fields);
                }

                Ok(ParseResult {
                    arms: quote_spanned!(self.input.span()=> #name #matcher => #fmt,),
                    bounds,
                    requires_helper: false,
                })
            }
            syn::Data::Union(_) => {
                let meta =
                    self.find_meta(&self.input.attrs, "fmt")?.ok_or_else(|| {
                        Error::new(
                            self.input.span(),
                            "Cannot automatically infer format for unions",
                        )
                    })?;
                let fmt = self.parse_meta_fmt(&meta, false)?.0;

                Ok(ParseResult {
                    arms: quote_spanned!(self.input.span()=> _ => #fmt,),
                    bounds: HashMap::default(),
                    requires_helper: false,
                })
            }
        };

        let mut result = result?;

        let meta = match self.find_meta(&self.input.attrs, "bound")? {
            Some(meta) => meta,
            _ => return Ok(result),
        };

        let span = meta.span();

        let meta = match meta {
            syn::Meta::List(meta) => meta.nested,
            _ => return Err(Error::new(span, self.get_proper_bound_syntax())),
        };

        if meta.len() != 1 {
            return Err(Error::new(span, self.get_proper_bound_syntax()));
        }

        let meta = match &meta[0] {
            syn::NestedMeta::Meta(syn::Meta::NameValue(meta)) => meta,
            _ => return Err(Error::new(span, self.get_proper_bound_syntax())),
        };

        let extra_bounds = match &meta.lit {
            syn::Lit::Str(extra_bounds) => extra_bounds,
            _ => return Err(Error::new(span, self.get_proper_bound_syntax())),
        };

        let extra_bounds = self.parse_meta_bounds(extra_bounds)?;

        extra_bounds.into_iter().for_each(|(ty, trait_names)| {
            result.bounds.entry(ty).or_default().extend(trait_names)
        });

        Ok(result)
    }
    fn get_used_type_params_bounds(
        &self,
        fields: &syn::Fields,
        meta: &syn::Meta,
    ) -> HashMap<syn::Type, HashSet<syn::TraitBound>> {
        if self.type_params.is_empty() {
            return HashMap::default();
        }

        let fields_type_params: HashMap<syn::Path, _> = fields
            .iter()
            .enumerate()
            .filter_map(|(i, field)| {
                utils::get_if_type_parameter_used_in_type(&self.type_params, &field.ty)
                    .map(|ty| {
                        (
                            field
                                .ident
                                .clone()
                                .unwrap_or_else(|| {
                                    Ident::new(&format!("_{}", i), Span::call_site())
                                })
                                .into(),
                            ty,
                        )
                    })
            })
            .collect();
        if fields_type_params.is_empty() {
            return HashMap::default();
        }

        let list = match meta {
            syn::Meta::List(list) => list,
            // This one has been checked already in get_meta_fmt() method.
            _ => unreachable!(),
        };
        let fmt_args: HashMap<_, _> = list
            .nested
            .iter()
            .skip(1) // skip fmt = "..."
            .enumerate()
            .filter_map(|(i, arg)| match arg {
                syn::NestedMeta::Lit(syn::Lit::Str(ref s)) => {
                    syn::parse_str(&s.value()).ok().map(|id| (i, id))
                }
                syn::NestedMeta::Meta(syn::Meta::Path(ref id)) => Some((i, id.clone())),
                // This one has been checked already in get_meta_fmt() method.
                _ => unreachable!(),
            })
            .collect();
        if fmt_args.is_empty() {
            return HashMap::default();
        }
        let fmt_string = match &list.nested[0] {
            syn::NestedMeta::Meta(syn::Meta::NameValue(syn::MetaNameValue {
                path,
                lit: syn::Lit::Str(s),
                ..
            })) if path
                .segments
                .first()
                .expect("path shouldn't be empty")
                .ident
                == "fmt" =>
            {
                s.value()
            }
            // This one has been checked already in get_meta_fmt() method.
            _ => unreachable!(),
        };

        Placeholder::parse_fmt_string(&fmt_string).into_iter().fold(
            HashMap::default(),
            |mut bounds, pl| {
                if let Some(arg) = fmt_args.get(&pl.position) {
                    if fields_type_params.contains_key(arg) {
                        bounds
                            .entry(fields_type_params[arg].clone())
                            .or_insert_with(HashSet::default)
                            .insert(trait_name_to_trait_bound(pl.trait_name));
                    }
                }
                bounds
            },
        )
    }
    fn infer_type_params_bounds(
        &self,
        fields: &syn::Fields,
    ) -> HashMap<syn::Type, HashSet<syn::TraitBound>> {
        if self.type_params.is_empty() {
            return HashMap::default();
        }
        if let syn::Fields::Unit = fields {
            return HashMap::default();
        }
        // infer_fmt() uses only first field.
        fields
            .iter()
            .take(1)
            .filter_map(|field| {
                utils::get_if_type_parameter_used_in_type(&self.type_params, &field.ty)
                    .map(|ty| {
                        (
                            ty,
                            [trait_name_to_trait_bound(attribute_name_to_trait_name(
                                self.trait_attr,
                            ))]
                            .iter()
                            .cloned()
                            .collect(),
                        )
                    })
            })
            .collect()
    }
}

/// Representation of formatting placeholder.
#[derive(Debug, PartialEq)]
struct Placeholder {
    /// Position of formatting argument to be used for this placeholder.
    position: usize,
    /// Name of [`std::fmt`] trait to be used for rendering this placeholder.
    trait_name: &'static str,
}

impl Placeholder {
    /// Parses [`Placeholder`]s from a given formatting string.
    fn parse_fmt_string(s: &str) -> Vec<Placeholder> {
        let mut n = 0;
        crate::parsing::all_placeholders(s)
            .into_iter()
            .flatten()
            .map(|m| {
                let (maybe_arg, maybe_typ) = crate::parsing::format(m).unwrap();
                let position = maybe_arg.unwrap_or_else(|| {
                    // Assign "the next argument".
                    // https://doc.rust-lang.org/stable/std/fmt/index.html#positional-parameters
                    n += 1;
                    n - 1
                });
                let typ = maybe_typ.unwrap_or_default();
                let trait_name = match typ {
                    "" => "Display",
                    "?" | "x?" | "X?" => "Debug",
                    "o" => "Octal",
                    "x" => "LowerHex",
                    "X" => "UpperHex",
                    "p" => "Pointer",
                    "b" => "Binary",
                    "e" => "LowerExp",
                    "E" => "UpperExp",
                    _ => unreachable!(),
                };
                Placeholder {
                    position,
                    trait_name,
                }
            })
            .collect()
    }
}

#[cfg(test)]
mod regex_maybe_placeholder_spec {

    #[test]
    fn parses_placeholders_and_omits_escaped() {
        let fmt_string = "{}, {:?}, {{}}, {{{1:0$}}}";
        let placeholders: Vec<_> = crate::parsing::all_placeholders(&fmt_string)
            .into_iter()
            .flatten()
            .collect();
        assert_eq!(placeholders, vec!["{}", "{:?}", "{1:0$}"]);
    }
}

#[cfg(test)]
mod regex_placeholder_format_spec {

    #[test]
    fn detects_type() {
        for (p, expected) in vec![
            ("{}", ""),
            ("{:?}", "?"),
            ("{:x?}", "x?"),
            ("{:X?}", "X?"),
            ("{:o}", "o"),
            ("{:x}", "x"),
            ("{:X}", "X"),
            ("{:p}", "p"),
            ("{:b}", "b"),
            ("{:e}", "e"),
            ("{:E}", "E"),
            ("{:.*}", ""),
            ("{8}", ""),
            ("{:04}", ""),
            ("{1:0$}", ""),
            ("{:width$}", ""),
            ("{9:>8.*}", ""),
            ("{2:.1$x}", "x"),
        ] {
            let typ = crate::parsing::format(p).unwrap().1.unwrap_or_default();
            assert_eq!(typ, expected);
        }
    }

    #[test]
    fn detects_arg() {
        for (p, expected) in vec![
            ("{}", ""),
            ("{0:?}", "0"),
            ("{12:x?}", "12"),
            ("{3:X?}", "3"),
            ("{5:o}", "5"),
            ("{6:x}", "6"),
            ("{:X}", ""),
            ("{8}", "8"),
            ("{:04}", ""),
            ("{1:0$}", "1"),
            ("{:width$}", ""),
            ("{9:>8.*}", "9"),
            ("{2:.1$x}", "2"),
        ] {
            let arg = crate::parsing::format(p)
                .unwrap()
                .0
                .map(|s| s.to_string())
                .unwrap_or_default();
            assert_eq!(arg, String::from(expected));
        }
    }
}

#[cfg(test)]
mod placeholder_parse_fmt_string_spec {
    use super::*;

    #[test]
    fn indicates_position_and_trait_name_for_each_fmt_placeholder() {
        let fmt_string = "{},{:?},{{}},{{{1:0$}}}-{2:.1$x}{0:#?}{:width$}";
        assert_eq!(
            Placeholder::parse_fmt_string(&fmt_string),
            vec![
                Placeholder {
                    position: 0,
                    trait_name: "Display",
                },
                Placeholder {
                    position: 1,
                    trait_name: "Debug",
                },
                Placeholder {
                    position: 1,
                    trait_name: "Display",
                },
                Placeholder {
                    position: 2,
                    trait_name: "LowerHex",
                },
                Placeholder {
                    position: 0,
                    trait_name: "Debug",
                },
                Placeholder {
                    position: 2,
                    trait_name: "Display",
                },
            ],
        )
    }
}
