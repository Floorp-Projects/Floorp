use proc_macro2::TokenStream;
use quote::{TokenStreamExt, ToTokens};
use syn::Ident;

use ast::Fields;
use codegen::error::{ErrorCheck, ErrorDeclaration};
use codegen::{Field, FieldsGen};
use usage::{self, IdentRefSet, IdentSet, UsesTypeParams};

/// An enum variant.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Variant<'a> {
    /// The name which will appear in code passed to the `FromMeta` input.
    pub name_in_attr: String,

    /// The name of the variant which will be returned for a given `name_in_attr`.
    pub variant_ident: &'a Ident,

    /// The name of the parent enum type.
    pub ty_ident: &'a Ident,

    pub data: Fields<Field<'a>>,

    /// Whether or not the variant should be skipped in the generated code.
    pub skip: bool,
}

impl<'a> Variant<'a> {
    pub fn as_unit_match_arm(&'a self) -> UnitMatchArm<'a> {
        UnitMatchArm(self)
    }

    pub fn as_data_match_arm(&'a self) -> DataMatchArm<'a> {
        DataMatchArm(self)
    }
}

impl<'a> UsesTypeParams for Variant<'a> {
    fn uses_type_params<'b>(
        &self,
        options: &usage::Options,
        type_set: &'b IdentSet,
    ) -> IdentRefSet<'b> {
        self.data.uses_type_params(options, type_set)
    }
}

pub struct UnitMatchArm<'a>(&'a Variant<'a>);

impl<'a> ToTokens for UnitMatchArm<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let val: &Variant<'a> = self.0;

        if val.skip {
            return;
        }

        let name_in_attr = &val.name_in_attr;

        if val.data.is_unit() {
            let variant_ident = val.variant_ident;
            let ty_ident = val.ty_ident;

            tokens.append_all(quote!(
                #name_in_attr => ::darling::export::Ok(#ty_ident::#variant_ident),
            ));
        } else {
            tokens.append_all(quote!(
                #name_in_attr => ::darling::export::Err(::darling::Error::unsupported_format("literal")),
            ));
        }
    }
}

pub struct DataMatchArm<'a>(&'a Variant<'a>);

impl<'a> ToTokens for DataMatchArm<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let val: &Variant<'a> = self.0;

        if val.skip {
            return;
        }

        let name_in_attr = &val.name_in_attr;
        let variant_ident = val.variant_ident;
        let ty_ident = val.ty_ident;

        if val.data.is_unit() {
            tokens.append_all(quote!(
                #name_in_attr => ::darling::export::Err(::darling::Error::unsupported_format("list")),
            ));

            return;
        }

        let vdg = FieldsGen(&val.data);

        if val.data.is_struct() {
            let declare_errors = ErrorDeclaration::new();
            let check_errors = ErrorCheck::with_location(&name_in_attr);
            let require_fields = vdg.require_fields();
            let decls = vdg.declarations();
            let core_loop = vdg.core_loop();
            let inits = vdg.initializers();

            tokens.append_all(quote!(
                #name_in_attr => {
                    if let ::syn::Meta::List(ref __data) = *__nested {
                        let __items = &__data.nested;

                        #declare_errors

                        #decls

                        #core_loop

                        #require_fields

                        #check_errors

                        ::darling::export::Ok(#ty_ident::#variant_ident {
                            #inits
                        })
                    } else {
                        ::darling::export::Err(::darling::Error::unsupported_format("non-list"))
                    }
                }
            ));
        } else if val.data.is_newtype() {
            tokens.append_all(quote!(
                #name_in_attr => {
                    ::darling::export::Ok(
                        #ty_ident::#variant_ident(
                            ::darling::FromMeta::from_meta(__nested)
                                .map_err(|e| e.at(#name_in_attr))?)
                    )
                }
            ));
        } else {
            panic!("Match arms aren't supported for tuple variants yet");
        }
    }
}
