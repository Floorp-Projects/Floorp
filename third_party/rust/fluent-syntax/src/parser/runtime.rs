use super::{
    core::{Parser, Result},
    errors::ParserError,
    slice::Slice,
};
use crate::ast;

impl<'s, S> Parser<S>
where
    S: Slice<'s>,
{
    pub fn parse_runtime(
        mut self,
    ) -> std::result::Result<ast::Resource<S>, (ast::Resource<S>, Vec<ParserError>)> {
        let mut errors = vec![];

        // That default allocation gives the lowest
        // number of instructions and cycles in ioi.
        let mut body = Vec::with_capacity(6);

        self.skip_blank_block();

        while self.ptr < self.length {
            let entry_start = self.ptr;
            let entry = self.get_entry_runtime(entry_start);

            match entry {
                Ok(Some(entry)) => {
                    body.push(entry);
                }
                Ok(None) => {}
                Err(mut err) => {
                    self.skip_to_next_entry_start();
                    err.slice = Some(entry_start..self.ptr);
                    errors.push(err);
                    let content = self.source.slice(entry_start..self.ptr);
                    body.push(ast::Entry::Junk { content });
                }
            }
            self.skip_blank_block();
        }

        if errors.is_empty() {
            Ok(ast::Resource { body })
        } else {
            Err((ast::Resource { body }, errors))
        }
    }

    fn get_entry_runtime(&mut self, entry_start: usize) -> Result<Option<ast::Entry<S>>> {
        let entry = match get_current_byte!(self) {
            Some(b'#') => {
                self.skip_comment();
                None
            }
            Some(b'-') => Some(ast::Entry::Term(self.get_term(entry_start)?)),
            _ => Some(ast::Entry::Message(self.get_message(entry_start)?)),
        };
        Ok(entry)
    }
}
