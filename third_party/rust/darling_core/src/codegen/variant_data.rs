use proc_macro2::TokenStream;

use ast::Fields;
use ast::Style;
use codegen::Field;

pub struct FieldsGen<'a> {
    fields: &'a Fields<Field<'a>>,
    allow_unknown_fields: bool,
}

impl<'a> FieldsGen<'a> {
    pub fn new(fields: &'a Fields<Field<'a>>, allow_unknown_fields: bool) -> Self {
        Self {
            fields,
            allow_unknown_fields,
        }
    }

    /// Create declarations for all the fields in the struct.
    pub(in codegen) fn declarations(&self) -> TokenStream {
        match *self.fields {
            Fields {
                style: Style::Struct,
                ref fields,
            } => {
                let vdr = fields.iter().map(Field::as_declaration);
                quote!(#(#vdr)*)
            }
            _ => panic!("FieldsGen doesn't support tuples yet"),
        }
    }

    /// Generate the loop which walks meta items looking for property matches.
    pub(in codegen) fn core_loop(&self) -> TokenStream {
        let arms = self.fields.as_ref().map(Field::as_match);

        // If we're allowing unknown fields, then handling one is a no-op.
        // Otherwise, we're going to push a new spanned error pointing at the field.
        let handle_unknown = if self.allow_unknown_fields {
            quote!()
        } else {
            // We can't call `unknown_field_with_alts` with an empty slice, or else it fails to
            // infer the type of the slice item.
            let err_fn = if arms.is_empty() {
                quote!(unknown_field(__other))
            } else {
                let names = self.fields.as_ref().map(Field::as_name);
                let names = names.iter();
                quote!(unknown_field_with_alts(__other, &[#(#names),*]))
            };

            quote! {
                __errors.push(::darling::Error::#err_fn.with_span(__inner));
            }
        };
        let arms = arms.iter();

        quote!(
            for __item in __items {
                if let ::syn::NestedMeta::Meta(ref __inner) = *__item {
                    let __name = __inner.path().segments.iter().map(|s| s.ident.to_string()).collect::<Vec<String>>().join("::");
                    match __name.as_str() {
                        #(#arms)*
                        __other => { #handle_unknown }
                    }
                }
            }
        )
    }

    pub fn require_fields(&self) -> TokenStream {
        match *self.fields {
            Fields {
                style: Style::Struct,
                ref fields,
            } => {
                let checks = fields.iter().map(Field::as_presence_check);
                quote!(#(#checks)*)
            }
            _ => panic!("FieldsGen doesn't support tuples for requirement checks"),
        }
    }

    pub(in codegen) fn initializers(&self) -> TokenStream {
        let inits = self.fields.as_ref().map(Field::as_initializer);
        let inits = inits.iter();

        quote!(#(#inits),*)
    }
}
