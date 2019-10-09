use super::*;

ast_struct! {
    /// A braced block containing Rust statements.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    pub struct Block {
        pub brace_token: token::Brace,
        /// Statements in a block
        pub stmts: Vec<Stmt>,
    }
}

ast_enum! {
    /// A statement, usually ending in a semicolon.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    pub enum Stmt {
        /// A local (let) binding.
        Local(Local),

        /// An item definition.
        Item(Item),

        /// Expr without trailing semicolon.
        Expr(Expr),

        /// Expression with trailing semicolon.
        Semi(Expr, Token![;]),
    }
}

ast_struct! {
    /// A local `let` binding: `let x: u64 = s.parse()?`.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    pub struct Local {
        pub attrs: Vec<Attribute>,
        pub let_token: Token![let],
        pub pat: Pat,
        pub init: Option<(Token![=], Box<Expr>)>,
        pub semi_token: Token![;],
    }
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;

    use crate::parse::{Parse, ParseStream, Result};
    use crate::punctuated::Punctuated;

    impl Block {
        /// Parse the body of a block as zero or more statements, possibly
        /// including one trailing expression.
        ///
        /// *This function is available if Syn is built with the `"parsing"`
        /// feature.*
        ///
        /// # Example
        ///
        /// ```
        /// use syn::{braced, token, Attribute, Block, Ident, Result, Stmt, Token};
        /// use syn::parse::{Parse, ParseStream};
        ///
        /// // Parse a function with no generics or parameter list.
        /// //
        /// //     fn playground {
        /// //         let mut x = 1;
        /// //         x += 1;
        /// //         println!("{}", x);
        /// //     }
        /// struct MiniFunction {
        ///     attrs: Vec<Attribute>,
        ///     fn_token: Token![fn],
        ///     name: Ident,
        ///     brace_token: token::Brace,
        ///     stmts: Vec<Stmt>,
        /// }
        ///
        /// impl Parse for MiniFunction {
        ///     fn parse(input: ParseStream) -> Result<Self> {
        ///         let outer_attrs = input.call(Attribute::parse_outer)?;
        ///         let fn_token: Token![fn] = input.parse()?;
        ///         let name: Ident = input.parse()?;
        ///
        ///         let content;
        ///         let brace_token = braced!(content in input);
        ///         let inner_attrs = content.call(Attribute::parse_inner)?;
        ///         let stmts = content.call(Block::parse_within)?;
        ///
        ///         Ok(MiniFunction {
        ///             attrs: {
        ///                 let mut attrs = outer_attrs;
        ///                 attrs.extend(inner_attrs);
        ///                 attrs
        ///             },
        ///             fn_token,
        ///             name,
        ///             brace_token,
        ///             stmts,
        ///         })
        ///     }
        /// }
        /// ```
        pub fn parse_within(input: ParseStream) -> Result<Vec<Stmt>> {
            let mut stmts = Vec::new();
            loop {
                while input.peek(Token![;]) {
                    input.parse::<Token![;]>()?;
                }
                if input.is_empty() {
                    break;
                }
                let s = parse_stmt(input, true)?;
                let requires_semicolon = if let Stmt::Expr(s) = &s {
                    expr::requires_terminator(s)
                } else {
                    false
                };
                stmts.push(s);
                if input.is_empty() {
                    break;
                } else if requires_semicolon {
                    return Err(input.error("unexpected token"));
                }
            }
            Ok(stmts)
        }
    }

    impl Parse for Block {
        fn parse(input: ParseStream) -> Result<Self> {
            let content;
            Ok(Block {
                brace_token: braced!(content in input),
                stmts: content.call(Block::parse_within)?,
            })
        }
    }

    impl Parse for Stmt {
        fn parse(input: ParseStream) -> Result<Self> {
            parse_stmt(input, false)
        }
    }

    fn parse_stmt(input: ParseStream, allow_nosemi: bool) -> Result<Stmt> {
        // TODO: optimize using advance_to
        let ahead = input.fork();
        ahead.call(Attribute::parse_outer)?;

        if {
            let ahead = ahead.fork();
            // Only parse braces here; paren and bracket will get parsed as
            // expression statements
            ahead.call(Path::parse_mod_style).is_ok()
                && ahead.parse::<Token![!]>().is_ok()
                && (ahead.peek(token::Brace) || ahead.peek(Ident))
        } {
            stmt_mac(input)
        } else if ahead.peek(Token![let]) {
            stmt_local(input).map(Stmt::Local)
        } else if ahead.peek(Token![pub])
            || ahead.peek(Token![crate]) && !ahead.peek2(Token![::])
            || ahead.peek(Token![extern]) && !ahead.peek2(Token![::])
            || ahead.peek(Token![use])
            || ahead.peek(Token![static]) && (ahead.peek2(Token![mut]) || ahead.peek2(Ident))
            || ahead.peek(Token![const])
            || ahead.peek(Token![unsafe]) && !ahead.peek2(token::Brace)
            || ahead.peek(Token![async])
                && (ahead.peek2(Token![unsafe])
                    || ahead.peek2(Token![extern])
                    || ahead.peek2(Token![fn]))
            || ahead.peek(Token![fn])
            || ahead.peek(Token![mod])
            || ahead.peek(Token![type])
            || ahead.peek(item::parsing::existential) && ahead.peek2(Token![type])
            || ahead.peek(Token![struct])
            || ahead.peek(Token![enum])
            || ahead.peek(Token![union]) && ahead.peek2(Ident)
            || ahead.peek(Token![auto]) && ahead.peek2(Token![trait])
            || ahead.peek(Token![trait])
            || ahead.peek(Token![default])
                && (ahead.peek2(Token![unsafe]) || ahead.peek2(Token![impl]))
            || ahead.peek(Token![impl])
            || ahead.peek(Token![macro])
        {
            input.parse().map(Stmt::Item)
        } else {
            stmt_expr(input, allow_nosemi)
        }
    }

    fn stmt_mac(input: ParseStream) -> Result<Stmt> {
        let attrs = input.call(Attribute::parse_outer)?;
        let path = input.call(Path::parse_mod_style)?;
        let bang_token: Token![!] = input.parse()?;
        let ident: Option<Ident> = input.parse()?;
        let (delimiter, tokens) = mac::parse_delimiter(input)?;
        let semi_token: Option<Token![;]> = input.parse()?;

        Ok(Stmt::Item(Item::Macro(ItemMacro {
            attrs,
            ident,
            mac: Macro {
                path,
                bang_token,
                delimiter,
                tokens,
            },
            semi_token,
        })))
    }

    fn stmt_local(input: ParseStream) -> Result<Local> {
        Ok(Local {
            attrs: input.call(Attribute::parse_outer)?,
            let_token: input.parse()?,
            pat: {
                let leading_vert: Option<Token![|]> = input.parse()?;
                let mut pat: Pat = input.parse()?;
                if leading_vert.is_some()
                    || input.peek(Token![|]) && !input.peek(Token![||]) && !input.peek(Token![|=])
                {
                    let mut cases = Punctuated::new();
                    cases.push_value(pat);
                    while input.peek(Token![|])
                        && !input.peek(Token![||])
                        && !input.peek(Token![|=])
                    {
                        let punct = input.parse()?;
                        cases.push_punct(punct);
                        let pat: Pat = input.parse()?;
                        cases.push_value(pat);
                    }
                    pat = Pat::Or(PatOr {
                        attrs: Vec::new(),
                        leading_vert,
                        cases,
                    });
                }
                if input.peek(Token![:]) {
                    let colon_token: Token![:] = input.parse()?;
                    let ty: Type = input.parse()?;
                    pat = Pat::Type(PatType {
                        attrs: Vec::new(),
                        pat: Box::new(pat),
                        colon_token,
                        ty: Box::new(ty),
                    });
                }
                pat
            },
            init: {
                if input.peek(Token![=]) {
                    let eq_token: Token![=] = input.parse()?;
                    let init: Expr = input.parse()?;
                    Some((eq_token, Box::new(init)))
                } else {
                    None
                }
            },
            semi_token: input.parse()?,
        })
    }

    fn stmt_expr(input: ParseStream, allow_nosemi: bool) -> Result<Stmt> {
        let mut attrs = input.call(Attribute::parse_outer)?;
        let mut e = expr::parsing::expr_early(input)?;

        attrs.extend(e.replace_attrs(Vec::new()));
        e.replace_attrs(attrs);

        if input.peek(Token![;]) {
            return Ok(Stmt::Semi(e, input.parse()?));
        }

        if allow_nosemi || !expr::requires_terminator(&e) {
            Ok(Stmt::Expr(e))
        } else {
            Err(input.error("expected semicolon"))
        }
    }
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;

    use proc_macro2::TokenStream;
    use quote::{ToTokens, TokenStreamExt};

    impl ToTokens for Block {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.brace_token.surround(tokens, |tokens| {
                tokens.append_all(&self.stmts);
            });
        }
    }

    impl ToTokens for Stmt {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            match self {
                Stmt::Local(local) => local.to_tokens(tokens),
                Stmt::Item(item) => item.to_tokens(tokens),
                Stmt::Expr(expr) => expr.to_tokens(tokens),
                Stmt::Semi(expr, semi) => {
                    expr.to_tokens(tokens);
                    semi.to_tokens(tokens);
                }
            }
        }
    }

    impl ToTokens for Local {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            expr::printing::outer_attrs_to_tokens(&self.attrs, tokens);
            self.let_token.to_tokens(tokens);
            self.pat.to_tokens(tokens);
            if let Some((eq_token, init)) = &self.init {
                eq_token.to_tokens(tokens);
                init.to_tokens(tokens);
            }
            self.semi_token.to_tokens(tokens);
        }
    }
}
