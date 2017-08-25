use quote::{Tokens, ToTokens};
use syn::{MetaItem, NestedMetaItem};

use {Error, FromMetaItem, Result};

#[derive(Debug, Clone)]
pub struct Shape {
    enum_values: DataShape,
    struct_values: DataShape,
    any: bool,
}

impl Shape {
    pub fn all() -> Self {
        Shape {
            any: true,
            ..Default::default()
        }
    }
}

impl Default for Shape {
    fn default() -> Self {
        Shape {
            enum_values: DataShape::new("enum_"),
            struct_values: DataShape::new("struct_"),
            any: Default::default(),
        }
    }
}

impl FromMetaItem for Shape {
    fn from_list(items: &[NestedMetaItem]) -> Result<Self> {
        let mut new = Shape::default();
        for item in items {
            if let NestedMetaItem::MetaItem(MetaItem::Word(ref ident)) = *item {
                let word = ident.as_ref();
                if word == "any" {
                    new.any = true;
                }
                else if word.starts_with("enum_") {
                    new.enum_values.set_word(word)?;
                } else if word.starts_with("struct_") {
                    new.struct_values.set_word(word)?;
                } else {
                    return Err(Error::unknown_value(word));
                }
            } else {
                return Err(Error::unsupported_format("non-word"));
            }
        }

        Ok(new)
    }
}

impl ToTokens for Shape {
    fn to_tokens(&self, tokens: &mut Tokens) {
        let fn_body = if self.any == true {
            quote!(::darling::export::Ok(()))
        }
        else {
            let en = &self.enum_values;
            let st = &self.struct_values;
            quote! {
                match *__body {
                    ::syn::Body::Enum(ref variants) => {
                        fn validate_variant(data: &::syn::VariantData) -> ::darling::Result<()> {
                            #en
                        }

                        for variant in variants {
                            validate_variant(&variant.data)?;
                        }

                        Ok(())
                    }
                    ::syn::Body::Struct(ref data) => {
                        #st
                    }
                }
            }
        };

        tokens.append(quote!{
            #[allow(unused_variables)]
            fn __validate_body(__body: &::syn::Body) -> ::darling::Result<()> {
                #fn_body
            }
        });
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct DataShape {
    prefix: &'static str,
    newtype: bool,
    named: bool,
    tuple: bool,
    unit: bool,
    any: bool,
    embedded: bool,
}

impl DataShape {
    fn new(prefix: &'static str) -> Self {
        DataShape {
            prefix,
            embedded: true,
            ..Default::default()
        }
    }

    fn supports_none(&self) -> bool {
        !(self.any || self.newtype || self.named || self.tuple || self.unit)
    } 

    fn set_word(&mut self, word: &str) -> Result<()> {
        match word.trim_left_matches(self.prefix) {
            "newtype" => {
                self.newtype = true;
                Ok(())
            }
            "named" => {
                self.named = true;
                Ok(())
            }
            "tuple" => {
                self.tuple = true;
                Ok(())
            }
            "unit" => {
                self.unit = true;
                Ok(())
            }
            "any" => {
                self.any = true;
                Ok(())
            }
            _ => Err(Error::unknown_value(word)),
        }
    }
}

impl FromMetaItem for DataShape {
    fn from_list(items: &[NestedMetaItem]) -> Result<Self> {
        let mut new = DataShape::default();
        for item in items {
            if let NestedMetaItem::MetaItem(MetaItem::Word(ref ident)) = *item {
                new.set_word(ident.as_ref())?;
            } else {
                return Err(Error::unsupported_format("non-word"));
            }
        }

        Ok(new)
    }
}

impl ToTokens for DataShape {
    fn to_tokens(&self, tokens: &mut Tokens) {
        let body = if self.any {
            quote!(::darling::export::Ok(()))
        } else if self.supports_none() {
            let ty = self.prefix.trim_right_matches("_");
            quote!(::darling::export::Err(::darling::Error::unsupported_format(#ty)))
        } else {
            let unit = match_arm("unit", self.unit);
            let newtype = match_arm("newtype", self.newtype);
            let named = match_arm("named", self.named);
            let tuple = match_arm("tuple", self.tuple);
            quote! {
                match *data {
                    ::syn::VariantData::Unit => #unit,
                    ::syn::VariantData::Tuple(ref fields) if fields.len() == 1 => #newtype,
                    ::syn::VariantData::Tuple(_) => #tuple,
                    ::syn::VariantData::Struct(_) => #named,
                }
            }
        };

        if self.embedded {
            tokens.append(body);
        } else {
            tokens.append(quote! {
                fn __validate_data(data: &::syn::VariantData) -> ::darling::Result<()> {
                    #body
                }
            });
        }
    }
}

fn match_arm(name: &'static str, is_supported: bool) -> Tokens {
    if is_supported {
        quote!(::darling::export::Ok(()))
    } else {
        quote!(::darling::export::Err(::darling::Error::unsupported_format(#name)))
    }
}

#[cfg(test)]
mod tests {
    use syn;
    
    use super::Shape;
    use {FromMetaItem};

    /// parse a string as a syn::MetaItem instance.
    fn pmi(s: &str) -> ::std::result::Result<syn::MetaItem, String> {
        Ok(syn::parse_outer_attr(&format!("#[{}]", s))?.value)
    }

    fn fmi<T: FromMetaItem>(s: &str) -> T {
        FromMetaItem::from_meta_item(&pmi(s).expect("Tests should pass well-formed input"))
            .expect("Tests should pass valid input")
    }

    #[test]
    fn supports_any() {
        let decl = fmi::<Shape>("ignore(any)");
        assert_eq!(decl.any, true);
    }

    #[test]
    fn supports_struct() {
        let decl = fmi::<Shape>("ignore(struct_any, struct_newtype)");
        assert_eq!(decl.struct_values.any, true);
        assert_eq!(decl.struct_values.newtype, true);
    }

    #[test]
    fn supports_mixed() {
        let decl = fmi::<Shape>("ignore(struct_newtype, enum_newtype, enum_tuple)");
        assert_eq!(decl.struct_values.newtype, true);
        assert_eq!(decl.enum_values.newtype, true);
        assert_eq!(decl.enum_values.tuple, true);
        assert_eq!(decl.struct_values.any, false);
    }
}