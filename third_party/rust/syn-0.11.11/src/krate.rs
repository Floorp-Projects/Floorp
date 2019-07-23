use super::*;

#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct Crate {
    pub shebang: Option<String>,
    pub attrs: Vec<Attribute>,
    pub items: Vec<Item>,
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use attr::parsing::inner_attr;
    use item::parsing::items;

    named!(pub krate -> Crate, do_parse!(
        option!(byte_order_mark) >>
        shebang: option!(shebang) >>
        attrs: many0!(inner_attr) >>
        items: items >>
        (Crate {
            shebang: shebang,
            attrs: attrs,
            items: items,
        })
    ));

    named!(byte_order_mark -> &str, tag!("\u{feff}"));

    named!(shebang -> String, do_parse!(
        tag!("#!") >>
        not!(tag!("[")) >>
        content: take_until!("\n") >>
        (format!("#!{}", content))
    ));
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use attr::FilterAttrs;
    use quote::{Tokens, ToTokens};

    impl ToTokens for Crate {
        fn to_tokens(&self, tokens: &mut Tokens) {
            if let Some(ref shebang) = self.shebang {
                tokens.append(&format!("{}\n", shebang));
            }
            for attr in self.attrs.inner() {
                attr.to_tokens(tokens);
            }
            for item in &self.items {
                item.to_tokens(tokens);
            }
        }
    }
}
