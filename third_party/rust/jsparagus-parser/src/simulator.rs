//! Simulates parser execution, for a single token of input, without incurring
//! any side effects.
//!
//! This is basically a copy of the parser.rs source code with calls to
//! generated_parser::reduce, and stack bookkeeping, omitted.

use crate::parser::Parser;
use arrayvec::ArrayVec;
use ast::SourceLocation;
use generated_parser::{
    noop_actions, ParseError, ParserTrait, Result, StackValue, TermValue, TerminalId, Token, TABLES,
};

/// The Simulator is used to check whether we can shift one token, either to
/// check what might be accepted, or to check whether we can End parsing now.
/// This is used by the REPL to verify whether or not we can end the input.
pub struct Simulator<'alloc, 'parser> {
    /// Define the top of the immutable stack.
    sp: usize,
    /// Immutable state stack coming from the forked parser.
    state_stack: &'parser [usize],
    /// Immuatable term stack coming from the forked parser.
    node_stack: &'parser [TermValue<StackValue<'alloc>>],
    /// Mutable state stack used by the simulator on top of the immutable
    /// parser's state stack.
    ///
    /// Uses a fixed-size array as the number of lookahead is bounded to a lower
    /// value, panics otherwise.
    sim_state_stack: ArrayVec<usize, 4>,
    /// Mutable term stack used by the simulator on top of the immutable
    /// parser's term stack.
    ///
    /// Uses a fixed-size array as the number of lookahead is bounded to a lower
    /// value, panics otherwise.
    sim_node_stack: ArrayVec<TermValue<()>, 4>,
    /// Mutable term stack used by the simulator for replaying terms when
    /// reducing non-terminals are replaying lookahead terminals.
    ///
    /// Uses a fixed-size array as the number of lookahead is bounded to a lower
    /// value, panics otherwise.
    replay_stack: ArrayVec<TermValue<()>, 4>,
}

impl<'alloc, 'parser> ParserTrait<'alloc, ()> for Simulator<'alloc, 'parser> {
    fn shift(&mut self, tv: TermValue<()>) -> Result<'alloc, bool> {
        // Shift the new terminal/nonterminal and its associated value.
        let mut state = self.state();
        assert!(state < TABLES.shift_count);
        let mut tv = tv;
        loop {
            let term_index: usize = tv.term.into();
            assert!(term_index < TABLES.shift_width);
            let index = state * TABLES.shift_width + term_index;
            let goto = TABLES.shift_table[index];
            if goto < 0 {
                // Error handling is in charge of shifting an ErrorSymbol from the
                // current state.
                self.try_error_handling(tv)?;
                tv = self.replay_stack.pop().unwrap();
                continue;
            }
            state = goto as usize;
            self.sim_state_stack.push(state);
            self.sim_node_stack.push(tv);
            // Execute any actions, such as reduce actions.
            if state >= TABLES.shift_count {
                assert!(state < TABLES.action_count + TABLES.shift_count);
                if noop_actions(self, state)? {
                    return Ok(true);
                }
                state = self.state();
            }
            assert!(state < TABLES.shift_count);
            if let Some(tv_temp) = self.replay_stack.pop() {
                tv = tv_temp;
            } else {
                break;
            }
        }
        Ok(false)
    }
    fn shift_replayed(&mut self, state: usize) {
        let tv = self.replay_stack.pop().unwrap();
        self.sim_state_stack.push(state);
        self.sim_node_stack.push(tv);
    }
    fn unshift(&mut self) {
        let tv = self.pop();
        self.replay(tv)
    }
    fn pop(&mut self) -> TermValue<()> {
        if let Some(s) = self.sim_node_stack.pop() {
            self.sim_state_stack.pop();
            return s;
        }
        let t = self.node_stack[self.sp - 1].term;
        self.sp -= 1;
        TermValue { term: t, value: () }
    }
    fn replay(&mut self, tv: TermValue<()>) {
        self.replay_stack.push(tv)
    }
    fn epsilon(&mut self, state: usize) {
        if self.sim_state_stack.is_empty() {
            self.sim_state_stack.push(self.state_stack[self.sp]);
            self.sim_node_stack.push(TermValue {
                term: self.node_stack[self.sp - 1].term,
                value: (),
            });
            self.sp -= 1;
        }
        *self.sim_state_stack.last_mut().unwrap() = state;
    }
    fn top_state(&self) -> usize {
        self.state()
    }
    fn check_not_on_new_line(&mut self, _peek: usize) -> Result<'alloc, bool> {
        Ok(true)
    }
}

impl<'alloc, 'parser> Simulator<'alloc, 'parser> {
    pub fn new(
        state_stack: &'parser [usize],
        node_stack: &'parser [TermValue<StackValue<'alloc>>],
    ) -> Simulator<'alloc, 'parser> {
        let sp = state_stack.len() - 1;
        assert_eq!(state_stack.len(), node_stack.len() + 1);
        Simulator {
            sp,
            state_stack,
            node_stack,
            sim_state_stack: ArrayVec::new(),
            sim_node_stack: ArrayVec::new(),
            replay_stack: ArrayVec::new(),
        }
    }

    fn state(&self) -> usize {
        if let Some(res) = self.sim_state_stack.last() {
            *res
        } else {
            self.state_stack[self.sp]
        }
    }

    pub fn write_token(&mut self, t: TerminalId) -> Result<'alloc, ()> {
        // Shift the token with the associated StackValue.
        let accept = self.shift(TermValue {
            term: t.into(),
            value: (),
        })?;
        // JavaScript grammar accepts empty inputs, therefore we can never
        // accept any program before receiving a TerminalId::End.
        assert!(!accept);
        Ok(())
    }

    pub fn close(&mut self, _position: usize) -> Result<'alloc, ()> {
        // Shift the End terminal with the associated StackValue.
        let accept = self.shift(TermValue {
            term: TerminalId::End.into(),
            value: (),
        })?;
        // Adding a TerminalId::End would either lead to a parse error, or to
        // accepting the current input. In which case we return matching node
        // value.
        assert!(accept);

        // We can either reduce a Script/Module, or a Script/Module followed by
        // an <End> terminal.
        assert!(self.sp + self.sim_node_stack.len() >= 1);
        Ok(())
    }

    // Simulate the action of Parser::try_error_handling.
    fn try_error_handling(&mut self, t: TermValue<()>) -> Result<'alloc, bool> {
        if t.term.is_terminal() {
            let term = t.term.to_terminal();
            let bogus_loc = SourceLocation::new(0, 0);
            let token = &Token::basic_token(term, bogus_loc);

            // Error tokens might them-self cause more errors to be reported.
            // This happens due to the fact that the ErrorToken can be replayed,
            // and while the ErrorToken might be in the lookahead rules, it
            // might not be in the shifted terms coming after the reduced
            // nonterminal.
            if term == TerminalId::ErrorToken {
                return Err(Parser::parse_error(token).into());
            }

            // Otherwise, check if the current rule accept an Automatic
            // Semi-Colon insertion (ASI).
            let state = self.state();
            assert!(state < TABLES.shift_count);
            let error_code = TABLES.error_codes[state];
            if let Some(error_code) = error_code {
                Parser::recover(token, error_code)?;
                self.replay(t);
                self.replay(TermValue {
                    term: TerminalId::ErrorToken.into(),
                    value: (),
                });
                return Ok(false);
            }
            return Err(Parser::parse_error(token).into());
        }
        // On error, don't attempt error handling again.
        Err(ParseError::ParserCannotUnpackToken.into())
    }
}
