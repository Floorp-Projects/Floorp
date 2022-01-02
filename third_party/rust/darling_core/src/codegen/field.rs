use std::borrow::Cow;

use proc_macro2::TokenStream;
use quote::{ToTokens, TokenStreamExt};
use syn::{Ident, Path, Type};

use codegen::DefaultExpression;
use usage::{self, IdentRefSet, IdentSet, UsesTypeParams};

/// Properties needed to generate code for a field in all the contexts
/// where one may appear.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Field<'a> {
    /// The name presented to the user of the library. This will appear
    /// in error messages and will be looked when parsing names.
    pub name_in_attr: Cow<'a, String>,

    /// The name presented to the author of the library. This will appear
    /// in the setters or temporary variables which contain the values.
    pub ident: &'a Ident,

    /// The type of the field in the input.
    pub ty: &'a Type,
    pub default_expression: Option<DefaultExpression<'a>>,
    pub with_path: Cow<'a, Path>,
    pub map: Option<&'a Path>,
    pub skip: bool,
    pub multiple: bool,
}

impl<'a> Field<'a> {
    pub fn as_name(&'a self) -> &'a str {
        &self.name_in_attr
    }

    pub fn as_declaration(&'a self) -> Declaration<'a> {
        Declaration(self, !self.skip)
    }

    pub fn as_match(&'a self) -> MatchArm<'a> {
        MatchArm(self)
    }

    pub fn as_initializer(&'a self) -> Initializer<'a> {
        Initializer(self)
    }

    pub fn as_presence_check(&'a self) -> CheckMissing<'a> {
        CheckMissing(self)
    }
}

impl<'a> UsesTypeParams for Field<'a> {
    fn uses_type_params<'b>(
        &self,
        options: &usage::Options,
        type_set: &'b IdentSet,
    ) -> IdentRefSet<'b> {
        self.ty.uses_type_params(options, type_set)
    }
}

/// An individual field during variable declaration in the generated parsing method.
pub struct Declaration<'a>(&'a Field<'a>, bool);

impl<'a> Declaration<'a> {
    /// Creates a new declaration with the given field and mutability.
    pub fn new(field: &'a Field<'a>, mutable: bool) -> Self {
        Declaration(field, mutable)
    }
}

impl<'a> ToTokens for Declaration<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let field: &Field = self.0;
        let ident = field.ident;
        let ty = field.ty;

        let mutable = if self.1 { quote!(mut) } else { quote!() };

        tokens.append_all(if field.multiple {
            // This is NOT mutable, as it will be declared mutable only temporarily.
            quote!(let #mutable #ident: #ty = ::darling::export::Default::default();)
        } else {
            quote!(let #mutable #ident: (bool, ::darling::export::Option<#ty>) = (false, None);)
        });
    }
}

/// Represents an individual field in the match.
pub struct MatchArm<'a>(&'a Field<'a>);

impl<'a> ToTokens for MatchArm<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let field: &Field = self.0;
        if !field.skip {
            let name_str = &field.name_in_attr;
            let ident = field.ident;
            let with_path = &field.with_path;

            // Errors include the location of the bad input, so we compute that here.
            // Fields that take multiple values add the index of the error for convenience,
            // while single-value fields only expose the name in the input attribute.
            let location = if field.multiple {
                // we use the local variable `len` here because location is accessed via
                // a closure, and the borrow checker gets very unhappy if we try to immutably
                // borrow `#ident` in that closure when it was declared `mut` outside.
                quote!(&format!("{}[{}]", #name_str, __len))
            } else {
                quote!(#name_str)
            };

            // Add the span immediately on extraction failure, so that it's as specific as possible.
            // The behavior of `with_span` makes this safe to do; if the child applied an
            // even-more-specific span, our attempt here will not overwrite that and will only cost
            // us one `if` check.
            let mut extractor =
                quote!(#with_path(__inner).map_err(|e| e.with_span(&__inner).at(#location)));
            if let Some(ref map) = field.map {
                extractor = quote!(#extractor.map(#map))
            }

            tokens.append_all(if field.multiple {
                quote!(
                    #name_str => {
                        // Store the index of the name we're assessing in case we need
                        // it for error reporting.
                        let __len = #ident.len();
                        match #extractor {
                            ::darling::export::Ok(__val) => {
                                #ident.push(__val)
                            }
                            ::darling::export::Err(__err) => {
                                __errors.push(__err)
                            }
                        }
                    }
                )
            } else {
                quote!(
                    #name_str => {
                        if !#ident.0 {
                            match #extractor {
                                ::darling::export::Ok(__val) => {
                                    #ident = (true, ::darling::export::Some(__val));
                                }
                                ::darling::export::Err(__err) => {
                                    #ident = (true, None);
                                    __errors.push(__err);
                                }
                            }
                        } else {
                            __errors.push(::darling::Error::duplicate_field(#name_str).with_span(&__inner));
                        }
                    }
                )
            });
        }
    }
}

/// Wrapper to generate initialization code for a field.
pub struct Initializer<'a>(&'a Field<'a>);

impl<'a> ToTokens for Initializer<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let field: &Field = self.0;
        let ident = field.ident;
        tokens.append_all(if field.multiple {
            if let Some(ref expr) = field.default_expression {
                quote!(#ident: if !#ident.is_empty() {
                    #ident
                } else {
                    #expr
                })
            } else {
                quote!(#ident: #ident)
            }
        } else if let Some(ref expr) = field.default_expression {
            quote!(#ident: match #ident.1 {
                ::darling::export::Some(__val) => __val,
                ::darling::export::None => #expr,
            })
        } else {
            quote!(#ident: #ident.1.expect("Uninitialized fields without defaults were already checked"))
        });
    }
}

/// Creates an error if a field has no value and no default.
pub struct CheckMissing<'a>(&'a Field<'a>);

impl<'a> ToTokens for CheckMissing<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        if !self.0.multiple && self.0.default_expression.is_none() {
            let ident = self.0.ident;
            let name_in_attr = &self.0.name_in_attr;

            tokens.append_all(quote! {
                if !#ident.0 {
                    __errors.push(::darling::Error::missing_field(#name_in_attr));
                }
            })
        }
    }
}
