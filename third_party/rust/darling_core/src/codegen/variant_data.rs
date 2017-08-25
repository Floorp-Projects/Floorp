use quote::Tokens;

use ast::Style;
use codegen::field;
use codegen::Field;
use ast::VariantData;

pub struct VariantDataGen<'a>(pub &'a VariantData<Field<'a>>);

impl<'a> VariantDataGen<'a> {
    pub(in codegen) fn declarations(&self) -> Tokens {
        match *self.0 {
            VariantData { style: Style::Struct, ref fields } => {
                let vdr = fields.into_iter().map(Field::as_declaration);
                quote!(#(#vdr)*)
            }
            _ => panic!("VariantDataGen doesn't support tuples yet"),
        }
    }

    /// Generate the loop which walks meta items looking for property matches.
    pub(in codegen) fn core_loop(&self) -> Tokens {
        let arms: Vec<field::MatchArm> = self.0.as_ref().map(Field::as_match).fields;

        quote!(
            for __item in __items {
                if let ::syn::NestedMetaItem::MetaItem(ref __inner) = *__item {
                    let __name = __inner.name().to_string();
                    match __name.as_str() {
                        #(#arms)*
                        __other => { __errors.push(::darling::Error::unknown_field(__other)); }
                    }
                }
            }
        )
    }

    pub fn require_fields(&self) -> Tokens {
        match *self.0 {
            VariantData { style: Style::Struct, ref fields } => {
                let checks = fields.into_iter().map(Field::as_presence_check);
                quote!(#(#checks)*)
            }
            _ => panic!("VariantDataGen doesn't support tuples for requirement checks")
        }
    }

    pub(in codegen) fn initializers(&self) -> Tokens {
        let inits: Vec<_> = self.0.as_ref().map(Field::as_initializer).fields;

        quote!(#(#inits),*)
    }
}