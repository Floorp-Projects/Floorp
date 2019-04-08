//! A compiler from an LR(1) table to a traditional table driven parser.

use collections::{Entry, Map, Set};
use grammar::parse_tree::WhereClause;
use grammar::repr::*;
use lr1::core::*;
use lr1::lookahead::Token;
use rust::RustWrite;
use std::fmt;
use std::io::{self, Write};
use string_cache::DefaultAtom as Atom;
use tls::Tls;
use util::Sep;

use super::base::CodeGenerator;

const DEBUG_PRINT: bool = false;

pub fn compile<'grammar, W: Write>(
    grammar: &'grammar Grammar,
    user_start_symbol: NonterminalString,
    start_symbol: NonterminalString,
    states: &[LR1State<'grammar>],
    action_module: &str,
    out: &mut RustWrite<W>,
) -> io::Result<()> {
    let mut table_driven = CodeGenerator::new_table_driven(
        grammar,
        user_start_symbol,
        start_symbol,
        states,
        action_module,
        out,
    );
    table_driven.write()
}

// We create three parse tables:
//
// - `ACTION[state * num_states + terminal]: i32`: given a state and next token,
//   yields an integer indicating whether to shift/reduce (see below)
// - `EOF_ACTION[state]: i32`: as above, but for the EOF token
// - `GOTO[state * num_states + nonterminal]: i32`: index + 1 of state to jump to when given
//   nonterminal is pushed (no error is possible)
//
// For the `ACTION` and `EOF_ACTION` tables, the value is an `i32` and
// its interpretation varies depending on whether it is positive or
// negative:
//
// - if zero, parse error.
// - if a positive integer (not zero), it is the next state to shift to.
// - if a negative integer (not zero), it is the index of a reduction
//   action to execute (actually index + 1).
//
// We maintain two stacks: one is a stack of state indexes (each an
// u32). The other is a stack of values and spans: `(L, T, L)`. `L` is
// the location type and represents the start/end span. `T` is the
// value of the symbol. The type `T` is an `enum` that we synthesize
// which contains a variant for all the possibilities:
//
// ```
// enum Value<> {
//     // One variant for each terminal:
//     Term1(Ty1),
//     ...
//     TermN(TyN),
//
//     // One variant for each nonterminal:
//     Nt1(Ty1),
//     ...
//     NtN(TyN),
// }
// ```
//
// The action parser function looks like this (pseudo-code):
//
// ```
// fn parse_fn<TOKENS>(tokens: TOKENS) -> Result<T, Error>
//    where TOKENS: Iterator<Item=Result<(Location, Token, Location), Error>>
// {
//    let mut states = vec![0]; // initial state is zero
//    let mut symbols = vec![];
//    'shift: loop {
//        // Code to shift the next symbol and determine which terminal
//        // it is; emitted by `shift_symbol()`.
//        let lookahead = match tokens.next() {
//            Some(Ok(l)) => l,
//            None => break 'shift,
//            Some(Err(e)) => return Err(e),
//        };
//        let integer = match lookahead {
//            (_, PatternForTerminal0(...), _) => 0,
//            ...
//        };
//
//        // Code to process next symbol.
//        'inner: loop {
//            let symbol = match lookahead {
//               (l, PatternForTerminal0(...), r) => {
//                   (l, Value::VariantForTerminal0(...), r),
//               }
//               ...
//            };
//            let state = *states.last().unwrap() as usize;
//            let action = ACTION[state * NUM_STATES + integer];
//            if action > 0 { // shift
//                states.push(action - 1);
//                symbols.push(symbol);
//                continue 'shift;
//            } else if action < 0 { // reduce
//                if let Some(r) = reduce(action, Some(&lookahead.0), &mut states, &mut symbols) {
//                    // Give errors from within grammar a higher priority
//                    if r.is_err() {
//                        return r;
//                    }
//                    return Err(lalrpop_util::ParseError::ExtraToken { token: lookahead });
//                }
//            } else {
//                // Error recovery code: emitted by `try_error_recovery`
//                let mut err_lookahead = Some(lookahead);
//                let mut err_integer = Some(integer);
//                match error_recovery(&mut tokens, &mut states, &mut symbols, last_location,
//                                     &mut err_lookahead, &mut err_integer) {
//                  Err(e) => return e,
//                  Ok(Some(v)) => return Ok(v),
//                  Ok(None) => { }
//                }
//                match (err_lookahead, err_integer) {
//                  (Some(l), Some(i)) => {
//                    lookahead = l;
//                    integer = i;
//                    continue 'inner;
//                  }
//                  _ => break 'shift;
//                }
//            }
//        }
//    }
//
//    // Process EOF
//    while let Some(state) = self.states.pop() {
//        let action = EOF_ACTION[state * NUM_STATES];
//        if action < 0 { // reduce
//            try!(reduce(action, None, &mut states, &mut symbols));
//        } else {
//            let mut err_lookahead = None;
//            let mut err_integer = None;
//            match error_recovery(&mut tokens, &mut states, &mut symbols, last_location,
//                                 &mut err_lookahead, &mut err_integer) {
//              Err(e) => return e,
//              Ok(Some(v)) => return Ok(v),
//              Ok(None) => { }
//            }
//        }
//    }
// }
//
// // generated by `emit_reduce_actions()`
// fn reduce(action: i32, lookahead_start: Option<&L>,
//           states: &mut Vec<i32>, symbols: &mut Vec<(L, Symbol, L))
//           -> Option<Result<..>> {
//     let nonterminal = match -action {
//        0 => {
//            // Execute reduce action 0 to produce nonterminal N, popping from stacks etc
//            // (generated by `emit_reduce_action()`). If this is a fallible action,
//            // it may return `Some(Err)`, and if this is a reduce of the start NT,
//            // it may return `Some(Ok)`.
//            states.pop(); // however many times
//            symbols.pop(); // however many times
//            let data = action_fn0(...);
//            symbols.push((l, Value::VariantForNonterminalN(data), r));
//            N
//        }
//        ...
//     };
//     let state = *states.last().unwrap();
//     let next_state = GOTO[state * NUM_STATES + nonterminal] - 1;
//     state_stack.push(next_state);
//     None
// }
//
// generated by `write_error_recovery_fn`
// fn error_recovery(...) {
//     let mut dropped_tokens = vec![];
//
//     // First, reduce as long as we can with the `!` token as lookahead
//     loop {
//         let state = *states.last().unwrap() as usize;
//         let action = ACTION[(state + 1) * ACTIONS_PER_STATE - 1];
//         if action >= 0 {
//             break;
//         }
//         if let Some(r) = reduce(action, None, &mut states, &mut symbols) {
//             return r;
//         }
//     }
//
//     let top0;
//     'find_state: loop {
//         // See if there is a state that can shift `!` token. If so,
//         // break.
//         for top in (0..states.len()).rev() {
//             let state = states[top];
//             let action = ACTION[state * ACTIONS_PER_STATE + 1];
//             if action <= 0 { continue; }
//             let error_state = action - 1;
//             if accepts(error_state, &states[..top+1], *opt-integer) {
//                 top0 = top;
//                 break 'find_state;
//             }
//         }
//
//         // Else, drop a token from the input and try again.
//         'eof: loop {
//             match opt_lookahead.take() {
//                 None => {
//                     // No more tokens to drop
//                     return Err(...);
//                 }
//                 Some(mut lookahead) => {
//                     dropped_tokens.push(lookahead);
//                     next_token()
//                     opt_lookahead = Some(match tokens.next() {
//                         Some(Ok(l)) => l,
//                         None => break 'eof,
//                         Some(Err(e)) => return Err(e),
//                     });
//                     opt_integer = Some(match lookahead {
//                         (_, PatternForTerminal0(...), _) => 0,
//                         ...
//                     });
//                     continue 'find_state;
//                 }
//             }
//         }
//         opt_lookahead = None;
//         opt_integer = None;
//     }
//
//     let top = top0;
//     let start = /* figure out "start" of error */;
//     let end = /* figure out "end" of error */;
//     states.truncate(top + 1);
//     symbols.truncate(top);
//     let recover_state = states[top];
//     let error_state = ACTION[recover_state * ACTIONS_PER_STATE + 1] - 1;
//     states.push(error_state);
//     let recovery = ErrorRecovery { dropped_tokens, ... };
//     symbols.push((start, Symbol::Termerror(recovery), end));
//     Ok(None)
// }
// ```

