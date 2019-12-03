use crate::attr::Display;
use proc_macro2::TokenStream;
use quote::quote_spanned;
use syn::{Ident, LitStr};

impl Display<'_> {
    // Transform `"error {var}"` to `"error {}", var`.
    pub fn expand_shorthand(&mut self) {
        if !self.args.is_empty() {
            return;
        }

        let span = self.fmt.span();
        let fmt = self.fmt.value();
        let mut read = fmt.as_str();
        let mut out = String::new();
        let mut args = TokenStream::new();
        let mut has_bonus_display = false;

        while let Some(brace) = read.find('{') {
            out += &read[..=brace];
            read = &read[brace + 1..];
            if read.starts_with('{') {
                out.push('{');
                read = &read[1..];
                continue;
            }
            let next = match read.chars().next() {
                Some(next) => next,
                None => return,
            };
            let var = match next {
                '0'..='9' => take_int(&mut read),
                'a'..='z' | 'A'..='Z' | '_' => take_ident(&mut read),
                _ => return,
            };
            let ident = Ident::new(&var, span);
            args.extend(quote_spanned!(span=> , #ident));
            if read.starts_with('}') {
                has_bonus_display = true;
                args.extend(quote_spanned!(span=> .as_display()));
            }
        }

        out += read;
        self.fmt = LitStr::new(&out, self.fmt.span());
        self.args = args;
        self.was_shorthand = true;
        self.has_bonus_display = has_bonus_display;
    }
}

fn take_int(read: &mut &str) -> String {
    let mut int = String::new();
    int.push('_');
    for (i, ch) in read.char_indices() {
        match ch {
            '0'..='9' => int.push(ch),
            _ => {
                *read = &read[i..];
                break;
            }
        }
    }
    int
}

fn take_ident(read: &mut &str) -> String {
    let mut ident = String::new();
    for (i, ch) in read.char_indices() {
        match ch {
            'a'..='z' | 'A'..='Z' | '0'..='9' | '_' => ident.push(ch),
            _ => {
                *read = &read[i..];
                break;
            }
        }
    }
    ident
}

#[cfg(test)]
mod tests {
    use super::*;
    use proc_macro2::Span;
    use syn::parse_quote;

    fn assert(input: &str, fmt: &str, args: &str) {
        let mut display = Display {
            original: &parse_quote!(#[error]),
            fmt: LitStr::new(input, Span::call_site()),
            args: TokenStream::new(),
            was_shorthand: false,
            has_bonus_display: false,
        };
        display.expand_shorthand();
        assert_eq!(fmt, display.fmt.value());
        assert_eq!(args, display.args.to_string());
    }

    #[test]
    fn test_expand() {
        assert("error {var}", "error {}", ", var . as_display ( )");
        assert("fn main() {{ }}", "fn main() {{ }}", "");
        assert(
            "{v} {v:?} {0} {0:?}",
            "{} {:?} {} {:?}",
            ", v . as_display ( ) , v , _0 . as_display ( ) , _0",
        );
    }
}
