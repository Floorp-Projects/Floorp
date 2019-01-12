//! Validate checks some basic safety conditions.

use super::norm_util::{self, Symbols};
use super::{NormError, NormResult};

use collections::{set, Multimap};
use grammar::consts::*;
use grammar::parse_tree::*;
use grammar::repr as r;
use string_cache::DefaultAtom as Atom;
use util::Sep;

#[cfg(test)]
mod test;

pub fn validate(grammar: &Grammar) -> NormResult<()> {
    let match_token: Option<&MatchToken> = grammar
        .items
        .iter()
        .filter_map(|item| item.as_match_token())
        .next();

    let extern_token: Option<&ExternToken> = grammar
        .items
        .iter()
        .filter_map(|item| item.as_extern_token())
        .next();

    let validator = Validator {
        grammar: grammar,
        match_token: match_token,
        extern_token: extern_token,
    };

    validator.validate()
}

struct Validator<'grammar> {
    grammar: &'grammar Grammar,
    match_token: Option<&'grammar MatchToken>,
    extern_token: Option<&'grammar ExternToken>,
}

impl<'grammar> Validator<'grammar> {
    fn validate(&self) -> NormResult<()> {
        let allowed_names = vec![
            Atom::from(LALR),
            Atom::from(TABLE_DRIVEN),
            Atom::from(RECURSIVE_ASCENT),
            Atom::from(TEST_ALL),
        ];
        for annotation in &self.grammar.annotations {
            if !allowed_names.contains(&annotation.id) {
                return_err!(
                    annotation.id_span,
                    "unrecognized annotation `{}`",
                    annotation.id
                );
            }
        }

        for item in &self.grammar.items {
            match *item {
                GrammarItem::Use(..) => {}

                GrammarItem::MatchToken(ref data) => {
                    if data.span != self.match_token.unwrap().span {
                        return_err!(data.span, "multiple match definitions are not permitted");
                    }

                    // Only error if a custom lexer is specified, having a custom types is ok
                    if let Some(d) = self.extern_token {
                        if d.enum_token.is_some() {
                            return_err!(
                                d.span,
                                "extern (with custom tokens) and match definitions are mutually exclusive");
                        }
                    }

                    // Ensure that the catch all is final item of final block
                    for (contents_idx, match_contents) in data.contents.iter().enumerate() {
                        for (item_idx, item) in match_contents.items.iter().enumerate() {
                            if item.is_catch_all()
                                && (contents_idx != &data.contents.len() - 1
                                    || item_idx != &match_contents.items.len() - 1)
                            {
                                return_err!(item.span(), "Catch all must be final item");
                            } else {
                                println!("ok");
                            }
                        }
                    }
                }

                GrammarItem::ExternToken(ref data) => {
                    if data.span != self.extern_token.unwrap().span {
                        return_err!(data.span, "multiple extern definitions are not permitted");
                    }

                    // Only error if a custom lexer is specified, having a custom types is ok
                    if let Some(d) = self.match_token {
                        if data.enum_token.is_some() {
                            return_err!(
                                d.span,
                                "match and extern (with custom tokens) definitions are mutually exclusive");
                        }
                    }

                    let allowed_names = vec![Atom::from(LOCATION), Atom::from(ERROR)];
                    let mut new_names = set();
                    for associated_type in &data.associated_types {
                        if !allowed_names.contains(&associated_type.type_name) {
                            return_err!(
                                associated_type.type_span,
                                "associated type `{}` not recognized, \
                                 try one of the following: {}",
                                associated_type.type_name,
                                Sep(", ", &allowed_names)
                            );
                        } else if !new_names.insert(associated_type.type_name.clone()) {
                            return_err!(
                                associated_type.type_span,
                                "associated type `{}` already specified",
                                associated_type.type_name
                            );
                        }
                    }
                }
                GrammarItem::Nonterminal(ref data) => {
                    if data.visibility.is_pub() && !data.args.is_empty() {
                        return_err!(data.span, "macros cannot be marked public");
                    }
                    let inline_annotation = Atom::from(INLINE);
                    let cfg_annotation = Atom::from(CFG);
                    let known_annotations = [inline_annotation.clone(), cfg_annotation.clone()];
                    let mut found_annotations = set();
                    for annotation in &data.annotations {
                        if !known_annotations.contains(&annotation.id) {
                            return_err!(
                                annotation.id_span,
                                "unrecognized annotation `{}`",
                                annotation.id
                            );
                        } else if !found_annotations.insert(annotation.id.clone()) {
                            return_err!(
                                annotation.id_span,
                                "duplicate annotation `{}`",
                                annotation.id
                            );
                        } else if annotation.id == inline_annotation && data.visibility.is_pub() {
                            return_err!(
                                annotation.id_span,
                                "public items cannot be marked #[inline]"
                            );
                        } else if annotation.id == cfg_annotation {
                            if data.visibility.is_pub() {
                                match annotation.arg {
                                Some((ref name, _)) if name == "feature" => (),
                                _ => return_err!(
                                    annotation.id_span,
                                    r#"`cfg` annotations must have a `feature = "my_feature" argument"#
                                ),
                            }
                            } else {
                                return_err!(
                                    annotation.id_span,
                                    "private items cannot be marked #[cfg]"
                                );
                            }
                        }
                    }

                    for alternative in &data.alternatives {
                        try!(self.validate_alternative(alternative));
                    }
                }
                GrammarItem::InternToken(..) => {}
            }
        }
        Ok(())
    }

