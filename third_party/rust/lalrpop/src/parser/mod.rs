use std::iter;

use grammar::parse_tree::*;
use grammar::pattern::*;
use lalrpop_util;
use tok;

#[cfg(not(any(feature = "test", test)))]
#[allow(dead_code)]
mod lrgrammar;

#[cfg(any(feature = "test", test))]
lalrpop_mod!(
    // ---------------------------------------------------------------------------------------
    // NOTE: Run `cargo build -p lalrpop` once before running `cargo test` to create this file
    // ---------------------------------------------------------------------------------------
    #[allow(dead_code)]
    lrgrammar,
    "/src/parser/lrgrammar.rs"
);

#[cfg(test)]
mod test;

pub enum Top {
    Grammar(Grammar),
    Pattern(Pattern<TypeRef>),
    MatchMapping(TerminalString),
    TypeRef(TypeRef),
    GrammarWhereClauses(Vec<WhereClause<TypeRef>>),
}

pub type ParseError<'input> = lalrpop_util::ParseError<usize, tok::Tok<'input>, tok::Error>;

macro_rules! parser {
    ($input: expr, $offset: expr, $pat: ident, $tok: ident) => {{
        let input = $input;
        let tokenizer =
            iter::once(Ok((0, tok::Tok::$tok, 0))).chain(tok::Tokenizer::new(input, $offset));
        lrgrammar::TopParser::new()
            .parse(input, tokenizer)
            .map(|top| match top {
                Top::$pat(x) => x,
                _ => unreachable!(),
            })
    }};
}

pub fn parse_grammar<'input>(input: &'input str) -> Result<Grammar, ParseError<'input>> {
    let mut grammar = try!(parser!(input, 0, Grammar, StartGrammar));

    // find a unique prefix that does not appear anywhere in the input
    while input.contains(&grammar.prefix) {
        grammar.prefix.push('_');
    }

    Ok(grammar)
}

fn parse_pattern<'input>(
    input: &'input str,
    offset: usize,
) -> Result<Pattern<TypeRef>, ParseError<'input>> {
    parser!(input, offset, Pattern, StartPattern)
}

fn parse_match_mapping<'input>(
    input: &'input str,
    offset: usize,
) -> Result<MatchMapping, ParseError<'input>> {
    parser!(input, offset, MatchMapping, StartMatchMapping)
}

#[cfg(test)]
pub fn parse_type_ref<'input>(input: &'input str) -> Result<TypeRef, ParseError<'input>> {
    parser!(input, 0, TypeRef, StartTypeRef)
}

#[cfg(test)]
pub fn parse_where_clauses<'input>(
    input: &'input str,
) -> Result<Vec<WhereClause<TypeRef>>, ParseError<'input>> {
    parser!(input, 0, GrammarWhereClauses, StartGrammarWhereClauses)
}
