use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span};
use syntax::ptr::P;

use ident::ToIdent;
use invoke::{Invoke, Identity};
use struct_field::StructFieldBuilder;
use ty::TyBuilder;

//////////////////////////////////////////////////////////////////////////////

pub struct VariantDataBuilder<F=Identity> {
    callback: F,
    span: Span,
}

impl VariantDataBuilder {
    pub fn new() -> Self {
        VariantDataBuilder::with_callback(Identity)
    }
}

impl<F> VariantDataBuilder<F>
    where F: Invoke<ast::VariantData>
{
    pub fn with_callback(callback: F) -> Self {
        VariantDataBuilder {
            callback: callback,
            span: DUMMY_SP,
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn tuple(self) -> StructFieldBuilder<VariantDataTupleBuilder<F>> {
        // tuple variants with no arguments are illegal, so force the user to define at least one
        // field.
        let builder = VariantDataTupleBuilder {
            callback: self.callback,
            span: self.span,
            fields: Vec::new(),
        };

        builder.field()
    }

    pub fn struct_(self) -> VariantDataStructBuilder<F> {
        VariantDataStructBuilder {
            callback: self.callback,
            span: self.span,
            fields: Vec::new(),
        }
    }

    pub fn unit(self) -> F::Result {
        self.callback.invoke(ast::VariantData::Unit(ast::DUMMY_NODE_ID))
    }
}

pub struct VariantDataTupleBuilder<F> {
    callback: F,
    span: Span,
    fields: Vec<ast::StructField>,
}

impl<F> VariantDataTupleBuilder<F>
    where F: Invoke<ast::VariantData>
{
    pub fn with_fields<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=ast::StructField>,
    {
        self.fields.extend(iter);
        self
    }

    pub fn with_field(mut self, field: ast::StructField) -> Self {
        self.fields.push(field);
        self
    }

    pub fn field(self) -> StructFieldBuilder<Self> {
        let span = self.span;
        StructFieldBuilder::unnamed_with_callback(self).span(span)
    }

    pub fn with_ty(self, ty: P<ast::Ty>) -> Self {
        self.field().build_ty(ty)
    }

    pub fn ty(self) -> TyBuilder<Self> {
        let span = self.span;
        TyBuilder::with_callback(self).span(span)
    }

    pub fn build(self) -> F::Result {
        self.callback.invoke(ast::VariantData::Tuple(self.fields, ast::DUMMY_NODE_ID))
    }
}

impl<F> Invoke<P<ast::Ty>> for VariantDataTupleBuilder<F>
    where F: Invoke<ast::VariantData>,
{
    type Result = Self;

    fn invoke(self, ty: P<ast::Ty>) -> Self {
        self.with_ty(ty)
    }
}

impl<F> Invoke<ast::StructField> for VariantDataTupleBuilder<F>
    where F: Invoke<ast::VariantData>,
{
    type Result = Self;

    fn invoke(self, field: ast::StructField) -> Self {
        self.with_field(field)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct VariantDataStructBuilder<F> {
    callback: F,
    span: Span,
    fields: Vec<ast::StructField>,
}

impl<F> VariantDataStructBuilder<F>
    where F: Invoke<ast::VariantData>,
{
    pub fn with_fields<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=ast::StructField>,
    {
        self.fields.extend(iter);
        self
    }

    pub fn with_field(mut self, field: ast::StructField) -> Self {
        self.fields.push(field);
        self
    }

    pub fn field<T>(self, id: T) -> StructFieldBuilder<Self>
        where T: ToIdent,
    {
        let span = self.span;
        StructFieldBuilder::named_with_callback(id, self).span(span)
    }

    pub fn build(self) -> F::Result {
        self.callback.invoke(ast::VariantData::Struct(self.fields, ast::DUMMY_NODE_ID))
    }
}

impl<F> Invoke<ast::StructField> for VariantDataStructBuilder<F>
    where F: Invoke<ast::VariantData>,
{
    type Result = Self;

    fn invoke(self, field: ast::StructField) -> Self {
        self.with_field(field)
    }
}
