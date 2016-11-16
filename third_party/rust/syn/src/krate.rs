use super::*;

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct Crate {
    pub shebang: Option<String>,
    pub attrs: Vec<Attribute>,
    pub items: Vec<Item>,
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use attr::parsing::inner_attr;
    use space::whitespace;
    use item::parsing::item;

    named!(pub krate -> Crate, do_parse!(
        attrs: many0!(inner_attr) >>
        items: many0!(item) >>
        option!(whitespace) >>
        (Crate {
            shebang: None,
            attrs: attrs,
            items: items,
        })
    ));
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use attr::FilterAttrs;
    use quote::{Tokens, ToTokens};

    impl ToTokens for Crate {
        fn to_tokens(&self, tokens: &mut Tokens) {
            for attr in self.attrs.inner() {
                attr.to_tokens(tokens);
            }
            for item in &self.items {
                item.to_tokens(tokens);
            }
        }
    }
}
