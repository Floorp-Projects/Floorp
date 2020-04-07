use crate::simulator::Simulator;
use ast::SourceLocation;
use generated_parser::{
    full_actions, AstBuilder, AstBuilderDelegate, ErrorCode, ParseError, ParserTrait, Result,
    StackValue, Term, TermValue, TerminalId, Token, TABLES,
};
use json_log::json_trace;

pub struct Parser<'alloc> {
    /// Vector of states visited in the LR parse table.
    state_stack: Vec<usize>,
    /// Vector of terms and their associated values.
    node_stack: Vec<TermValue<StackValue<'alloc>>>,
    /// Vector of lookahead terms and their associated value, to be emptied by
    /// pop-ing elements from it before shifting any new terminals.
    replay_stack: Vec<TermValue<StackValue<'alloc>>>,
    /// Build the AST stored in the TermValue vectors.
    handler: AstBuilder<'alloc>,
}

impl<'alloc> AstBuilderDelegate<'alloc> for Parser<'alloc> {
    fn ast_builder_refmut(&mut self) -> &mut AstBuilder<'alloc> {
        &mut self.handler
    }
}

impl<'alloc> ParserTrait<'alloc, StackValue<'alloc>> for Parser<'alloc> {
    fn shift(&mut self, tv: TermValue<StackValue<'alloc>>) -> Result<'alloc, bool> {
        // Shift the new terminal/nonterminal and its associated value.
        json_trace!({ "enter": "shift" });
        let mut state = self.state();
        assert!(state < TABLES.shift_count);
        let mut tv = tv;
        loop {
            let term_index: usize = tv.term.into();
            assert!(term_index < TABLES.shift_width);
            let index = state * TABLES.shift_width + term_index;
            let goto = TABLES.shift_table[index];
            json_trace!({
                "from": state,
                "to": goto,
                "term": format!("{:?}", { let s: &'static str = tv.term.into(); s }),
            });
            if goto < 0 {
                // Error handling is in charge of shifting an ErrorSymbol from the
                // current state.
                self.try_error_handling(tv)?;
                tv = self.replay_stack.pop().unwrap();
                json_trace!({ "replay_term": true });
                continue;
            }
            state = goto as usize;
            self.state_stack.push(state);
            self.node_stack.push(tv);
            // Execute any actions, such as reduce actions ast builder actions.
            while state >= TABLES.shift_count {
                assert!(state < TABLES.action_count + TABLES.shift_count);
                json_trace!({ "action": state });
                if full_actions(self, state)? {
                    return Ok(true);
                }
                state = self.state();
            }
            assert!(state < TABLES.shift_count);
            if let Some(tv_temp) = self.replay_stack.pop() {
                json_trace!({ "replay_term": true });
                tv = tv_temp;
            } else {
                break;
            }
        }
        Ok(false)
    }
    fn replay(&mut self, tv: TermValue<StackValue<'alloc>>) {
        self.replay_stack.push(tv)
    }
    fn epsilon(&mut self, state: usize) {
        *self.state_stack.last_mut().unwrap() = state;
    }
    fn pop(&mut self) -> TermValue<StackValue<'alloc>> {
        self.state_stack.pop().unwrap();
        self.node_stack.pop().unwrap()
    }
    fn check_not_on_new_line(&mut self, peek: usize) -> Result<'alloc, bool> {
        let sv = &self.node_stack[self.node_stack.len() - peek].value;
        if let StackValue::Token(ref token) = sv {
            if !token.is_on_new_line {
                return Ok(true);
            }
            self.rewind(peek - 1);
            let tv = self.pop();
            self.try_error_handling(tv)?;
            return Ok(false);
        }
        Err(ParseError::NoLineTerminatorHereExpectedToken)
    }
}

impl<'alloc> Parser<'alloc> {
    pub fn new(handler: AstBuilder<'alloc>, entry_state: usize) -> Self {
        TABLES.check();
        assert!(entry_state < TABLES.shift_count);

        Self {
            state_stack: vec![entry_state],
            node_stack: vec![],
            replay_stack: vec![],
            handler,
        }
    }

    fn state(&self) -> usize {
        *self.state_stack.last().unwrap()
    }

