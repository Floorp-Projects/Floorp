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

    fn from_value(value: &syn::Lit) -> Result<Self> {
        syn::Path::from_value(value).map(DefaultExpression::Explicit)
    }
}

/// Run a parsing task, and if it produces an error push it into `$errors`
macro_rules! collect_error {
    ($errors:ident, $task:expr) => {
        if let Err(e) = $task {
            $errors.push(e);
        }
    };
}

/// Middleware for extracting attribute values. Implementers are expected to override
/// `parse_nested` so they can apply individual items to themselves, while `parse_attributes`
/// is responsible for looping through distinct outer attributes and collecting errors.
pub trait ParseAttribute: Sized {
    fn parse_attributes(mut self, attrs: &[syn::Attribute]) -> Result<Self> {
        let mut errors = Vec::new();
        for attr in attrs {
            if attr.path == parse_quote!(darling) {
                collect_error!(errors, parse_attr(attr, &mut self));
            }
        }

        if !errors.is_empty() {
            Err(Error::multiple(errors))
        } else {
            Ok(self)
        }
    }

    /// Read a meta-item, and apply its values to the current instance.
    fn parse_nested(&mut self, mi: &syn::Meta) -> Result<()>;
}

fn parse_attr<T: ParseAttribute>(attr: &syn::Attribute, target: &mut T) -> Result<()> {
    let mut errors = Vec::new();
    match attr.parse_meta().ok() {
        Some(syn::Meta::List(data)) => {
            for item in data.nested {
                if let syn::NestedMeta::Meta(ref mi) = item {
                    collect_error!(errors, target.parse_nested(mi));
                } else {
                    panic!("Wasn't able to parse: `{:?}`", item);
                }
            }

            if !errors.is_empty() {
                Err(Error::multiple(errors))
            } else {
                Ok(())
            }
        }
        Some(ref item) => panic!("Wasn't able to parse: `{:?}`", item),
        None => panic!("Unable to parse {:?}", attr),
    }
}

/// Middleware for extracting values from the body of the derive input. Implementers are
/// expected to override `parse_field` or `parse_variant` as appropriate for their use-case,
/// while `parse_body` dispatches to the appropriate methods and handles error collection.
pub trait ParseData: Sized {
    fn parse_body(mut self, body: &syn::Data) -> Result<Self> {
        use syn::{Data, Fields};

        let mut errors = Vec::new();

        match *body {
            Data::Struct(ref data) => match data.fields {
                Fields::Unit => {}
                Fields::Named(ref fields) => {
                    for field in &fields.named {
                        collect_error!(errors, self.parse_field(field));
                    }
                }
                Fields::Unnamed(ref fields) => {
                    for field in &fields.unnamed {
                        collect_error!(errors, self.parse_field(field));
                    }
                }
            },
            Data::Enum(ref data) => {
                for variant in &data.variants {
                    collect_error!(errors, self.parse_variant(variant));
                }
            }
            Data::Union(_) => unreachable!(),
        };

        if !errors.is_empty() {
            Err(Error::multiple(errors))
        } else {
            Ok(self)
        }
    }

    /// Apply the next found variant to the object, returning an error
    /// if parsing goes wrong.
    fn parse_variant(&mut self, variant: &syn::Variant) -> Result<()> {
        Err(Error::unsupported_format("enum variant").with_span(variant))
    }

    /// Apply the next found struct field to the object, returning an error
    /// if parsing goes wrong.
    fn parse_field(&mut self, field: &syn::Field) -> Result<()> {
        Err(Error::unsupported_format("struct field").with_span(field))
    }
}
