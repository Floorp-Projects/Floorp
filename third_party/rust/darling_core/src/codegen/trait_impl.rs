use proc_macro2::TokenStream;
use syn::{Generics, Ident, Path, WherePredicate};

use ast::{Data, Fields};
use codegen::error::{ErrorCheck, ErrorDeclaration};
use codegen::field;
use codegen::{DefaultExpression, Field, FieldsGen, Variant};
use usage::{CollectTypeParams, IdentSet, Purpose};

#[derive(Debug)]
pub struct TraitImpl<'a> {
    pub ident: &'a Ident,
    pub generics: &'a Generics,
    pub data: Data<Variant<'a>, Field<'a>>,
    pub default: Option<DefaultExpression<'a>>,
    pub map: Option<&'a Path>,
    pub bound: Option<&'a [WherePredicate]>,
    pub allow_unknown_fields: bool,
}

impl<'a> TraitImpl<'a> {
    /// Get all declared type parameters.
    pub fn declared_type_params(&self) -> IdentSet {
        self.generics
            .type_params()
            .map(|tp| tp.ident.clone())
            .collect()
    }

    /// Get the type parameters which are used by non-skipped, non-magic fields.
    /// These type parameters will have a `FromMeta` bound applied to them in emitted
    /// code.
    pub fn used_type_params(&self) -> IdentSet {
        self.type_params_matching(|f| !f.skip, |v| !v.skip)
    }

    fn type_params_matching<'b, F, V>(&'b self, field_filter: F, variant_filter: V) -> IdentSet
    where
        F: Fn(&&Field) -> bool,
        V: Fn(&&Variant) -> bool,
    {
        let declared = self.declared_type_params();
        match self.data {
            Data::Struct(ref v) => self.type_params_in_fields(v, &field_filter, &declared),
            Data::Enum(ref v) => {
                v.iter()
                    .filter(variant_filter)
                    .fold(Default::default(), |mut state, variant| {
                        state.extend(self.type_params_in_fields(
                            &variant.data,
                            &field_filter,
                            &declared,
                        ));
                        state
                    })
            }
        }
    }

    /// Get the type parameters of all fields in a set matching some filter
    fn type_params_in_fields<'b, F>(
        &'b self,
        fields: &'b Fields<Field<'a>>,
        field_filter: F,
        declared: &IdentSet,
    ) -> IdentSet
    where
        F: Fn(&&'b Field) -> bool,
    {
        fields
            .iter()
            .filter(field_filter)
            .collect_type_params_cloned(&Purpose::BoundImpl.into(), declared)
    }
}

impl<'a> TraitImpl<'a> {
    /// Gets the `let` declaration for errors accumulated during parsing.
    pub fn declare_errors(&self) -> ErrorDeclaration {
        ErrorDeclaration::default()
    }

    /// Gets the check which performs an early return if errors occurred during parsing.
    pub fn check_errors(&self) -> ErrorCheck {
        ErrorCheck::default()
    }

    /// Generate local variable declarations for all fields.
    pub(in codegen) fn local_declarations(&self) -> TokenStream {
        if let Data::Struct(ref vd) = self.data {
            let vdr = vd.as_ref().map(Field::as_declaration);
            let decls = vdr.fields.as_slice();
            quote!(#(#decls)*)
        } else {
            quote!()
        }
    }

    /// Generate immutable variable declarations for all fields.
    pub(in codegen) fn immutable_declarations(&self) -> TokenStream {
        if let Data::Struct(ref vd) = self.data {
            let vdr = vd.as_ref().map(|f| field::Declaration::new(f, false));
            let decls = vdr.fields.as_slice();
            quote!(#(#decls)*)
        } else {
            quote!()
        }
    }

    pub(in codegen) fn map_fn(&self) -> Option<TokenStream> {
        self.map.as_ref().map(|path| quote!(.map(#path)))
    }

    /// Generate local variable declaration and initialization for instance from which missing fields will be taken.
    pub(in codegen) fn fallback_decl(&self) -> TokenStream {
        let default = self.default.as_ref().map(DefaultExpression::as_declaration);
        quote!(#default)
    }

    pub fn require_fields(&self) -> TokenStream {
        if let Data::Struct(ref vd) = self.data {
            let check_nones = vd.as_ref().map(Field::as_presence_check);
            let checks = check_nones.fields.as_slice();
            quote!(#(#checks)*)
        } else {
            quote!()
        }
    }

    pub(in codegen) fn initializers(&self) -> TokenStream {
        self.make_field_ctx().initializers()
    }

    /// Generate the loop which walks meta items looking for property matches.
    pub(in codegen) fn core_loop(&self) -> TokenStream {
        self.make_field_ctx().core_loop()
    }

    fn make_field_ctx(&'a self) -> FieldsGen<'a> {
        match self.data {
            Data::Enum(_) => panic!("Core loop on enums isn't supported"),
            Data::Struct(ref data) => FieldsGen::new(data, self.allow_unknown_fields),
        }
    }
}
