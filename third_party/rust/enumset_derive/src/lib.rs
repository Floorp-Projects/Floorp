#![recursion_limit="256"]

extern crate proc_macro;

use darling::*;
use proc_macro::TokenStream;
use proc_macro2::{TokenStream as SynTokenStream, Literal, Span};
use std::collections::HashSet;
use syn::{*, Result, Error};
use syn::spanned::Spanned;
use quote::*;

/// Helper function for emitting compile errors.
fn error<T>(span: Span, message: &str) -> Result<T> {
    Err(Error::new(span, message))
}

/// Decodes the custom attributes for our custom derive.
#[derive(FromDeriveInput, Default)]
#[darling(attributes(enumset), default)]
struct EnumsetAttrs {
    no_ops: bool,
    serialize_as_list: bool,
    serialize_deny_unknown: bool,
    #[darling(default)]
    serialize_repr: Option<String>,
    #[darling(default)]
    crate_name: Option<String>,
}

/// An variant in the enum set type.
struct EnumSetValue {
    /// The name of the variant.
    name: Ident,
    /// The discriminant of the variant.
    variant_repr: u32,
}

/// Stores information about the enum set type.
#[allow(dead_code)]
struct EnumSetInfo {
    /// The name of the enum.
    name: Ident,
    /// The crate name to use.
    crate_name: Option<Ident>,
    /// The numeric type to serialize the enum as.
    explicit_serde_repr: Option<Ident>,
    /// Whether the underlying repr of the enum supports negative values.
    has_signed_repr: bool,
    /// Whether the underlying repr of the enum supports values higher than 2^32.
    has_large_repr: bool,
    /// A list of variants in the enum.
    variants: Vec<EnumSetValue>,

    /// The highest encountered variant discriminant.
    max_discrim: u32,
    /// The current variant discriminant. Used to track, e.g. `A=10,B,C`.
    cur_discrim: u32,
    /// A list of variant names that are already in use.
    used_variant_names: HashSet<String>,
    /// A list of variant discriminants that are already in use.
    used_discriminants: HashSet<u32>,

    /// Avoid generating operator overloads on the enum type.
    no_ops: bool,
    /// Serialize the enum as a list.
    serialize_as_list: bool,
    /// Disallow unknown bits while deserializing the enum.
    serialize_deny_unknown: bool,
}
impl EnumSetInfo {
    fn new(input: &DeriveInput, attrs: EnumsetAttrs) -> EnumSetInfo {
        EnumSetInfo {
            name: input.ident.clone(),
            crate_name: attrs.crate_name.map(|x| Ident::new(&x, Span::call_site())),
            explicit_serde_repr: attrs.serialize_repr.map(|x| Ident::new(&x, Span::call_site())),
            has_signed_repr: false,
            has_large_repr: false,
            variants: Vec::new(),
            max_discrim: 0,
            cur_discrim: 0,
            used_variant_names: HashSet::new(),
            used_discriminants: HashSet::new(),
            no_ops: attrs.no_ops,
            serialize_as_list: attrs.serialize_as_list,
            serialize_deny_unknown: attrs.serialize_deny_unknown
        }
    }