    fn validate_alternative(&self, alternative: &Alternative) -> NormResult<()> {
        try!(self.validate_expr(&alternative.expr));

        match norm_util::analyze_expr(&alternative.expr) {
            Symbols::Named(syms) => {
                if alternative.action.is_none() {
                    let sym = syms.iter().map(|&(_, _, sym)| sym).next().unwrap();
                    return_err!(
                        sym.span,
                        "named symbols (like `{}`) require a custom action",
                        sym
                    );
                }
            }
            Symbols::Anon(_) => {
                let empty_string = "".to_string();
                let action = {
                    match alternative.action {
                        Some(ActionKind::User(ref action)) => action,
                        Some(ActionKind::Fallible(ref action)) => action,
                        _ => &empty_string,
                    }
                };
                if norm_util::check_between_braces(action).is_in_curly_brackets() {
                    return_err!(
                        alternative.span,
                        "Using `<>` between curly braces (e.g., `{{<>}}`) only works when your parsed values have been given names (e.g., `<x:Foo>`, not just `<Foo>`)");
                }
            }
        }

        Ok(())
    }

    fn validate_expr(&self, expr: &ExprSymbol) -> NormResult<()> {
        for symbol in &expr.symbols {
            try!(self.validate_symbol(symbol));
        }

        let chosen: Vec<&Symbol> = expr
            .symbols
            .iter()
            .filter(|sym| match sym.kind {
                SymbolKind::Choose(_) => true,
                _ => false,
            })
            .collect();

        let named: Multimap<Atom, Vec<&Symbol>> = expr
            .symbols
            .iter()
            .filter_map(|sym| match sym.kind {
                SymbolKind::Name(ref nt, _) => Some((nt.clone(), sym)),
                _ => None,
            })
            .collect();

        if !chosen.is_empty() && !named.is_empty() {
            return_err!(
                chosen[0].span,
                "anonymous symbols like this one cannot be combined with \
                 named symbols like `{}`",
                named.into_iter().next().unwrap().1[0]
            );
        }

        for (name, syms) in named.into_iter() {
            if syms.len() > 1 {
                return_err!(
                    syms[1].span,
                    "multiple symbols named `{}` are not permitted",
                    name
                );
            }
        }

        Ok(())
    }

    fn validate_symbol(&self, symbol: &Symbol) -> NormResult<()> {
        match symbol.kind {
            SymbolKind::Expr(ref expr) => {
                try!(self.validate_expr(expr));
            }
            SymbolKind::AmbiguousId(_) => { /* see resolve */ }
            SymbolKind::Terminal(_) => { /* see postvalidate! */ }
            SymbolKind::Nonterminal(_) => { /* see resolve */ }
            SymbolKind::Error => {
                let mut algorithm = r::Algorithm::default();
                read_algorithm(&self.grammar.annotations, &mut algorithm);
                if algorithm.codegen == r::LrCodeGeneration::RecursiveAscent {
                    return_err!(
                        symbol.span,
                        "error recovery is not yet supported by recursive ascent parsers"
                    );
                }
            }
            SymbolKind::Macro(ref msym) => {
                debug_assert!(msym.args.len() > 0);
                for arg in &msym.args {
                    try!(self.validate_symbol(arg));
                }
            }
            SymbolKind::Repeat(ref repeat) => {
                try!(self.validate_symbol(&repeat.symbol));
            }
            SymbolKind::Choose(ref sym) | SymbolKind::Name(_, ref sym) => {
                try!(self.validate_symbol(sym));
            }
            SymbolKind::Lookahead | SymbolKind::Lookbehind => {
                // if using an internal tokenizer, lookahead/lookbehind are ok.
                if let Some(extern_token) = self.extern_token {
                    if extern_token.enum_token.is_some() {
                        // otherwise, the Location type must be specified.
                        let loc = Atom::from(LOCATION);
                        if self.extern_token.unwrap().associated_type(loc).is_none() {
                            return_err!(
                                symbol.span,
                                "lookahead/lookbehind require you to declare the type of \
                                 a location; add a `type {} = ..` statement to the extern token \
                                 block",
                                LOCATION
                            );
                        }
                    }
                }
            }
        }

        Ok(())
    }
}
