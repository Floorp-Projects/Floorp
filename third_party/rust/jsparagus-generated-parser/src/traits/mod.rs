use crate::error::Result;
use crate::parser_tables_generated::Term;

/// This macro is pre-processed by the python grammar processor and generate
/// code out-side the current context.
#[macro_export]
macro_rules! grammar_extension {
    ( $($_:tt)* ) => {};
}

/// Aggregate a Value (= StackValue or ()) and a nonterminal/terminal as a stack
/// element. The term is currently used for replaying the parse table when
/// handling errors and non-optimized reduced actions with lookahead.
#[derive(Debug)]
pub struct TermValue<Value> {
    pub term: Term,
    pub value: Value,
}

/// The parser trait is an abstraction to define the primitive of an LR Parser
/// with variable lookahead which can be replayed.
pub trait ParserTrait<'alloc, Value> {
    fn shift(&mut self, tv: TermValue<Value>) -> Result<'alloc, bool>;
    fn shift_replayed(&mut self, state: usize);
    fn unshift(&mut self);
    fn rewind(&mut self, n: usize) {
        for _ in 0..n {
            self.unshift();
        }
    }
    fn pop(&mut self) -> TermValue<Value>;
    fn replay(&mut self, tv: TermValue<Value>);
    fn epsilon(&mut self, state: usize);
    fn top_state(&self) -> usize;
    fn check_not_on_new_line(&mut self, peek: usize) -> Result<'alloc, bool>;
}