    /// Sets an explicit repr for the enumset.
    fn push_explicit_repr(&mut self, attr_span: Span, repr: &str) -> Result<()> {
        // Check whether the repr is supported, and if so, set some flags for better error
        // messages later on.
        match repr {
            "Rust" | "C" | "u8" | "u16" | "u32" => Ok(()),
            "usize" | "u64" | "u128" => {
                self.has_large_repr = true;
                Ok(())
            }
            "i8" | "i16" | "i32" => {
                self.has_signed_repr = true;
                Ok(())
            }
            "isize" | "i64" | "i128" => {
                self.has_signed_repr = true;
                self.has_large_repr = true;
                Ok(())
            }
            _ => error(attr_span, "Unsupported repr.")
        }
    }
    /// Adds a variant to the enumset.
    fn push_variant(&mut self, variant: &Variant) -> Result<()> {
        if self.used_variant_names.contains(&variant.ident.to_string()) {
            error(variant.span(), "Duplicated variant name.")
        } else if let Fields::Unit = variant.fields {
            // Parse the discriminant.
            if let Some((_, expr)) = &variant.discriminant {
                let discriminant_fail_message = format!(
                    "Enum set discriminants must be `u32`s.{}",
                    if self.has_signed_repr || self.has_large_repr {
                        format!(
                            " ({} discrimiants are still unsupported with reprs that allow them.)",
                            if self.has_large_repr {
                                "larger"
                            } else if self.has_signed_repr {
                                "negative"
                            } else {
                                "larger or negative"
                            }
                        )
                    } else {
                        String::new()
                    },
                );
                if let Expr::Lit(ExprLit { lit: Lit::Int(i), .. }) = expr {
                    match i.base10_parse() {
                        Ok(val) => self.cur_discrim = val,
                        Err(_) => error(expr.span(), &discriminant_fail_message)?,
                    }
                } else {
                    error(variant.span(), &discriminant_fail_message)?;
                }
            }

            // Validate the discriminant.
            let discriminant = self.cur_discrim;
            if discriminant >= 128 {
                let message = if self.variants.len() <= 127 {
                    "`#[derive(EnumSetType)]` currently only supports discriminants up to 127."
                } else {
                    "`#[derive(EnumSetType)]` currently only supports enums up to 128 variants."
                };
                error(variant.span(), message)?;
            }
            if self.used_discriminants.contains(&discriminant) {
                error(variant.span(), "Duplicated enum discriminant.")?;
            }

            // Add the variant to the info.
            self.cur_discrim += 1;
            if discriminant > self.max_discrim {
                self.max_discrim = discriminant;
            }
            self.variants.push(EnumSetValue {
                name: variant.ident.clone(),
                variant_repr: discriminant,
            });
            self.used_variant_names.insert(variant.ident.to_string());
            self.used_discriminants.insert(discriminant);

            Ok(())
        } else {
            error(variant.span(), "`#[derive(EnumSetType)]` can only be used on fieldless enums.")
        }
    }
    /// Validate the enumset type.
    fn validate(&self) -> Result<()> {
        // Check if all bits of the bitset can fit in the serialization representation.
        if let Some(explicit_serde_repr) = &self.explicit_serde_repr {
            let is_overflowed = match explicit_serde_repr.to_string().as_str() {
                "u8" => self.max_discrim >= 8,
                "u16" => self.max_discrim >= 16,
                "u32" => self.max_discrim >= 32,
                "u64" => self.max_discrim >= 64,
                "u128" => self.max_discrim >= 128,
                _ => error(
                    Span::call_site(),
                    "Only `u8`, `u16`, `u32`, `u64` and `u128` are supported for serde_repr."
                )?,
            };
            if is_overflowed {
                error(Span::call_site(), "serialize_repr cannot be smaller than bitset.")?;
            }
        }
        Ok(())
    }