enum Comment<'a, T> {
    Goto(T, usize),
    Error(T),
    Reduce(T, &'a Production),
}

impl<'a, T: fmt::Display> fmt::Display for Comment<'a, T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            Comment::Goto(ref token, new_state) => {
                write!(f, " // on {}, goto {}", token, new_state)
            }
            Comment::Error(ref token) => write!(f, " // on {}, error", token),
            Comment::Reduce(ref token, production) => {
                write!(f, " // on {}, reduce `{:?}`", token, production)
            }
        }
    }
}

struct TableDriven<'grammar> {
    /// type parameters for the `Nonterminal` type
    symbol_type_params: Vec<TypeParameter>,

    symbol_where_clauses: Vec<WhereClause<TypeRepr>>,

    /// a list of each nonterminal in some specific order
    all_nonterminals: Vec<NonterminalString>,

    reduce_indices: Map<&'grammar Production, usize>,

    state_type: &'static str,

    variant_names: Map<Symbol, String>,
    variants: Map<TypeRepr, String>,
    reduce_functions: Set<usize>,
}

impl<'ascent, 'grammar, W: Write> CodeGenerator<'ascent, 'grammar, W, TableDriven<'grammar>> {
    fn new_table_driven(
        grammar: &'grammar Grammar,
        user_start_symbol: NonterminalString,
        start_symbol: NonterminalString,
        states: &'ascent [LR1State<'grammar>],
        action_module: &str,
        out: &'ascent mut RustWrite<W>,
    ) -> Self {
        // The nonterminal type needs to be parameterized by all the
        // type parameters that actually appear in the types of
        // nonterminals.  We can't just use *all* type parameters
        // because that would leave unused lifetime/type parameters in
        // some cases.
        let referenced_ty_params: Set<TypeParameter> = grammar
            .types
            .nonterminal_types()
            .into_iter()
            .chain(grammar.types.terminal_types())
            .flat_map(|t| t.referenced())
            .collect();

        let symbol_type_params: Vec<_> = grammar
            .type_parameters
            .iter()
            .filter(|t| referenced_ty_params.contains(t))
            .cloned()
            .collect();

        let mut referenced_where_clauses = Set::new();
        for wc in &grammar.where_clauses {
            wc.map(|ty| {
                if ty
                    .referenced()
                    .iter()
                    .any(|p| symbol_type_params.contains(p))
                {
                    referenced_where_clauses.insert(wc.clone());
                }
            });
        }

        let symbol_where_clauses: Vec<_> = grammar
            .where_clauses
            .iter()
            .filter(|wc| referenced_where_clauses.contains(wc))
            .cloned()
            .collect();

        // Assign each production a unique index to use as the values for reduce
        // actions in the ACTION and EOF_ACTION tables.
        let reduce_indices: Map<&'grammar Production, usize> = grammar
            .nonterminals
            .values()
            .flat_map(|nt| &nt.productions)
            .zip(0..)
            .collect();

        let state_type = {
            // `reduce_indices` are allowed to be +1 since the negative maximum of any integer type
            // is one larger than the positive maximum
            let max_value = ::std::cmp::max(states.len(), reduce_indices.len());
            if max_value <= ::std::i8::MAX as usize {
                "i8"
            } else if max_value <= ::std::i16::MAX as usize {
                "i16"
            } else {
                "i32"
            }
        };

        CodeGenerator::new(
            grammar,
            user_start_symbol,
            start_symbol,
            states,
            out,
            false,
            action_module,
            TableDriven {
                symbol_type_params: symbol_type_params,
                symbol_where_clauses: symbol_where_clauses,
                all_nonterminals: grammar.nonterminals.keys().cloned().collect(),
                reduce_indices: reduce_indices,
                state_type: state_type,
                variant_names: Map::new(),
                variants: Map::new(),
                reduce_functions: Set::new(),
            },
        )
    }

    fn write(&mut self) -> io::Result<()> {
        self.write_parse_mod(|this| {
            try!(this.write_value_type_defn());
            try!(this.write_parse_table());
            try!(this.write_parser_fn());
            try!(this.write_error_recovery_fn());
            try!(this.write_accepts_fn());
            try!(this.emit_reduce_actions());
            try!(this.emit_downcast_fns());
            try!(this.emit_reduce_action_functions());
            Ok(())
        })
    }

    fn write_value_type_defn(&mut self) -> io::Result<()> {
        // sometimes some of the variants are not used, particularly
        // if we are generating multiple parsers from the same file:
        rust!(self.out, "#[allow(dead_code)]");
        rust!(
            self.out,
            "pub enum {}Symbol<{}>",
            self.prefix,
            Sep(", ", &self.custom.symbol_type_params)
        );

        if !self.custom.symbol_where_clauses.is_empty() {
            rust!(
                self.out,
                " where {}",
                Sep(", ", &self.custom.symbol_where_clauses)
            );
        }

        rust!(self.out, " {{");

        // make one variant per terminal
        for term in &self.grammar.terminals.all {
            let ty = self.types.terminal_type(term).clone();
            let len = self.custom.variants.len();
            let name = match self.custom.variants.entry(ty.clone()) {
                Entry::Occupied(entry) => entry.into_mut(),
                Entry::Vacant(entry) => {
                    let name = format!("Variant{}", len);

                    rust!(self.out, "{}({}),", name, ty);
                    entry.insert(name)
                }
            };

            self.custom
                .variant_names
                .insert(Symbol::Terminal(term.clone()), name.clone());
        }

        // make one variant per nonterminal
        for nt in self.grammar.nonterminals.keys() {
            let ty = self.types.nonterminal_type(nt).clone();
            let len = self.custom.variants.len();
            let name = match self.custom.variants.entry(ty.clone()) {
                Entry::Occupied(entry) => entry.into_mut(),
                Entry::Vacant(entry) => {
                    let name = format!("Variant{}", len);

                    rust!(self.out, "{}({}),", name, ty);
                    entry.insert(name)
                }
            };

            self.custom
                .variant_names
                .insert(Symbol::Nonterminal(nt.clone()), name.clone());
        }
        rust!(self.out, "}}");
        Ok(())
    }

    fn write_parse_table(&mut self) -> io::Result<()> {
        // The table is a two-dimensional matrix indexed first by state
        // and then by the terminal index. The value is described above.
        rust!(
            self.out,
            "const {}ACTION: &'static [{}] = &[",
            self.prefix,
            self.custom.state_type
        );

        for (index, state) in self.states.iter().enumerate() {
            rust!(self.out, "// State {}", index);

            if Tls::session().emit_comments {
                for item in state.items.vec.iter() {
                    rust!(self.out, "//     {:?}", item);
                }
            }

            // Write an action for each terminal (either shift, reduce, or error).
            let custom = &self.custom;
            let iterator = self.grammar.terminals.all.iter().map(|terminal| {
                if let Some(new_state) = state.shifts.get(&terminal) {
                    (
                        new_state.0 as i32 + 1,
                        Comment::Goto(Token::Terminal(terminal.clone()), new_state.0),
                    )
                } else {
                    Self::write_reduction(custom, state, &Token::Terminal(terminal.clone()))
                }
            });
            try!(self.out.write_table_row(iterator))
        }

        rust!(self.out, "];");

        // Actions on EOF. Indexed just by state.
        rust!(
            self.out,
            "const {}EOF_ACTION: &'static [{}] = &[",
            self.prefix,
            self.custom.state_type
        );
        for (index, state) in self.states.iter().enumerate() {
            rust!(self.out, "// State {}", index);
            let reduction = Self::write_reduction(&self.custom, state, &Token::EOF);
            try!(self.out.write_table_row(Some(reduction)));
        }
        rust!(self.out, "];");

        // The goto table is indexed by state and *nonterminal*.
        rust!(
            self.out,
            "const {}GOTO: &'static [{}] = &[",
            self.prefix,
            self.custom.state_type
        );
        for (index, state) in self.states.iter().enumerate() {
            rust!(self.out, "// State {}", index);
            let iterator = self.grammar.nonterminals.keys().map(|nonterminal| {
                if let Some(&new_state) = state.gotos.get(&nonterminal) {
                    (
                        new_state.0 as i32 + 1,
                        Comment::Goto(nonterminal, new_state.0),
                    )
                } else {
                    (0, Comment::Error(nonterminal))
                }
            });
            try!(self.out.write_table_row(iterator));
        }
        rust!(self.out, "];");

        try!(self.emit_expected_tokens_fn());

        Ok(())
    }

    fn write_reduction<'s>(
        custom: &TableDriven<'grammar>,
        state: &'s LR1State,
        token: &Token,
    ) -> (i32, Comment<'s, Token>) {
        let reduction = state
            .reductions
            .iter()
            .filter(|&&(ref t, _)| t.contains(token))
            .map(|&(_, p)| p)
            .next();
        if let Some(production) = reduction {
            let action = custom.reduce_indices[production];
            (
                -(action as i32 + 1),
                Comment::Reduce(token.clone(), production),
            )
        } else {
            // Otherwise, this is an error. Store 0.
            (0, Comment::Error(token.clone()))
        }
    }

    fn write_parser_fn(&mut self) -> io::Result<()> {
        let phantom_data_expr = self.phantom_data_expr();

        try!(self.start_parser_fn());

        try!(self.define_tokens());

        // State and data stack.
        rust!(
            self.out,
            "let mut {}states = vec![0_{}];",
            self.prefix,
            self.custom.state_type
        );
        rust!(self.out, "let mut {}symbols = vec![];", self.prefix);

        rust!(self.out, "let mut {}integer;", self.prefix);
        rust!(self.out, "let mut {}lookahead;", self.prefix);
        // The location of the last token is necessary for for error recovery at EOF (or they would not have
        // a location)
        rust!(
            self.out,
            "let {}last_location = &mut Default::default();",
            self.prefix
        );

        // Outer loop: each time we continue around this loop, we
        // shift a new token from the input. We break from the loop
        // when the end of the input is reached (we return early if an
        // error occurs).
        rust!(self.out, "'{}shift: loop {{", self.prefix);

        // Read next token from input.
        try!(self.next_token("lookahead", "tokens", "last_location", "shift"));
        try!(self.token_to_integer("integer", "lookahead"));

        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"pulled next token from input: {{:?}}\", \
                 {p}lookahead);",
                p = self.prefix
            );
            rust!(
                self.out,
                "println!(\"  - integer: {{}}\", \
                 {p}integer);",
                p = self.prefix
            );
        }

        // Loop.
        rust!(self.out, "'{}inner: loop {{", self.prefix);
        rust!(
            self.out,
            "let {}state = *{}states.last().unwrap() as usize;",
            self.prefix,
            self.prefix
        );

        // Load the next action to take.
        rust!(
            self.out,
            "let {}action = {}ACTION[{}state * {} + {}integer];",
            self.prefix,
            self.prefix,
            self.prefix,
            self.grammar.terminals.all.len(),
            self.prefix
        );

        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"state: {{}} lookahead: {{:?}}/{{}} action: {{}} stack-depth: {{}}\", \
                 {p}state, {p}lookahead, {p}integer, {p}action, {p}symbols.len());",
                p = self.prefix
            );
        }

        // Shift.
        rust!(self.out, "if {}action > 0 {{", self.prefix);
        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"--> shift `{{:?}}`\", {p}lookahead);",
                p = self.prefix
            );
        }
        try!(self.token_to_symbol());
        rust!(
            self.out,
            "{}states.push({}action - 1);",
            self.prefix,
            self.prefix
        );
        rust!(
            self.out,
            "{}symbols.push(({}lookahead.0, {}symbol, {}lookahead.2));",
            self.prefix,
            self.prefix,
            self.prefix,
            self.prefix
        );
        rust!(self.out, "continue '{}shift;", self.prefix);

        // Reduce.
        rust!(self.out, "}} else if {}action < 0 {{", self.prefix);
        if DEBUG_PRINT {
            rust!(self.out, "println!(\"--> reduce\");");
        }
        rust!(
            self.out,
            "if let Some(r) = {p}reduce({}{p}action, Some(&{p}lookahead.0), &mut {p}states, &mut \
             {p}symbols, {}) {{",
            self.grammar.user_parameter_refs(),
            phantom_data_expr,
            p = self.prefix
        );
        rust!(self.out, "if r.is_err() {{");
        rust!(self.out, "return r;");
        rust!(self.out, "}}");
        rust!(
            self.out,
            "return Err({}lalrpop_util::ParseError::ExtraToken {{ token: {}lookahead }});",
            self.prefix,
            self.prefix
        );
        rust!(self.out, "}}");

        // Error.
        rust!(self.out, "}} else {{");

        self.try_error_recovery(
            "tokens",
            "states",
            "symbols",
            "last_location",
            Some(("lookahead", "integer", "inner", "shift")),
        )?;

        rust!(self.out, "}}"); // if-else-if-else

        rust!(self.out, "}}"); // reduce loop

        rust!(self.out, "}}"); // shift loop

        // EOF loop
        rust!(self.out, "loop {{");
        rust!(
            self.out,
            "let {}state = *{}states.last().unwrap() as usize;",
            self.prefix,
            self.prefix
        );
        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"EOF loop state: {{}}\", {}state);",
                self.prefix
            );
        }
        rust!(
            self.out,
            "let {}action = {}EOF_ACTION[{}state];",
            self.prefix,
            self.prefix,
            self.prefix
        );
        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"EOF in state {{}} takes action {{}}\", {}state, {}action);",
                self.prefix,
                self.prefix
            );
        }
        rust!(self.out, "if {}action < 0 {{", self.prefix);
        rust!(
            self.out,
            "if let Some(r) = {}reduce({}{}action, None, &mut {}states, &mut {}symbols, {}) {{",
            self.prefix,
            self.grammar.user_parameter_refs(),
            self.prefix,
            self.prefix,
            self.prefix,
            phantom_data_expr
        );
        rust!(self.out, "return r;");
        rust!(self.out, "}}");
        rust!(self.out, "}} else {{");

        self.try_error_recovery("tokens", "states", "symbols", "last_location", None)?;

        rust!(self.out, "}}"); // else

        rust!(self.out, "}}"); // while let

        self.end_parser_fn()
    }

    fn next_token(
        &mut self,
        lookahead: &str,
        tokens: &str,
        last_location: &str,
        break_on_eof: &str,
    ) -> io::Result<()> {
        rust!(
            self.out,
            "{p}{lookahead} = match {p}{tokens}.next() {{",
            lookahead = lookahead,
            tokens = tokens,
            p = self.prefix
        );
        rust!(self.out, "Some(Ok(v)) => v,");
        rust!(self.out, "None => break '{}{},", self.prefix, break_on_eof); // EOF: break out
        if self.grammar.intern_token.is_some() {
            // when we generate the tokenizer, the generated errors are `ParseError` values
            rust!(self.out, "Some(Err(e)) => return Err(e),");
        } else {
            // otherwise, they are user errors
            rust!(
                self.out,
                "Some(Err(e)) => return Err({p}lalrpop_util::ParseError::User {{ error: e }}),",
                p = self.prefix
            );
        }
        rust!(self.out, "}};");
        rust!(
            self.out,
            "*{p}{last_location} = {p}{lookahead}.2.clone();",
            last_location = last_location,
            lookahead = lookahead,
            p = self.prefix
        );
        Ok(())
    }

    fn token_to_integer(&mut self, integer: &str, lookahead: &str) -> io::Result<()> {
        rust!(
            self.out,
            "{p}{integer} = match {p}{lookahead}.1 {{",
            integer = integer,
            lookahead = lookahead,
            p = self.prefix
        );
        for (terminal, index) in self.grammar.terminals.all.iter().zip(0..) {
            if *terminal == TerminalString::Error {
                continue;
            }
            let pattern = self.grammar.pattern(terminal).map(&mut |_| "_");
            rust!(
                self.out,
                "{pattern} if true => {index},",
                pattern = pattern,
                index = index
            );
        }

        rust!(self.out, "_ => {{");
        let prefix = self.prefix;
        try!(self.let_unrecognized_token_error(
            "error",
            &format!("Some({p}{lookahead})", lookahead = lookahead, p = prefix)
        ));
        rust!(self.out, "return Err({p}error);", p = self.prefix);
        rust!(self.out, "}}");

        rust!(self.out, "}};");
        Ok(())
    }

    fn token_to_symbol(&mut self) -> io::Result<()> {
        rust!(
            self.out,
            "let {}symbol = match {}integer {{",
            self.prefix,
            self.prefix
        );
        for (terminal, index) in self.grammar.terminals.all.iter().zip(0..) {
            if *terminal == TerminalString::Error {
                continue;
            }
            rust!(self.out, "{} => match {}lookahead.1 {{", index, self.prefix);

            let mut pattern_names = vec![];
            let pattern = self.grammar.pattern(terminal).map(&mut |_| {
                let index = pattern_names.len();
                pattern_names.push(format!("{}tok{}", self.prefix, index));
                pattern_names.last().cloned().unwrap()
            });

            let mut pattern = format!("{}", pattern);
            if pattern_names.is_empty() {
                pattern_names.push(format!("{}tok", self.prefix));
                pattern = format!("{}tok @ {}", self.prefix, pattern);
            }

            let variant_name = self.variant_name_for_symbol(&Symbol::Terminal(terminal.clone()));
            rust!(
                self.out,
                "{} => {}Symbol::{}(({})),",
                pattern,
                self.prefix,
                variant_name,
                pattern_names.join(", ")
            );
            rust!(self.out, "_ => unreachable!(),");
            rust!(self.out, "}},");
        }

        rust!(self.out, "_ => unreachable!(),");

        rust!(self.out, "}};");
        Ok(())
    }

    fn emit_reduce_actions(&mut self) -> io::Result<()> {
        let success_type = self.types.nonterminal_type(&self.start_symbol);
        let parse_error_type = self.types.parse_error_type();
        let loc_type = self.types.terminal_loc_type();
        let spanned_symbol_type = self.spanned_symbol_type();

        let parameters = vec![
            format!("{}action: {}", self.prefix, self.custom.state_type),
            format!("{}lookahead_start: Option<&{}>", self.prefix, loc_type),
            format!(
                "{}states: &mut ::std::vec::Vec<{}>",
                self.prefix, self.custom.state_type
            ),
            format!(
                "{}symbols: &mut ::std::vec::Vec<{}>",
                self.prefix, spanned_symbol_type
            ),
            format!("_: {}", self.phantom_data_type()),
        ];

        try!(self.out.write_fn_header(
            self.grammar,
            &Visibility::Pub(Some(Path::from_id(Atom::from("crate")))),
            format!("{}reduce", self.prefix),
            vec![],
            None,
            parameters,
            format!("Option<Result<{},{}>>", success_type, parse_error_type),
            vec![]
        ));
        rust!(self.out, "{{");

        rust!(
            self.out,
            "let ({p}pop_states, {p}symbol, {p}nonterminal) = match -{}action {{",
            p = self.prefix
        );
        for (production, index) in self
            .grammar
            .nonterminals
            .values()
            .flat_map(|nt| &nt.productions)
            .zip(1..)
        {
            rust!(self.out, "{} => {{", index);
            // In debug builds LLVM is not very good at reusing stack space which makes this
            // reduce function take up O(number of states) space. By wrapping each reduce action in
            // an immediately called function each reduction takes place in their own function
            // context which ends up reducing the stack space used.

            // Fallible actions and the start symbol may do early returns so we avoid wrapping
            // those
            let is_fallible = self.grammar.action_is_fallible(production.action);
            let reduce_stack_space = !is_fallible && production.nonterminal != self.start_symbol;

            if reduce_stack_space {
                self.custom.reduce_functions.insert(index);
                let phantom_data_expr = self.phantom_data_expr();
                rust!(
                    self.out,
                    "{p}reduce{}({}{p}action, {p}lookahead_start, {p}states, {p}symbols, {})",
                    index,
                    self.grammar.user_parameter_refs(),
                    phantom_data_expr,
                    p = self.prefix
                );
            } else {
                try!(self.emit_reduce_action(production));
            }

            rust!(self.out, "}}");
        }
        rust!(
            self.out,
            "_ => panic!(\"invalid action code {{}}\", {}action)",
            self.prefix
        );
        rust!(self.out, "}};");

        // pop the consumed states from the stack
        rust!(
            self.out,
            "let {p}states_len = {p}states.len();",
            p = self.prefix
        );
        rust!(
            self.out,
            "{p}states.truncate({p}states_len - {p}pop_states);",
            p = self.prefix
        );

        rust!(self.out, "{p}symbols.push({p}symbol);", p = self.prefix);

        rust!(
            self.out,
            "let {}state = *{}states.last().unwrap() as usize;",
            self.prefix,
            self.prefix
        );
        rust!(
            self.out,
            "let {}next_state = {}GOTO[{}state * {} + {}nonterminal] - 1;",
            self.prefix,
            self.prefix,
            self.prefix,
            self.grammar.nonterminals.len(),
            self.prefix
        );
        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"goto state {{}} from {{}} due to nonterminal {{}}\", {}next_state, \
                 {}state, {}nonterminal);",
                self.prefix,
                self.prefix,
                self.prefix
            );
        }
        rust!(
            self.out,
            "{}states.push({}next_state);",
            self.prefix,
            self.prefix
        );
        rust!(self.out, "None");
        rust!(self.out, "}}");
        Ok(())
    }

    fn emit_reduce_action_functions(&mut self) -> io::Result<()> {
        for (production, index) in self
            .grammar
            .nonterminals
            .values()
            .flat_map(|nt| &nt.productions)
            .zip(1..)
        {
            if self.custom.reduce_functions.contains(&index) {
                self.emit_reduce_alternative_fn_header(index)?;
                self.emit_reduce_action(production)?;
                rust!(self.out, "}}");
            }
        }
        Ok(())
    }

    fn emit_reduce_alternative_fn_header(&mut self, index: usize) -> io::Result<()> {
        let loc_type = self.types.terminal_loc_type();
        let spanned_symbol_type = self.spanned_symbol_type();

        let parameters = vec![
            format!("{}action: {}", self.prefix, self.custom.state_type),
            format!("{}lookahead_start: Option<&{}>", self.prefix, loc_type),
            format!(
                "{}states: &mut ::std::vec::Vec<{}>",
                self.prefix, self.custom.state_type
            ),
            format!(
                "{}symbols: &mut ::std::vec::Vec<{}>",
                self.prefix, spanned_symbol_type
            ),
            format!("_: {}", self.phantom_data_type()),
        ];

        try!(self.out.write_fn_header(
            self.grammar,
            &Visibility::Pub(Some(Path::from_id(Atom::from("crate")))),
            format!("{}reduce{}", self.prefix, index),
            vec![],
            None,
            parameters,
            format!("(usize, {}, usize)", spanned_symbol_type,),
            vec![]
        ));
        rust!(self.out, "{{");
        Ok(())
    }

    fn emit_reduce_action(&mut self, production: &Production) -> io::Result<()> {
        rust!(self.out, "// {:?}", production);

        // Pop each of the symbols and their associated states.
        for (index, symbol) in production.symbols.iter().enumerate().rev() {
            let name = self.variant_name_for_symbol(symbol);
            rust!(
                self.out,
                "let {}sym{} = {}pop_{}({}symbols);",
                self.prefix,
                index,
                self.prefix,
                name,
                self.prefix
            );
        }
        let transfer_syms: Vec<_> = (0..production.symbols.len())
            .map(|i| format!("{}sym{}", self.prefix, i))
            .collect();

        // Execute the action fn
        // identify the "start" location for this production; this
        // is typically the start of the first symbol we are
        // reducing; but in the case of an empty production, it
        // will be the last symbol pushed, or at worst `default`.
        if let Some(first_sym) = transfer_syms.first() {
            rust!(
                self.out,
                "let {}start = {}.0.clone();",
                self.prefix,
                first_sym
            );
        } else {
            // we pop no symbols, so grab from the top of the stack
            // (unless we are in the start state, in which case the
            // stack will be empty)
            rust!(
                self.out,
                "let {}start = {}symbols.last().map(|s| s.2.clone()).unwrap_or_default();",
                self.prefix,
                self.prefix
            );
        }

        // identify the "end" location for this production;
        // this is typically the end of the last symbol we are reducing,
        // but in the case of an empty production it will come from the
        // lookahead
        if let Some(last_sym) = transfer_syms.last() {
            rust!(self.out, "let {}end = {}.2.clone();", self.prefix, last_sym);
        } else {
            rust!(
                self.out,
                "let {}end = {}lookahead_start.cloned().unwrap_or_else(|| \
                 {}start.clone());",
                self.prefix,
                self.prefix,
                self.prefix
            );
        }

        let transfered_syms = transfer_syms.len();

        let mut args = transfer_syms;
        if transfered_syms == 0 {
            args.push(format!("&{}start", self.prefix));
            args.push(format!("&{}end", self.prefix));
        }

        // invoke the action code
        let is_fallible = self.grammar.action_is_fallible(production.action);
        if is_fallible {
            rust!(
                self.out,
                "let {}nt = match {}::{}action{}::<{}>({}{}) {{",
                self.prefix,
                self.action_module,
                self.prefix,
                production.action.index(),
                Sep(", ", &self.grammar.non_lifetime_type_parameters()),
                self.grammar.user_parameter_refs(),
                Sep(", ", &args)
            );
            rust!(self.out, "Ok(v) => v,");
            rust!(self.out, "Err(e) => return Some(Err(e)),");
            rust!(self.out, "}};");
        } else {
            rust!(
                self.out,
                "let {}nt = {}::{}action{}::<{}>({}{});",
                self.prefix,
                self.action_module,
                self.prefix,
                production.action.index(),
                Sep(", ", &self.grammar.non_lifetime_type_parameters()),
                self.grammar.user_parameter_refs(),
                Sep(", ", &args)
            );
        }

        // if this is the final state, return it
        if production.nonterminal == self.start_symbol {
            rust!(self.out, "return Some(Ok({}nt));", self.prefix);
            return Ok(());
        }

        // push the produced value on the stack
        let name =
            self.variant_name_for_symbol(&Symbol::Nonterminal(production.nonterminal.clone()));
        rust!(
            self.out,
            "let {}symbol = ({}start, {}Symbol::{}({}nt), {}end);",
            self.prefix,
            self.prefix,
            self.prefix,
            name,
            self.prefix,
            self.prefix
        );

        // produce the index that we will use to extract the next state
        // from GOTO array
        let index = self
            .custom
            .all_nonterminals
            .iter()
            .position(|x| *x == production.nonterminal)
            .unwrap();
        rust!(
            self.out,
            "({len}, {p}symbol, {index})",
            p = self.prefix,
            index = index,
            len = production.symbols.len()
        );

        Ok(())
    }

    fn variant_name_for_symbol(&mut self, s: &Symbol) -> String {
        self.custom.variant_names[s].clone()
    }

    fn emit_downcast_fns(&mut self) -> io::Result<()> {
        for (ty, name) in self.custom.variants.clone() {
            try!(self.emit_downcast_fn(&name, ty));
        }

        Ok(())
    }

    fn emit_downcast_fn(&mut self, variant_name: &str, variant_ty: TypeRepr) -> io::Result<()> {
        let spanned_symbol_type = self.spanned_symbol_type();

        rust!(self.out, "fn {}pop_{}<", self.prefix, variant_name);
        for type_parameter in &self.custom.symbol_type_params {
            rust!(self.out, "  {},", type_parameter);
        }
        rust!(self.out, ">(");
        rust!(
            self.out,
            "{}symbols: &mut ::std::vec::Vec<{}>",
            self.prefix,
            spanned_symbol_type
        );
        rust!(self.out, ") -> {}", self.types.spanned_type(variant_ty));

        if !self.custom.symbol_where_clauses.is_empty() {
            rust!(
                self.out,
                " where {}",
                Sep(", ", &self.custom.symbol_where_clauses)
            );
        }

        rust!(self.out, " {{");

        if DEBUG_PRINT {
            rust!(self.out, "println!(\"pop_{}\");", variant_name);
        }
        rust!(self.out, "match {}symbols.pop().unwrap() {{", self.prefix);
        rust!(
            self.out,
            "({}l, {}Symbol::{}({}v), {}r) => ({}l, {}v, {}r),",
            self.prefix,
            self.prefix,
            variant_name,
            self.prefix,
            self.prefix,
            self.prefix,
            self.prefix,
            self.prefix
        );
        rust!(self.out, "_ => panic!(\"symbol type mismatch\")");
        rust!(self.out, "}}");

        rust!(self.out, "}}");

        Ok(())
    }

    /// Tries to invoke the error recovery function. Takes a bunch of
    /// arguments from the surrounding state:
    ///
    /// - `tokens` -- name of a mut variable of type `I` where `I` is an iterator over tokens
    /// - `symbols` -- name of symbols vector
    /// - `last_location` -- name of `last_location` variable
    /// - `opt_lookahead` -- see below
    ///
    /// The `opt_lookahead` tuple contains the lookahead -- if any --
    /// or None for EOF. It is a 4-tuple: (lookahead, integer,
    /// tok_target, and eof_target). The idea is like this: on entry,
    /// lookahead/integer have values. On exit, if the next token is
    /// EOF, because we have dropped all remaining tokens, we will
    /// `break` to `eof_target`.  Otherwise, we will `continue` to
    /// `tok_target` after storing next token into the variable
    /// `lookahead` and its integer index into `integer`.
    fn try_error_recovery(
        &mut self,
        tokens: &str,
        states: &str,
        symbols: &str,
        last_location: &str,
        opt_lookahead: Option<(&str, &str, &str, &str)>,
    ) -> io::Result<()> {
        if let Some((out_lookahead, out_integer, _, _)) = opt_lookahead {
            rust!(
                self.out,
                "let mut {p}err_lookahead = Some({p}{});",
                out_lookahead,
                p = self.prefix,
            );

            rust!(
                self.out,
                "let mut {p}err_integer: Option<usize> = Some({p}{});",
                out_integer,
                p = self.prefix,
            );
        } else {
            rust!(
                self.out,
                "let mut {p}err_lookahead = None;",
                p = self.prefix,
            );

            rust!(
                self.out,
                "let mut {p}err_integer: Option<usize> = None;",
                p = self.prefix,
            );
        }

        // Easy case: error recovery is disabled. Just error out.
        if !self.grammar.uses_error_recovery {
            let prefix = self.prefix;
            self.let_unrecognized_token_error("error", &format!("{p}err_lookahead", p = prefix))?;
            rust!(self.out, "return Err({p}error)", p = prefix);
            return Ok(());
        }

        let phantom_data_expr = self.phantom_data_expr();

        rust!(
            self.out,
            "match {p}error_recovery(\
             {upr} \
             &mut {p}{tokens}, \
             &mut {p}{states}, \
             &mut {p}{symbols}, \
             {p}{last_location}, \
             &mut {p}err_lookahead, \
             &mut {p}err_integer, \
             {phantom_data_expr}) {{",
            upr = self.grammar.user_parameter_refs(),
            tokens = tokens,
            states = states,
            symbols = symbols,
            last_location = last_location,
            phantom_data_expr = phantom_data_expr,
            p = self.prefix
        );
        rust!(self.out, "Err({p}e) => return Err({p}e),", p = self.prefix);
        rust!(
            self.out,
            "Ok(Some({p}v)) => return Ok({p}v),",
            p = self.prefix
        );
        rust!(self.out, "Ok(None) => (),");
        rust!(self.out, "}}");

        if let Some((out_lookahead, out_integer, tok_target, eof_target)) = opt_lookahead {
            rust!(
                self.out,
                "match ({p}err_lookahead, {p}err_integer) {{",
                p = self.prefix
            );
            rust!(self.out, "(Some({p}l), Some({p}i)) => {{", p = self.prefix);
            rust!(self.out, "{p}{} = {p}l;", out_lookahead, p = self.prefix);
            rust!(self.out, "{p}{} = {p}i;", out_integer, p = self.prefix);
            rust!(self.out, "continue '{p}{};", tok_target, p = self.prefix);
            rust!(self.out, "}}"); // end arm
            rust!(self.out, "_ => break '{p}{},", eof_target, p = self.prefix);
            rust!(self.out, "}}"); // end match
        }

        Ok(())
    }

    fn write_error_recovery_fn(&mut self) -> io::Result<()> {
        // Easy case: error recovery is disabled. Just error out.
        if !self.grammar.uses_error_recovery {
            return Ok(());
        }

        let parse_error_type = self.types.parse_error_type();
        let error_type = self.types.error_type();
        let spanned_symbol_type = self.spanned_symbol_type();
        let triple_type = self.types.triple_type();
        let loc_type = self.types.terminal_loc_type();
        let prefix = self.prefix;
        let actions_per_state = self.grammar.terminals.all.len();
        let start_type = self.types.nonterminal_type(&self.start_symbol);

        // The tokenizr, when we supply it, returns parse
        // errors. Otherwise, it returns custom user errors.
        let tok_error_type = if self.grammar.intern_token.is_some() {
            parse_error_type
        } else {
            &error_type
        };

        let parameters = vec![
            format!("{p}tokens: &mut {p}I", p = self.prefix),
            format!(
                "{p}states: &mut ::std::vec::Vec<{typ}>",
                p = self.prefix,
                typ = self.custom.state_type
            ),
            format!(
                "{p}symbols: &mut ::std::vec::Vec<{spanned_symbol_type}>",
                spanned_symbol_type = spanned_symbol_type,
                p = self.prefix
            ),
            format!(
                "{p}last_location: &mut {loc_type}",
                loc_type = loc_type,
                p = self.prefix
            ),
            format!(
                "{p}opt_lookahead: &mut Option<{triple_type}>",
                triple_type = triple_type,
                p = self.prefix
            ),
            format!("{p}opt_integer: &mut Option<usize>", p = self.prefix),
            format!("_: {}", self.phantom_data_type()),
        ];

        try!(self.out.write_fn_header(
            self.grammar,
            &Visibility::Priv,
            format!("{p}error_recovery", p = self.prefix),
            vec![format!("{p}I", p = self.prefix)],
            None,
            parameters,
            format!(
                "Result<Option<{start_type}>, {parse_error_type}>",
                start_type = start_type,
                parse_error_type = parse_error_type
            ),
            vec![format!(
                "{p}I: Iterator<Item = \
                 Result<{triple_type}, {tok_error_type}>\
                 >",
                triple_type = triple_type,
                tok_error_type = tok_error_type,
                p = self.prefix
            ),]
        ));

        rust!(self.out, "{{");

        self.let_unrecognized_token_error(
            "error",
            &format!("{p}opt_lookahead.clone()", p = prefix),
        )?;

        rust!(self.out, "let mut {}dropped_tokens = vec![];", prefix);

        let phantom_data_expr = self.phantom_data_expr();

        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"Initiating error recovery in state: {{}}\", \
                 {p}states.last().unwrap());",
                p = self.prefix
            );
            rust!(
                self.out,
                "println!(\"  - state stack size: {{}}\", \
                 {p}states.len());",
                p = self.prefix
            );
            rust!(
                self.out,
                "println!(\"  - symbol stack size: {{}}\", \
                 {p}symbols.len());",
                p = self.prefix
            );
            rust!(
                self.out,
                "println!(\"  - opt lookahead: {{:?}}\", \
                 {p}opt_lookahead);",
                p = self.prefix
            );
            rust!(
                self.out,
                "println!(\"  - opt integer: {{:?}}\", \
                 {p}opt_integer);",
                p = self.prefix
            );
        }

        // We are going to insert ERROR into the lookahead. So, first,
        // perform all reductions from current state triggered by having
        // ERROR in the lookahead.
        rust!(self.out, "loop {{");
        rust!(
            self.out,
            "let {p}state = *{p}states.last().unwrap() as usize;",
            p = self.prefix
        );

        // Access the action with `error` as the lookahead; it is always final
        // column in the row for this state
        rust!(
            self.out,
            "let {p}action = {p}ACTION[{p}state * {} + {}];",
            actions_per_state,
            actions_per_state - 1,
            p = self.prefix
        );
        rust!(self.out, "if {p}action >= 0 {{", p = self.prefix);
        rust!(self.out, "break;");
        rust!(self.out, "}}");

        if DEBUG_PRINT {
            rust!(
                self.out,
                r#"println!("Error recovery reduces on action: {{}}", {}action);"#,
                self.prefix
            );
        }

        rust!(
            self.out,
            "let {p}lookahead_start = {p}opt_lookahead.as_ref().map(|l| &l.0);",
            p = self.prefix
        );
        rust!(
            self.out,
            "if let Some(r) = {p}reduce( \
             {upr} \
             {p}action, \
             {p}lookahead_start, \
             {p}states, \
             {p}symbols, \
             {phantoms} \
             ) {{",
            upr = self.grammar.user_parameter_refs(),
            phantoms = phantom_data_expr,
            p = self.prefix
        );
        rust!(self.out, "return Ok(Some(r?));");
        rust!(self.out, "}}");
        rust!(self.out, "}}"); // end reduce loop

        // Now try to find the recovery state.

        rust!(
            self.out,
            "let {p}states_len = {p}states.len();",
            p = self.prefix
        );

        // I'd rather generate `let top = loop {{...}}` but I do not
        // to retain compatibility with Rust 1.16.0.
        rust!(self.out, "let {p}top0;", p = self.prefix);

        rust!(self.out, "'{p}find_state: loop {{", p = self.prefix);

        // Go backwards through the states...
        rust!(
            self.out,
            "for {p}top in (0..{p}states_len).rev() {{",
            p = self.prefix
        );
        rust!(
            self.out,
            "let {p}state = {p}states[{p}top] as usize;",
            p = self.prefix
        );
        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"Probing recovery from state {{}} (top = {{}}).\", {p}state, {p}top);",
                p = self.prefix,
            );
        }
        // ...fetch action for error token...
        rust!(
            self.out,
            "let {p}action = {p}ACTION[{p}state * {} + {}];",
            actions_per_state,
            actions_per_state - 1,
            p = self.prefix
        );
        // ...if action is error or reduce, go to next state...
        rust!(
            self.out,
            "if {p}action <= 0 {{ continue; }}",
            p = self.prefix
        );
        // ...otherwise, action *must* be shift. That would take us into `error_state`.
        rust!(
            self.out,
            "let {p}error_state = {p}action - 1;",
            p = self.prefix
        );
        // If `error_state` can accept this lookahead, we are done.
        rust!(
            self.out,
            "if {p}accepts(\
             {upr} \
             {p}error_state, \
             &{p}states[..{p}top + 1], \
             *{p}opt_integer, \
             {phantoms},\
             ) {{",
            upr = self.grammar.user_parameter_refs(),
            phantoms = phantom_data_expr,
            p = self.prefix
        );
        rust!(self.out, "{p}top0 = {p}top;", p = self.prefix);
        rust!(self.out, "break '{p}find_state;", p = self.prefix);
        rust!(self.out, "}}"); // end if
        rust!(self.out, "}}"); // end for

        // Otherwise, if we did not find any enclosing state that can
        // error and then accept this lookahead, we need to drop the current token.

        // Introduce an artificial loop here so we can break to
        // it. This is a hack to re-use the `next_token` function.
        rust!(self.out, "'{p}eof: loop {{", p = self.prefix);
        rust!(
            self.out,
            "match {p}opt_lookahead.take() {{",
            p = self.prefix
        );

        // If the lookahead is EOF, and there is no suitable state to
        // recover to, we just have to abort EOF recovery. Find the
        // first token that we dropped (if any) and use that as the
        // point of error.
        rust!(self.out, "None => {{");
        if DEBUG_PRINT {
            rust!(
                self.out,
                r#"println!("Error recovery: cannot drop EOF; aborting");"#
            );
        }
        rust!(self.out, "return Err({}error)", prefix);
        rust!(self.out, "}}"); // end None arm

        // Else, drop the current token and shift to the next. If there is a next
        // token, we will `continue` to the start of the `'find_state` loop.
        rust!(self.out, "Some(mut {p}lookahead) => {{", p = self.prefix);
        if DEBUG_PRINT {
            rust!(
                self.out,
                r#"println!("Error recovery: dropping token `{{:?}}`", {p}lookahead);"#,
                p = self.prefix,
            );
        }
        rust!(
            self.out,
            "{p}dropped_tokens.push({p}lookahead);",
            p = self.prefix
        );
        self.next_token("lookahead", "tokens", "last_location", "eof")?;
        rust!(self.out, "let {p}integer;", p = self.prefix);
        try!(self.token_to_integer("integer", "lookahead"));
        rust!(
            self.out,
            "*{p}opt_lookahead = Some({p}lookahead);",
            p = self.prefix
        );
        rust!(
            self.out,
            "*{p}opt_integer = Some({p}integer);",
            p = self.prefix
        );
        rust!(self.out, "continue '{p}find_state;", p = self.prefix);
        rust!(self.out, "}}"); // end Some(_) arm
        rust!(self.out, "}}"); // end match
        rust!(self.out, "}}"); // end 'eof loop

        // The `next_token` function will break here (out of the
        // `'eof` loop) when we encounter EOF (i.e., there is no
        // `next_token`). Just set `opt_lookahead` to `None` in that
        // case.
        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"Encountered EOF during error recovery\");"
            );
        }
        rust!(self.out, "*{p}opt_lookahead = None;", p = self.prefix);
        rust!(self.out, "*{p}opt_integer = None;", p = self.prefix);
        rust!(self.out, "}};"); // end 'find_state loop

        // If we get here, we are ready to push the error recovery state.

        // We have to compute the span for the error recovery
        // token. We do this first, before we pop any symbols off the
        // stack. There are several possibilities, in order of
        // preference.
        //
        // For the **start** of the message, we prefer to use the start of any
        // popped states. This represents parts of the input we had consumed but
        // had to roll back and ignore.
        //
        // Example:
        //
        //       a + (b + /)
        //              ^ start point is here, since this `+` will be popped off
        //
        // If there are no popped states, but there *are* dropped tokens, we can use
        // the start of those.
        //
        // Example:
        //
        //       a + (b + c e)
        //                  ^ start point would be here
        //
        // Finally, if there are no popped states *nor* dropped tokens, we can use
        // the end of the top-most state.

        rust!(self.out, "let {p}top = {p}top0;", p = self.prefix);
        rust!(
            self.out,
            "let {p}start = if let Some({p}popped_sym) = {p}symbols.get({p}top) {{",
            p = self.prefix
        );
        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"Span starts from popped symbol {{:?}}\", \
                 (&{p}popped_sym.0 .. &{p}popped_sym.2));",
                p = self.prefix,
            );
        }
        rust!(self.out, "{p}popped_sym.0.clone()", p = self.prefix);
        rust!(
            self.out,
            "}} else if let Some({p}dropped_token) = {p}dropped_tokens.first() {{",
            p = self.prefix
        );
        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"Span starts from dropped token {{:?}}\", \
                 (&{p}dropped_token.0 .. &{p}dropped_token.2));",
                p = self.prefix,
            );
        }
        rust!(self.out, "{p}dropped_token.0.clone()", p = self.prefix);
        rust!(self.out, "}} else if {p}top > 0 {{", p = self.prefix);
        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"Span starts from end of last retained symbol\");"
            );
        }
        rust!(
            self.out,
            "{p}symbols[{p}top - 1].2.clone()",
            p = self.prefix
        );
        rust!(self.out, "}} else {{");
        if DEBUG_PRINT {
            rust!(self.out, "println!(\"Span starts from default\");");
        }
        rust!(self.out, "Default::default()");
        rust!(self.out, "}};"); // end if

        // For the end span, here are the possibilities:
        //
        // We prefer to use the end of the last dropped token.
        //
        // Examples:
        //
        //       a + (b + /)
        //              ---
        //       a + (b c)
        //              -
        //
        // But, if there are no dropped tokens, we will use the end of the popped states,
        // if any:
        //
        //       a + /
        //         -
        //
        // If there are neither dropped tokens *or* popped states,
        // then the user is simulating insertion of an operator. In
        // this case, we prefer the start of the lookahead, but
        // fallback to the start if we are at EOF.
        //
        // Examples:
        //
        //       a + (b c)
        //             -
        rust!(
            self.out,
            "let {p}end = if let Some({p}dropped_token) = {p}dropped_tokens.last() {{",
            p = self.prefix
        );
        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"Span ends at end of last dropped token {{:?}}\", \
                 (&{p}dropped_token.0 .. &{p}dropped_token.2));",
                p = self.prefix,
            );
        }
        rust!(self.out, "{p}dropped_token.2.clone()", p = self.prefix);
        rust!(
            self.out,
            "}} else if {p}states_len - 1 > {p}top {{",
            p = self.prefix
        );
        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"Span ends at end of last popped symbol {{:?}}\", \
                 {p}symbols.last().unwrap().2);",
                p = self.prefix,
            );
        }
        rust!(
            self.out,
            "{p}symbols.last().unwrap().2.clone()",
            p = self.prefix
        );
        rust!(
            self.out,
            "}} else if let Some({p}lookahead) = {p}opt_lookahead.as_ref() {{",
            p = self.prefix
        );
        if DEBUG_PRINT {
            rust!(self.out, "println!(\"Span ends at start of lookahead\");");
        }
        rust!(self.out, "{p}lookahead.0.clone()", p = self.prefix);
        rust!(self.out, "}} else {{");
        if DEBUG_PRINT {
            rust!(self.out, "println!(\"Span ends at start\");");
        }
        rust!(self.out, "{p}start.clone()", p = self.prefix);
        rust!(self.out, "}};"); // end if

        // First we have to pop off the states we are skipping. Note
        // that the bottom-most state doesn't have a symbol, so the
        // symbols vector is always 1 shorter, hence we truncate its
        // length to `{p}top` not `{p}top + 1`.
        rust!(self.out, "{p}states.truncate({p}top + 1);", p = self.prefix);
        rust!(self.out, "{p}symbols.truncate({p}top);", p = self.prefix);

        // Now load the new top state.
        rust!(
            self.out,
            "let {p}recover_state = {p}states[{p}top] as usize;",
            p = self.prefix
        );

        // Load the error action, which must be a shift.
        rust!(
            self.out,
            "let {p}error_action = {p}ACTION[{p}recover_state * {} + {}];",
            actions_per_state,
            actions_per_state - 1,
            p = self.prefix
        );
        rust!(
            self.out,
            "let {p}error_state = {p}error_action - 1;",
            p = self.prefix
        );

        if DEBUG_PRINT {
            rust!(self.out, "println!(\"Recovering from error:\");");
            rust!(
                self.out,
                "println!(\"  - recovery base state: {{}}\", {p}top);",
                p = self.prefix
            );
            rust!(
                self.out,
                "println!(\"  - new top state {{}}\", {p}recover_state);",
                p = self.prefix
            );
            rust!(
                self.out,
                "println!(\"  - error state {{}}\", {p}error_state);",
                p = self.prefix
            );
            rust!(
                self.out,
                "println!(\"  - new stack length: {{}}\", {p}states.len());",
                p = self.prefix
            );
            rust!(
                self.out,
                "println!(\"  - new symbol length: {{}}\", {p}symbols.len());",
                p = self.prefix
            );
            rust!(
                self.out,
                "println!(\"  - span {{:?}}..{{:?}}\", {p}start, {p}end);",
                p = self.prefix
            );
        }

        // Push the error state onto the stack.
        rust!(self.out, "{p}states.push({p}error_state);", p = self.prefix);
        rust!(
            self.out,
            "let {p}recovery = {}lalrpop_util::ErrorRecovery {{",
            p = self.prefix
        );
        rust!(self.out, "error: {p}error,", p = self.prefix);
        rust!(
            self.out,
            "dropped_tokens: {p}dropped_tokens,",
            p = self.prefix
        );
        rust!(self.out, "}};");

        let error_variant = self.variant_name_for_symbol(&Symbol::Terminal(TerminalString::Error));
        rust!(
            self.out,
            "{p}symbols.push(({p}start, {p}Symbol::{e}({p}recovery), {p}end));",
            p = self.prefix,
            e = error_variant
        );

        rust!(self.out, "Ok(None)");
        rust!(self.out, "}}"); // end fn
        Ok(())
    }

    /// The `accepts` function
    ///
    /// ```ignore
    /// fn __accepts() {
    ///     error_state: i32,
    ///     states: &Vec<i32>,
    ///     opt_integer: Option<usize>,
    /// ) -> bool {
    ///     ...
    /// }
    /// ```
    ///
    /// has the job of figuring out whether the given error state would
    /// "accept" the given lookahead. We basically trace through the LR
    /// automaton looking for one of two outcomes:
    ///
    /// - the lookahead is eventually shifted
    /// - we reduce to the end state successfully (in the case of EOF).
    ///
    /// If we used the pure LR(1) algorithm, we wouldn't need this
    /// function, because we would be guaranteed to error immediately
    /// (and not after some number of reductions). But with an LALR
    /// (or Lane Table) generated automaton, it is possible to reduce
    /// some number of times before encountering an error. Failing to
    /// take this into account can lead error recovery into an
    /// infinite loop (see the `error_recovery_lalr_loop` test) or
    /// produce crappy results (see `error_recovery_lock_in`).
    fn write_accepts_fn(&mut self) -> io::Result<()> {
        if !self.grammar.uses_error_recovery {
            return Ok(());
        }

        let actions_per_state = self.grammar.terminals.all.len();
        let parameters = vec![
            format!(
                "{p}error_state: {typ}",
                p = self.prefix,
                typ = self.custom.state_type
            ),
            format!(
                "{p}states: & [{typ}]",
                p = self.prefix,
                typ = self.custom.state_type
            ),
            format!("{p}opt_integer: Option<usize>", p = self.prefix),
            format!("_: {}", self.phantom_data_type()),
        ];

        try!(self.out.write_fn_header(
            self.grammar,
            &Visibility::Priv,
            format!("{}accepts", self.prefix),
            vec![],
            None,
            parameters,
            format!("bool"),
            vec![]
        ));
        rust!(self.out, "{{");

        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"Testing whether state {{}} accepts token {{:?}}\", \
                 {p}error_state, {p}opt_integer);",
                p = self.prefix
            );
        }

        // Create our own copy of the state stack to play with.
        rust!(
            self.out,
            "let mut {p}states = {p}states.to_vec();",
            p = self.prefix
        );
        rust!(self.out, "{p}states.push({p}error_state);", p = self.prefix);

        rust!(self.out, "loop {{",);

        rust!(
            self.out,
            "let mut {}states_len = {}states.len();",
            self.prefix,
            self.prefix
        );

        rust!(
            self.out,
            "let {p}top = {p}states[{p}states_len - 1] as usize;",
            p = self.prefix
        );

        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"accepts: top-state={{}} num-states={{}}\", {p}top, {p}states_len);",
                p = self.prefix
            );
        }

        rust!(
            self.out,
            "let {p}action = match {p}opt_integer {{",
            p = self.prefix
        );
        rust!(
            self.out,
            "None => {p}EOF_ACTION[{p}top as usize],",
            p = self.prefix
        );
        rust!(
            self.out,
            "Some({p}integer) => {p}ACTION[{p}top * {actions_per_state} + {p}integer],",
            p = self.prefix,
            actions_per_state = actions_per_state,
        );
        rust!(self.out, "}};"); // end `match`

        // If we encounter an error action, we do **not** accept.
        rust!(
            self.out,
            "if {p}action == 0 {{ return false; }}",
            p = self.prefix
        );

        // If we encounter a shift action, we DO accept.
        rust!(
            self.out,
            "if {p}action > 0 {{ return true; }}",
            p = self.prefix
        );

        // If we encounter a reduce action, we need to simulate its
        // effect on the state stack.
        rust!(
            self.out,
            "let ({p}to_pop, {p}nt) = match -{p}action {{",
            p = self.prefix
        );
        for (production, index) in self
            .grammar
            .nonterminals
            .values()
            .flat_map(|nt| &nt.productions)
            .zip(1..)
        {
            if Tls::session().emit_comments {
                rust!(self.out, "// simulate {:?}", production);
            }

            // if we just reduced the start symbol, that is also an accept criteria
            if production.nonterminal == self.start_symbol {
                rust!(self.out, "{} => return true,", index);
            } else {
                let num_symbols = production.symbols.len();
                let nt = self
                    .custom
                    .all_nonterminals
                    .iter()
                    .position(|x| *x == production.nonterminal)
                    .unwrap();
                rust!(self.out, "{} => {{", index);
                if DEBUG_PRINT {
                    rust!(
                        self.out,
                        "println!(r##\"accepts: simulating {:?}\"##);",
                        production
                    );
                }
                rust!(
                    self.out,
                    "({num_symbols}, {nt})",
                    num_symbols = num_symbols,
                    nt = nt
                );
                rust!(self.out, "}}");
            }
        }
        rust!(
            self.out,
            "_ => panic!(\"invalid action code {{}}\", {}action)",
            self.prefix
        );
        rust!(self.out, "}};"); // end match

        rust!(self.out, "{p}states_len -= {p}to_pop;", p = self.prefix);
        rust!(
            self.out,
            "{p}states.truncate({p}states_len);",
            p = self.prefix
        );
        rust!(
            self.out,
            "let {p}top = {p}states[{p}states_len - 1] as usize;",
            p = self.prefix
        );

        if DEBUG_PRINT {
            rust!(
                self.out,
                "println!(\"accepts: popped {{}} symbols, new top is {{}}, nt is {{}}\", \
                 {p}to_pop, \
                 {p}top, \
                 {p}nt, \
                 );",
                p = self.prefix
            );
        }

        rust!(
            self.out,
            "let {p}next_state = {p}GOTO[{p}top * {num_non_terminals} + {p}nt] - 1;",
            p = self.prefix,
            num_non_terminals = self.grammar.nonterminals.len(),
        );

        rust!(self.out, "{p}states.push({p}next_state);", p = self.prefix);

        rust!(self.out, "}}"); // end loop
        rust!(self.out, "}}"); // end fn

        Ok(())
    }

    fn symbol_type(&self) -> String {
        format!(
            "{}Symbol<{}>",
            self.prefix,
            Sep(", ", &self.custom.symbol_type_params)
        )
    }

    fn spanned_symbol_type(&self) -> String {
        let loc_type = self.types.terminal_loc_type();
        format!("({},{},{})", loc_type, self.symbol_type(), loc_type)
    }

    fn let_unrecognized_token_error(&mut self, error_var: &str, token: &str) -> io::Result<()> {
        rust!(
            self.out,
            "let {}state = *{}states.last().unwrap() as usize;",
            self.prefix,
            self.prefix
        );
        rust!(
            self.out,
            "let {}{} = {}lalrpop_util::ParseError::UnrecognizedToken {{",
            self.prefix,
            error_var,
            self.prefix
        );
        rust!(self.out, "token: {},", token);
        rust!(
            self.out,
            "expected: {}expected_tokens({}state),",
            self.prefix,
            self.prefix
        );
        rust!(self.out, "}};");
        Ok(())
    }

    fn emit_expected_tokens_fn(&mut self) -> io::Result<()> {
        rust!(
            self.out,
            "fn {}expected_tokens({}state: usize) -> Vec<::std::string::String> {{",
            self.prefix,
            self.prefix
        );

        rust!(
            self.out,
            "const {}TERMINAL: &'static [&'static str] = &[",
            self.prefix
        );
        let all_terminals = if self.grammar.uses_error_recovery {
            // Subtract one to exlude the error terminal
            &self.grammar.terminals.all[..self.grammar.terminals.all.len() - 1]
        } else {
            &self.grammar.terminals.all
        };
        for terminal in all_terminals {
            // Three # should hopefully be enough to prevent any
            // reasonable terminal from escaping the literal
            rust!(self.out, "r###\"{}\"###,", terminal);
        }
        rust!(self.out, "];");

        // Grab any terminals in the current state which would have resulted in a successful parse
        rust!(
            self.out,
            "{}ACTION[({}state * {})..].iter().zip({}TERMINAL).filter_map(|(&state, terminal)| {{",
            self.prefix,
            self.prefix,
            self.grammar.terminals.all.len(),
            self.prefix
        );
        rust!(self.out, "if state == 0 {{");
        rust!(self.out, "None");
        rust!(self.out, "}} else {{");
        rust!(self.out, "Some(terminal.to_string())");
        rust!(self.out, "}}");
        rust!(self.out, "}}).collect()");
        rust!(self.out, "}}");
        Ok(())
    }
}
