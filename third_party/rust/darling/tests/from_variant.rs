#[macro_use]
extern crate darling;
extern crate syn;

#[derive(FromVariant)]
#[darling(from_ident, attributes(hello))]
#[allow(dead_code)]
pub struct Lorem {
    ident: syn::Ident,
    into: Option<bool>,
    skip: Option<bool>,
    data: darling::ast::VariantData<syn::Ty>,
}

impl From<syn::Ident> for Lorem {
    fn from(ident: syn::Ident) -> Self {
        Lorem {
            ident,
            into: Default::default(),
            skip: Default::default(),
            data: darling::ast::Style::Unit.into(),
        }
    }
}

#[test]
fn expansion() {
    
}