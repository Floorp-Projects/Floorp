use proc_macro::{Ident, TokenStream, TokenTree};

use crate::to_tokens;

pub(crate) fn build(
    mod_name: Ident,
    items: impl to_tokens::ToTokenStream,
    ty: TokenTree,
    format_string: &str,
) -> TokenStream {
    let ty_s = &*ty.to_string();

    let visitor = quote! {
        struct Visitor;
        struct OptionVisitor;

        impl<'a> ::serde::de::Visitor<'a> for Visitor {
            type Value = __TimeSerdeType;

            fn expecting(&self, f: &mut ::std::fmt::Formatter<'_>) -> ::std::fmt::Result {
                write!(
                    f,
                    concat!(
                        "a(n) `",
                        #(ty_s),
                        "` in the format \"",
                        #(format_string),
                        "\"",
                    )
                )
            }

            fn visit_str<E: ::serde::de::Error>(
                self,
                value: &str
            ) -> Result<__TimeSerdeType, E> {
                __TimeSerdeType::parse(value, &DESCRIPTION).map_err(E::custom)
            }
        }

        impl<'a> ::serde::de::Visitor<'a> for OptionVisitor {
            type Value = Option<__TimeSerdeType>;

            fn expecting(&self, f: &mut ::std::fmt::Formatter<'_>) -> ::std::fmt::Result {
                write!(
                    f,
                    concat!(
                        "an `Option<",
                        #(ty_s),
                        ">` in the format \"",
                        #(format_string),
                        "\"",
                    )
                )
            }

            fn visit_some<D: ::serde::de::Deserializer<'a>>(
                self,
                deserializer: D
            ) -> Result<Option<__TimeSerdeType>, D::Error> {
                deserializer
                    .deserialize_any(Visitor)
                    .map(Some)
            }

            fn visit_none<E: ::serde::de::Error>(
                self
            ) -> Result<Option<__TimeSerdeType>, E> {
                Ok(None)
            }
        }

    };

    let primary_fns = quote! {
        pub fn serialize<S: ::serde::Serializer>(
            datetime: &__TimeSerdeType,
            serializer: S,
        ) -> Result<S::Ok, S::Error> {
            use ::serde::Serialize;
            datetime
                .format(&DESCRIPTION)
                .map_err(::time::error::Format::into_invalid_serde_value::<S>)?
                .serialize(serializer)
        }

        pub fn deserialize<'a, D: ::serde::Deserializer<'a>>(
            deserializer: D
        ) -> Result<__TimeSerdeType, D::Error> {
            use ::serde::Deserialize;
            deserializer.deserialize_any(Visitor)
        }
    };

    let options_fns = quote! {
        pub fn serialize<S: ::serde::Serializer>(
            option: &Option<__TimeSerdeType>,
            serializer: S,
        ) -> Result<S::Ok, S::Error> {
            use ::serde::Serialize;
            option.map(|datetime| datetime.format(&DESCRIPTION))
                .transpose()
                .map_err(::time::error::Format::into_invalid_serde_value::<S>)?
                .serialize(serializer)
        }

        pub fn deserialize<'a, D: ::serde::Deserializer<'a>>(
            deserializer: D
        ) -> Result<Option<__TimeSerdeType>, D::Error> {
            use ::serde::Deserialize;
            deserializer.deserialize_option(OptionVisitor)
        }
    };

    quote! {
        mod #(mod_name) {
            use ::time::#(ty) as __TimeSerdeType;

            const DESCRIPTION: &[::time::format_description::FormatItem<'_>] = &[#S(items)];

            #S(visitor)
            #S(primary_fns)

            pub(super) mod option {
                use super::{DESCRIPTION, OptionVisitor, Visitor, __TimeSerdeType};

                #S(options_fns)
            }
        }
    }
}
