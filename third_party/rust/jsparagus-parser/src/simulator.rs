//! Simulates parser execution, for a single token of input, without incurring
//! any side effects.
//!
//! This is basically a copy of the parser.rs source code with calls to
//! generated_parser::reduce, and stack bookkeeping, omitted.

use crate::parser::{Action, Parser};
use ast::SourceLocation;
use generated_parser::{ErrorCode, Result, TerminalId, Token, TABLES};

pub struct Simulator<'parser> {
    sp: usize,
    state: usize,
    state_stack: &'parser [usize],
}

impl<'parser> Simulator<'parser> {
    pub fn new(state_stack: &'parser [usize]) -> Simulator {
        let sp = state_stack.len() - 1;
        Simulator {
            sp,
            state: state_stack[sp],
            state_stack,
        }
    }

    fn action(&self, t: TerminalId) -> Action {
        Parser::action_at_state(t, self.state)
    }

    // Simulate the action of Parser::reduce_all without calling any AstBuilder
    // methods or modifying the Parser state. Naturally, "early errors"
    // detected by AstBuilder methods are not caught.
    fn reduce_all(&mut self, t: TerminalId) -> Action {
        let mut action = self.action(t);
        while action.is_reduce() {
            let prod_index = action.reduce_prod_index();
            let (num_pops, nt) = TABLES.reduce_simulator[prod_index];
            debug_assert!((nt as usize) < TABLES.goto_width);
            self.sp -= num_pops;
            let prev_state = self.state_stack[self.sp];
            let state_after =
                TABLES.goto_table[prev_state * TABLES.goto_width + nt as usize] as usize;
            debug_assert!(state_after < TABLES.state_count);
            self.sp += 1;
            self.state = state_after;
            action = self.action(t);
        }
        action
    }

    pub fn write_token<'alloc>(mut self, token: &Token<'alloc>) -> Result<'alloc, ()> {
        // Loop for error-handling. The normal path through this code reaches
        // the `return` statement.
        loop {
            let action = self.reduce_all(token.terminal_id);
            if action.is_shift() {
                return Ok(());
            } else {
                assert!(action.is_error());
                self.try_error_handling(token)?;
            }
        }
    }

    pub fn close(mut self, position: usize) -> Result<'static, ()> {
        // Loop for error-handling.
        loop {
            let action = self.reduce_all(TerminalId::End);
            if action.is_accept() {
                assert_eq!(self.sp, 1);
                return Ok(());
            } else {
                assert!(action.is_error());
                let loc = SourceLocation::new(position, position);
                self.try_error_handling(&Token::basic_token(TerminalId::End, loc))?;
            }
        }
    }

    // Simulate the action of Parser::try_error_handling.
    fn try_error_handling<'alloc>(&mut self, t: &Token<'alloc>) -> Result<'alloc, ()> {
        assert!(t.terminal_id != TerminalId::ErrorToken);

        let action = self.reduce_all(TerminalId::ErrorToken);
        if action.is_shift() {
            let error_code = TABLES.error_codes[self.state]
                .as_ref()
                .expect("state that accepts an ErrorToken must have an error_code")
                .clone();

            self.recover(t, error_code, action.shift_state())
        } else {
            assert!(action.is_error());
            Parser::parse_error(t)
        }
    }

    fn recover<'alloc>(
        &mut self,
        t: &Token<'alloc>,
        error_code: ErrorCode,
        next_state: usize,
    ) -> Result<'alloc, ()> {
        match error_code {
            ErrorCode::Asi => {
                if t.is_on_new_line
                    || t.terminal_id == TerminalId::End
                    || t.terminal_id == TerminalId::CloseBrace
                {
                    // Move to the recovered state.
                    self.state = next_state;
                    Ok(())
                } else {
                    Parser::parse_error(t)
                }
            }
            ErrorCode::DoWhileAsi => {
                self.state = next_state;
                Ok(())
            }
        }
    }
}