    pub fn write_token(&mut self, token: &Token) -> Result<'alloc, ()> {
        json_trace!({
            "method": "write_token",
            "is_on_new_line": token.is_on_new_line,
            "start": token.loc.start,
            "end": token.loc.end,
        });
        // Shift the token with the associated StackValue.
        let accept = self.shift(TermValue {
            term: Term::Terminal(token.terminal_id),
            value: StackValue::Token(self.handler.alloc(token.clone())),
        })?;
        // JavaScript grammar accepts empty inputs, therefore we can never
        // accept any program before receiving a TerminalId::End.
        assert!(!accept);
        Ok(())
    }

    pub fn close(&mut self, position: usize) -> Result<'alloc, StackValue<'alloc>> {
        // Shift the End terminal with the associated StackValue.
        json_trace!({
            "method": "close",
            "position": position,
        });
        let loc = SourceLocation::new(position, position);
        let token = Token::basic_token(TerminalId::End, loc);
        let accept = self.shift(TermValue {
            term: Term::Terminal(TerminalId::End),
            value: StackValue::Token(self.handler.alloc(token.clone())),
        })?;
        // Adding a TerminalId::End would either lead to a parse error, or to
        // accepting the current input. In which case we return matching node
        // value.
        assert!(accept);

        // We can either reduce a Script/Module, or a Script/Module followed by
        // an <End> terminal.
        assert!(self.node_stack.len() >= 1);
        assert!(self.node_stack.len() <= 2);
        if self.node_stack.len() > 1 {
            self.node_stack.pop();
        }
        Ok(self.node_stack.pop().unwrap().value)
    }

    pub(crate) fn parse_error(t: &Token) -> ParseError<'alloc> {
        if t.terminal_id == TerminalId::End {
            ParseError::UnexpectedEnd
        } else {
            ParseError::SyntaxError(t.clone())
        }
    }

    fn try_error_handling(&mut self, t: TermValue<StackValue<'alloc>>) -> Result<'alloc, bool> {
        json_trace!({
            "try_error_handling_term": format!("{}", {
                let s: &'static str = t.term.into();
                s
            }),
        });
        if let StackValue::Token(ref token) = t.value {
            // Error tokens might them-self cause more errors to be reported.
            // This happens due to the fact that the ErrorToken can be replayed,
            // and while the ErrorToken might be in the lookahead rules, it
            // might not be in the shifted terms coming after the reduced
            // nonterminal.
            if t.term == Term::Terminal(TerminalId::ErrorToken) {
                return Err(Self::parse_error(token));
            }

            // Otherwise, check if the current rule accept an Automatic
            // Semi-Colon insertion (ASI).
            let state = self.state();
            assert!(state < TABLES.shift_count);
            let error_code = TABLES.error_codes[state];
            if let Some(error_code) = error_code {
                let err_token = (*token).clone();
                Self::recover(token, error_code)?;
                self.replay(t);
                let err_token = self.handler.alloc(err_token);
                self.replay(TermValue {
                    term: Term::Terminal(TerminalId::ErrorToken),
                    value: StackValue::Token(err_token),
                });
                return Ok(false);
            }
            // On error, don't attempt error handling again.
            return Err(Self::parse_error(token));
        }
        Err(ParseError::ParserCannotUnpackToken)
    }

    pub(crate) fn recover(t: &Token, error_code: ErrorCode) -> Result<'alloc, ()> {
        match error_code {
            ErrorCode::Asi => {
                if t.is_on_new_line
                    || t.terminal_id == TerminalId::End
                    || t.terminal_id == TerminalId::CloseBrace
                {
                    Ok(())
                } else {
                    Err(Self::parse_error(t))
                }
            }
            ErrorCode::DoWhileAsi => Ok(()),
        }
    }

    fn simulator<'a>(&'a self) -> Simulator<'alloc, 'a> {
        assert_eq!(self.replay_stack.len(), 0);
        Simulator::new(&self.state_stack, &self.node_stack)
    }

    pub fn can_accept_terminal(&self, t: TerminalId) -> bool {
        let bogus_loc = SourceLocation::new(0, 0);
        let result = self
            .simulator()
            .write_token(&Token::basic_token(t, bogus_loc))
            .is_ok();
        json_trace!({
            "can_accept": result,
            "terminal": format!("{:?}", t),
        });
        result
    }

    /// Return true if self.close() would succeed.
    pub fn can_close(&self) -> bool {
        self.simulator().close(0).is_ok()
    }
}
