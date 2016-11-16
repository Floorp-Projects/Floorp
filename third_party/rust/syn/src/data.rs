use super::*;

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct Variant {
    pub ident: Ident,
    pub attrs: Vec<Attribute>,
    pub data: VariantData,
    /// Explicit discriminant, e.g. `Foo = 1`
    pub discriminant: Option<ConstExpr>,
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub enum VariantData {
    Struct(Vec<Field>),
    Tuple(Vec<Field>),
    Unit,
}

impl VariantData {
    pub fn fields(&self) -> &[Field] {
        match *self {
            VariantData::Struct(ref fields) |
            VariantData::Tuple(ref fields) => fields,
            VariantData::Unit => &[],
        }
    }

    pub fn fields_mut(&mut self) -> &mut [Field] {
        match *self {
            VariantData::Struct(ref mut fields) |
            VariantData::Tuple(ref mut fields) => fields,
            VariantData::Unit => &mut [],
        }
    }
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct Field {
    pub ident: Option<Ident>,
    pub vis: Visibility,
    pub attrs: Vec<Attribute>,
    pub ty: Ty,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum Visibility {
    Public,
    Inherited,
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use attr::parsing::outer_attr;
    use constant::parsing::const_expr;
    use ident::parsing::ident;
    use ty::parsing::ty;

    named!(pub struct_body -> VariantData, alt!(
        struct_like_body => { VariantData::Struct }
        |
        terminated!(tuple_like_body, punct!(";")) => { VariantData::Tuple }
        |
        punct!(";") => { |_| VariantData::Unit }
    ));

    named!(pub enum_body -> Vec<Variant>, do_parse!(
        punct!("{") >>
        variants: terminated_list!(punct!(","), variant) >>
        punct!("}") >>
        (variants)
    ));

    named!(variant -> Variant, do_parse!(
        attrs: many0!(outer_attr) >>
        id: ident >>
        data: alt!(
            struct_like_body => { VariantData::Struct }
            |
            tuple_like_body => { VariantData::Tuple }
            |
            epsilon!() => { |_| VariantData::Unit }
        ) >>
        disr: option!(preceded!(punct!("="), const_expr)) >>
        (Variant {
            ident: id,
            attrs: attrs,
            data: data,
            discriminant: disr,
        })
    ));

    named!(pub struct_like_body -> Vec<Field>, do_parse!(
        punct!("{") >>
        fields: terminated_list!(punct!(","), struct_field) >>
        punct!("}") >>
        (fields)
    ));

    named!(tuple_like_body -> Vec<Field>, do_parse!(
        punct!("(") >>
        fields: terminated_list!(punct!(","), tuple_field) >>
        punct!(")") >>
        (fields)
    ));

    named!(struct_field -> Field, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        id: ident >>
        punct!(":") >>
        ty: ty >>
        (Field {
            ident: Some(id),
            vis: vis,
            attrs: attrs,
            ty: ty,
        })
    ));

    named!(tuple_field -> Field, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        ty: ty >>
        (Field {
            ident: None,
            vis: vis,
            attrs: attrs,
            ty: ty,
        })
    ));

    named!(pub visibility -> Visibility, alt!(
        keyword!("pub") => { |_| Visibility::Public }
        |
        epsilon!() => { |_| Visibility::Inherited }
    ));
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use quote::{Tokens, ToTokens};

    impl ToTokens for Variant {
        fn to_tokens(&self, tokens: &mut Tokens) {
            for attr in &self.attrs {
                attr.to_tokens(tokens);
            }
            self.ident.to_tokens(tokens);
            self.data.to_tokens(tokens);
            if let Some(ref disr) = self.discriminant {
                tokens.append("=");
                disr.to_tokens(tokens);
            }
        }
    }

    impl ToTokens for VariantData {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                VariantData::Struct(ref fields) => {
                    tokens.append("{");
                    tokens.append_separated(fields, ",");
                    tokens.append("}");
                }
                VariantData::Tuple(ref fields) => {
                    tokens.append("(");
                    tokens.append_separated(fields, ",");
                    tokens.append(")");
                }
                VariantData::Unit => {}
            }
        }
    }

    impl ToTokens for Field {
        fn to_tokens(&self, tokens: &mut Tokens) {
            for attr in &self.attrs {
                attr.to_tokens(tokens);
            }
            self.vis.to_tokens(tokens);
            if let Some(ref ident) = self.ident {
                ident.to_tokens(tokens);
                tokens.append(":");
            }
            self.ty.to_tokens(tokens);
        }
    }

    impl ToTokens for Visibility {
        fn to_tokens(&self, tokens: &mut Tokens) {
            if let Visibility::Public = *self {
                tokens.append("pub");
            }
        }
    }
}
