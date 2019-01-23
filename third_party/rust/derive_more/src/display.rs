use proc_macro2::{Ident, Span, TokenStream};
use std::fmt::Display;
use syn::{
    parse::{Error, Result},
    spanned::Spanned,
    Attribute, Data, DeriveInput, Fields, Lit, Meta, MetaNameValue, NestedMeta,
};

/// Provides the hook to expand `#[derive(Display)]` into an implementation of `From`
pub fn expand(input: &DeriveInput, trait_name: &str) -> Result<TokenStream> {
    let trait_ident = Ident::new(trait_name, Span::call_site());
    let trait_path = &quote!(::std::fmt::#trait_ident);
    let trait_attr = match trait_name {
        "Display" => "display",
        "Binary" => "binary",
        "Octal" => "octal",
        "LowerHex" => "lower_hex",
        "UpperHex" => "upper_hex",
        "LowerExp" => "lower_exp",
        "UpperExp" => "upper_exp",
        "Pointer" => "pointer",
        _ => unimplemented!(),
    };

    let (impl_generics, ty_generics, where_clause) = input.generics.split_for_impl();
    let name = &input.ident;

    let arms = State {
        trait_path,
        trait_attr,
        input,
    }
    .get_match_arms()?;

    Ok(quote! {
        impl #impl_generics #trait_path for #name #ty_generics #where_clause
        {
            #[inline]
            fn fmt(&self, _derive_more_Display_formatter: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
                match self {
                    #arms
                    _ => Ok(()) // This is needed for empty enums
                }
            }
        }
    })
}

struct State<'a, 'b> {
    trait_path: &'b TokenStream,
    trait_attr: &'static str,
    input: &'a DeriveInput,
}

impl<'a, 'b> State<'a, 'b> {
    fn get_proper_syntax(&self) -> impl Display {
        format!(
            r#"Proper syntax: #[{}(fmt = "My format", "arg1", "arg2")]"#,
            self.trait_attr
        )
    }
    fn get_matcher(&self, fields: &Fields) -> TokenStream {
        match fields {
            Fields::Unit => TokenStream::new(),
            Fields::Unnamed(fields) => {
                let fields: TokenStream = (0..fields.unnamed.len())
                    .map(|n| {
                        let i = Ident::new(&format!("_{}", n), Span::call_site());
                        quote!(#i,)
                    })
                    .collect();
                quote!((#fields))
            }
            Fields::Named(fields) => {
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
    fn find_meta(&self, attrs: &[Attribute]) -> Result<Option<Meta>> {
        let mut it = attrs
            .iter()
            .filter_map(|a| a.interpret_meta())
            .filter(|m| m.name() == self.trait_attr);

        let meta = it.next();
        if it.next().is_some() {
            Err(Error::new(meta.span(), "Too many formats given"))
        } else {
            Ok(meta)
        }
    }
    fn get_meta_fmt(&self, meta: Meta) -> Result<TokenStream> {
        let list = match &meta {
            Meta::List(list) => list,
            _ => return Err(Error::new(meta.span(), self.get_proper_syntax())),
        };

        let fmt = match &list.nested[0] {
            NestedMeta::Meta(Meta::NameValue(MetaNameValue {
                ident,
                lit: Lit::Str(s),
                ..
            }))
                if ident == "fmt" =>
            {
                s
            }
            _ => return Err(Error::new(list.nested[0].span(), self.get_proper_syntax())),
        };

        let args = list
            .nested
            .iter()
            .skip(1) // skip fmt = "..."
            .try_fold(TokenStream::new(), |args, arg| {
                let arg = match arg {
                    NestedMeta::Literal(Lit::Str(s)) => s,
                    NestedMeta::Meta(Meta::Word(i)) => {
                        return Ok(quote_spanned!(list.span()=> #args #i,))
                    }
                    _ => return Err(Error::new(arg.span(), self.get_proper_syntax())),
                };
                let arg: TokenStream = match arg.parse() {
                    Ok(arg) => arg,
                    Err(e) => return Err(Error::new(arg.span(), e)),
                };
                Ok(quote_spanned!(list.span()=> #args #arg,))
            })?;

        Ok(quote_spanned!(meta.span()=> write!(_derive_more_Display_formatter, #fmt, #args)))
    }
    fn infer_fmt(&self, fields: &Fields, name: &Ident) -> Result<TokenStream> {
        let fields = match fields {
            Fields::Unit => {
                return Ok(quote!(write!(
                    _derive_more_Display_formatter,
                    stringify!(#name)
                )));
            }
            Fields::Named(fields) => &fields.named,
            Fields::Unnamed(fields) => &fields.unnamed,
        };
        if fields.len() == 0 {
            return Ok(quote!(write!(
                _derive_more_Display_formatter,
                stringify!(#name)
            )));
        } else if fields.len() > 1 {
            return Err(Error::new(
                fields.span(),
                "Can not automatically infer format for types with more than 1 field",
            ));
        }

        let trait_path = self.trait_path;
        if let Some(ident) = &fields.iter().next().as_ref().unwrap().ident {
            Ok(quote!(#trait_path::fmt(#ident, _derive_more_Display_formatter)))
        } else {
            Ok(quote!(#trait_path::fmt(_0, _derive_more_Display_formatter)))
        }
    }
    fn get_match_arms(&self) -> Result<TokenStream> {
        match &self.input.data {
            Data::Enum(e) => {
                if let Some(meta) = self.find_meta(&self.input.attrs)? {
                    let fmt = self.get_meta_fmt(meta)?;
                    e.variants.iter().try_for_each(|v| {
                        if let Some(meta) = self.find_meta(&v.attrs)? {
                            Err(Error::new(
                                meta.span(),
                                "Can not have a format on the variant when the whole enum has one",
                            ))
                        } else {
                            Ok(())
                        }
                    })?;
                    Ok(quote_spanned!(self.input.span()=> _ => #fmt,))
                } else {
                    e.variants.iter().try_fold(TokenStream::new(), |arms, v| {
                        let matcher = self.get_matcher(&v.fields);
                        let fmt = if let Some(meta) = self.find_meta(&v.attrs)? {
                            self.get_meta_fmt(meta)?
                        } else {
                            self.infer_fmt(&v.fields, &v.ident)?
                        };
                        let name = &self.input.ident;
                        let v_name = &v.ident;
                        Ok(quote_spanned!(self.input.span()=> #arms #name::#v_name #matcher => #fmt,))
                    })
                }
            }
            Data::Struct(s) => {
                let matcher = self.get_matcher(&s.fields);
                let fmt = if let Some(meta) = self.find_meta(&self.input.attrs)? {
                    self.get_meta_fmt(meta)?
                } else {
                    self.infer_fmt(&s.fields, &self.input.ident)?
                };
                let name = &self.input.ident;
                Ok(quote_spanned!(self.input.span()=> #name #matcher => #fmt,))
            }
            Data::Union(_) => {
                let meta = self.find_meta(&self.input.attrs)?.ok_or(Error::new(
                    self.input.span(),
                    "Can not automatically infer format for unions",
                ))?;
                let fmt = self.get_meta_fmt(meta)?;
                Ok(quote_spanned!(self.input.span()=> _ => #fmt,))
            }
        }
    }
}