    /// Computes the underlying type used to store the enumset.
    fn enumset_repr(&self) -> SynTokenStream {
        if self.max_discrim <= 7 {
            quote! { u8 }
        } else if self.max_discrim <= 15 {
            quote! { u16 }
        } else if self.max_discrim <= 31 {
            quote! { u32 }
        } else if self.max_discrim <= 63 {
            quote! { u64 }
        } else if self.max_discrim <= 127 {
            quote! { u128 }
        } else {
            panic!("max_variant > 127?")
        }
    }
    /// Computes the underlying type used to serialize the enumset.
    #[cfg(feature = "serde")]
    fn serde_repr(&self) -> SynTokenStream {
        if let Some(serde_repr) = &self.explicit_serde_repr {
            quote! { #serde_repr }
        } else {
            self.enumset_repr()
        }
    }

    /// Returns a bitmask of all variants in the set.
    fn all_variants(&self) -> u128 {
        let mut accum = 0u128;
        for variant in &self.variants {
            assert!(variant.variant_repr <= 127);
            accum |= 1u128 << variant.variant_repr as u128;
        }
        accum
    }
}

/// Generates the actual `EnumSetType` impl.
fn enum_set_type_impl(info: EnumSetInfo) -> SynTokenStream {
    let name = &info.name;
    let enumset = match &info.crate_name {
        Some(crate_name) => quote!(::#crate_name),
        None => quote!(::enumset),
    };
    let typed_enumset = quote!(#enumset::EnumSet<#name>);
    let core = quote!(#enumset::__internal::core_export);

    let repr = info.enumset_repr();
    let all_variants = Literal::u128_unsuffixed(info.all_variants());

    let ops = if info.no_ops {
        quote! {}
    } else {
        quote! {
            impl <O : Into<#typed_enumset>> #core::ops::Sub<O> for #name {
                type Output = #typed_enumset;
                fn sub(self, other: O) -> Self::Output {
                    #enumset::EnumSet::only(self) - other.into()
                }
            }
            impl <O : Into<#typed_enumset>> #core::ops::BitAnd<O> for #name {
                type Output = #typed_enumset;
                fn bitand(self, other: O) -> Self::Output {
                    #enumset::EnumSet::only(self) & other.into()
                }
            }
            impl <O : Into<#typed_enumset>> #core::ops::BitOr<O> for #name {
                type Output = #typed_enumset;
                fn bitor(self, other: O) -> Self::Output {
                    #enumset::EnumSet::only(self) | other.into()
                }
            }
            impl <O : Into<#typed_enumset>> #core::ops::BitXor<O> for #name {
                type Output = #typed_enumset;
                fn bitxor(self, other: O) -> Self::Output {
                    #enumset::EnumSet::only(self) ^ other.into()
                }
            }
            impl #core::ops::Not for #name {
                type Output = #typed_enumset;
                fn not(self) -> Self::Output {
                    !#enumset::EnumSet::only(self)
                }
            }
            impl #core::cmp::PartialEq<#typed_enumset> for #name {
                fn eq(&self, other: &#typed_enumset) -> bool {
                    #enumset::EnumSet::only(*self) == *other
                }
            }
        }
    };


    #[cfg(feature = "serde")]
    let serde = quote!(#enumset::__internal::serde);

    #[cfg(feature = "serde")]
    let serde_ops = if info.serialize_as_list {
        let expecting_str = format!("a list of {}", name);
        quote! {
            fn serialize<S: #serde::Serializer>(
                set: #enumset::EnumSet<#name>, ser: S,
            ) -> #core::result::Result<S::Ok, S::Error> {
                use #serde::ser::SerializeSeq;
                let mut seq = ser.serialize_seq(#core::prelude::v1::Some(set.len()))?;
                for bit in set {
                    seq.serialize_element(&bit)?;
                }
                seq.end()
            }
            fn deserialize<'de, D: #serde::Deserializer<'de>>(
                de: D,
            ) -> #core::result::Result<#enumset::EnumSet<#name>, D::Error> {
                struct Visitor;
                impl <'de> #serde::de::Visitor<'de> for Visitor {
                    type Value = #enumset::EnumSet<#name>;
                    fn expecting(
                        &self, formatter: &mut #core::fmt::Formatter,
                    ) -> #core::fmt::Result {
                        write!(formatter, #expecting_str)
                    }
                    fn visit_seq<A>(
                        mut self, mut seq: A,
                    ) -> #core::result::Result<Self::Value, A::Error> where
                        A: #serde::de::SeqAccess<'de>
                    {
                        let mut accum = #enumset::EnumSet::<#name>::new();
                        while let #core::prelude::v1::Some(val) = seq.next_element::<#name>()? {
                            accum |= val;
                        }
                        #core::prelude::v1::Ok(accum)
                    }
                }
                de.deserialize_seq(Visitor)
            }
        }
    } else {
        let serialize_repr = info.serde_repr();
        let check_unknown = if info.serialize_deny_unknown {
            quote! {
                if value & !#all_variants != 0 {
                    use #serde::de::Error;
                    return #core::prelude::v1::Err(
                        D::Error::custom("enumset contains unknown bits")
                    )
                }
            }
        } else {
            quote! { }
        };
        quote! {
            fn serialize<S: #serde::Serializer>(
                set: #enumset::EnumSet<#name>, ser: S,
            ) -> #core::result::Result<S::Ok, S::Error> {
                #serde::Serialize::serialize(&(set.__priv_repr as #serialize_repr), ser)
            }
            fn deserialize<'de, D: #serde::Deserializer<'de>>(
                de: D,
            ) -> #core::result::Result<#enumset::EnumSet<#name>, D::Error> {
                let value = <#serialize_repr as #serde::Deserialize>::deserialize(de)?;
                #check_unknown
                #core::prelude::v1::Ok(#enumset::EnumSet {
                    __priv_repr: (value & #all_variants) as #repr,
                })
            }
        }
    };

    #[cfg(not(feature = "serde"))]
    let serde_ops = quote! { };

    let is_uninhabited = info.variants.is_empty();
    let is_zst = info.variants.len() == 1;
    let into_impl = if is_uninhabited {
        quote! {
            fn enum_into_u32(self) -> u32 {
                panic!(concat!(stringify!(#name), " is uninhabited."))
            }
            unsafe fn enum_from_u32(val: u32) -> Self {
                panic!(concat!(stringify!(#name), " is uninhabited."))
            }
        }
    } else if is_zst {
        let variant = &info.variants[0].name;
        quote! {
            fn enum_into_u32(self) -> u32 {
                self as u32
            }
            unsafe fn enum_from_u32(val: u32) -> Self {
                #name::#variant
            }
        }
    } else {
        let variant_name: Vec<_> = info.variants.iter().map(|x| &x.name).collect();
        let variant_value: Vec<_> = info.variants.iter().map(|x| x.variant_repr).collect();

        let const_field: Vec<_> = ["IS_U8", "IS_U16", "IS_U32", "IS_U64", "IS_U128"]
            .iter().map(|x| Ident::new(x, Span::call_site())).collect();
        let int_type: Vec<_> = ["u8", "u16", "u32", "u64", "u128"]
            .iter().map(|x| Ident::new(x, Span::call_site())).collect();

        quote! {
            fn enum_into_u32(self) -> u32 {
                self as u32
            }
            unsafe fn enum_from_u32(val: u32) -> Self {
                // We put these in const fields so the branches they guard aren't generated even
                // on -O0
                #(const #const_field: bool =
                    #core::mem::size_of::<#name>() == #core::mem::size_of::<#int_type>();)*
                match val {
                    // Every valid variant value has an explicit branch. If they get optimized out,
                    // great. If the representation has changed somehow, and they don't, oh well,
                    // there's still no UB.
                    #(#variant_value => #name::#variant_name,)*
                    // Helps hint to the LLVM that this is a transmute. Note that this branch is
                    // still unreachable.
                    #(x if #const_field => {
                        let x = x as #int_type;
                        *(&x as *const _ as *const #name)
                    })*
                    // Default case. Sometimes causes LLVM to generate a table instead of a simple
                    // transmute, but, oh well.
                    _ => #core::hint::unreachable_unchecked(),
                }
            }
        }
    };

    let eq_impl = if is_uninhabited {
        quote!(panic!(concat!(stringify!(#name), " is uninhabited.")))
    } else {
        quote!((*self as u32) == (*other as u32))
    };

    // used in the enum_set! macro `const fn`s.
    let self_as_repr_mask = if is_uninhabited {
        quote! { 0 } // impossible anyway
    } else {
        quote! { 1 << self as #repr }
    };

    quote! {
        unsafe impl #enumset::__internal::EnumSetTypePrivate for #name {
            type Repr = #repr;
            const ALL_BITS: Self::Repr = #all_variants;
            #into_impl
            #serde_ops
        }

        unsafe impl #enumset::EnumSetType for #name { }

        impl #core::cmp::PartialEq for #name {
            fn eq(&self, other: &Self) -> bool {
                #eq_impl
            }
        }
        impl #core::cmp::Eq for #name { }
        impl #core::clone::Clone for #name {
            fn clone(&self) -> Self {
                *self
            }
        }
        impl #core::marker::Copy for #name { }

        impl #name {
            /// Creates a new enumset with only this variant.
            #[deprecated(note = "This method is an internal implementation detail generated by \
                                 the `enumset` crate's procedural macro. It should not be used \
                                 directly. Use `EnumSet::only` instead.")]
            #[doc(hidden)]
            pub const fn __impl_enumset_internal__const_only(self) -> #enumset::EnumSet<#name> {
                #enumset::EnumSet { __priv_repr: #self_as_repr_mask }
            }

            /// Creates a new enumset with this variant added.
            #[deprecated(note = "This method is an internal implementation detail generated by \
                                 the `enumset` crate's procedural macro. It should not be used \
                                 directly. Use the `|` operator instead.")]
            #[doc(hidden)]
            pub const fn __impl_enumset_internal__const_merge(
                self, chain: #enumset::EnumSet<#name>,
            ) -> #enumset::EnumSet<#name> {
                #enumset::EnumSet { __priv_repr: chain.__priv_repr | #self_as_repr_mask }
            }
        }

        #ops
    }
}

/// A wrapper that parses the input enum.
#[proc_macro_derive(EnumSetType, attributes(enumset))]
pub fn derive_enum_set_type(input: TokenStream) -> TokenStream {
    let input: DeriveInput = parse_macro_input!(input);
    let attrs: EnumsetAttrs = match EnumsetAttrs::from_derive_input(&input) {
        Ok(attrs) => attrs,
        Err(e) => return e.write_errors().into(),
    };
    match derive_enum_set_type_0(input, attrs) {
        Ok(v) => v,
        Err(e) => e.to_compile_error().into(),
    }
}
fn derive_enum_set_type_0(input: DeriveInput, attrs: EnumsetAttrs) -> Result<TokenStream> {
    if !input.generics.params.is_empty() {
        error(
            input.generics.span(),
            "`#[derive(EnumSetType)]` cannot be used on enums with type parameters.",
        )
    } else if let Data::Enum(data) = &input.data {
        let mut info = EnumSetInfo::new(&input, attrs);
        for attr in &input.attrs {
            if attr.path.is_ident(&Ident::new("repr", Span::call_site())) {
                let meta: Ident = attr.parse_args()?;
                info.push_explicit_repr(attr.span(), meta.to_string().as_str())?;
            }
        }
        for variant in &data.variants {
            info.push_variant(variant)?;
        }
        info.validate()?;
        Ok(enum_set_type_impl(info).into())
    } else {
        error(input.span(), "`#[derive(EnumSetType)]` may only be used on enums")
    }
}
