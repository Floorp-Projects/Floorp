use syn;

use {Error, FromMeta, Result};

mod core;
mod forward_attrs;
mod from_derive;
mod from_field;
mod from_meta;
mod from_type_param;
mod from_variant;
mod input_field;
mod input_variant;
mod outer_from;
mod shape;

pub use self::core::Core;
pub use self::forward_attrs::ForwardAttrs;
pub use self::from_derive::FdiOptions;
pub use self::from_field::FromFieldOptions;
pub use self::from_meta::FromMetaOptions;
pub use self::from_type_param::FromTypeParamOptions;
pub use self::from_variant::FromVariantOptions;
pub use self::input_field::InputField;
pub use self::input_variant::InputVariant;
pub use self::outer_from::OuterFrom;
pub use self::shape::{DataShape, Shape};

/// A default/fallback expression encountered in attributes during parsing.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DefaultExpression {
    /// The value should be taken from the `default` instance of the containing struct.
    /// This is not valid in container options.
    Inherit,
    Explicit(syn::Path),
    Trait,
}

#[doc(hidden)]
impl FromMeta for DefaultExpression {
    fn from_word() -> Result<Self> {
        Ok(DefaultExpression::Trait)
    }

    fn from_string(lit: &str) -> Result<Self> {
        Ok(DefaultExpression::Explicit(
            syn::parse_str(lit).map_err(|_| Error::unknown_value(lit))?
        ))
    }
}

/// Middleware for extracting attribute values.
pub trait ParseAttribute: Sized {
    fn parse_attributes(mut self, attrs: &[syn::Attribute]) -> Result<Self> {
        for attr in attrs {
            if attr.path == parse_quote!(darling) {
                parse_attr(attr, &mut self)?;
            }
        }

        Ok(self)
    }

    /// Read a meta-item, and apply its values to the current instance.
    fn parse_nested(&mut self, mi: &syn::Meta) -> Result<()>;
}

fn parse_attr<T: ParseAttribute>(attr: &syn::Attribute, target: &mut T) -> Result<()> {
    match attr.interpret_meta() {
        Some(syn::Meta::List(data)) => {
            for item in data.nested {
                if let syn::NestedMeta::Meta(ref mi) = item {
                    target.parse_nested(mi)?;
                } else {
                    panic!("Wasn't able to parse: `{:?}`", item);
                }
            }

            Ok(())
        }
        Some(ref item) => panic!("Wasn't able to parse: `{:?}`", item),
        None => panic!("Unable to parse {:?}", attr),
    }
}

pub trait ParseData: Sized {
    fn parse_body(mut self, body: &syn::Data) -> Result<Self> {
        use syn::{Data, Fields};

        match *body {
            Data::Struct(ref data) => match data.fields {
                Fields::Unit => Ok(self),
                Fields::Named(ref fields) => {
                    for field in &fields.named {
                        self.parse_field(field)?;
                    }
                    Ok(self)
                }
                Fields::Unnamed(ref fields) => {
                    for field in &fields.unnamed {
                        self.parse_field(field)?;
                    }

                    Ok(self)
                }
            },
            Data::Enum(ref data) => {
                for variant in &data.variants {
                    self.parse_variant(variant)?;
                }

                Ok(self)
            }
            Data::Union(_) => unreachable!(),
        }
    }

    /// Apply the next found variant to the object, returning an error
    /// if parsing goes wrong.
    #[allow(unused_variables)]
    fn parse_variant(&mut self, variant: &syn::Variant) -> Result<()> {
        Err(Error::unsupported_format("enum variant"))
    }

    /// Apply the next found struct field to the object, returning an error
    /// if parsing goes wrong.
    #[allow(unused_variables)]
    fn parse_field(&mut self, field: &syn::Field) -> Result<()> {
        Err(Error::unsupported_format("struct field"))
    }
}
