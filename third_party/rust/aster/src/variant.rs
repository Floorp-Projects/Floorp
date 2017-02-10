use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span, respan};

use attr::AttrBuilder;
use ident::ToIdent;
use invoke::{Invoke, Identity};
use struct_field::StructFieldBuilder;
use variant_data::{
    VariantDataBuilder,
    VariantDataStructBuilder,
    VariantDataTupleBuilder,
};

//////////////////////////////////////////////////////////////////////////////

pub struct VariantBuilder<F=Identity> {
    callback: F,
    span: Span,
    attrs: Vec<ast::Attribute>,
    id: ast::Ident,
}

impl VariantBuilder {
    pub fn new<T>(id: T) -> Self
        where T: ToIdent,
    {
        VariantBuilder::with_callback(id, Identity)
    }
}

impl<F> VariantBuilder<F>
    where F: Invoke<ast::Variant>,
{
    pub fn with_callback<T>(id: T, callback: F) -> Self
        where T: ToIdent,
    {
        VariantBuilder {
            callback: callback,
            span: DUMMY_SP,
            attrs: vec![],
            id: id.to_ident(),
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn attr(self) -> AttrBuilder<Self> {
        let span = self.span;
        AttrBuilder::with_callback(self).span(span)
    }

    pub fn unit(self) -> F::Result {
        VariantDataBuilder::with_callback(self).unit()
    }

    pub fn tuple(self) -> StructFieldBuilder<VariantDataTupleBuilder<Self>> {
        VariantDataBuilder::with_callback(self).tuple()
    }

    pub fn struct_(self) -> VariantDataStructBuilder<Self> {
        VariantDataBuilder::with_callback(self).struct_()
    }

    pub fn build_variant_data(self, data: ast::VariantData) -> F::Result {
        let variant_ = ast::Variant_ {
            name: self.id,
            attrs: self.attrs,
            data: data,
            disr_expr: None,
        };
        let variant = respan(self.span, variant_);
        self.callback.invoke(variant)
    }

    pub fn build_variant_(self, variant: ast::Variant_) -> F::Result {
        let variant = respan(self.span, variant);
        self.build(variant)
    }

    pub fn build(self, variant: ast::Variant) -> F::Result {
        self.callback.invoke(variant)
    }
}

impl<F> Invoke<ast::Attribute> for VariantBuilder<F>
    where F: Invoke<ast::Variant>,
{
    type Result = Self;

    fn invoke(mut self, attr: ast::Attribute) -> Self {
        self.attrs.push(attr);
        self
    }
}

impl<F> Invoke<ast::VariantData> for VariantBuilder<F>
    where F: Invoke<ast::Variant>,
{
    type Result = F::Result;

    fn invoke(self, data: ast::VariantData) -> F::Result {
        self.build_variant_data(data)
    }
}
