use super::{Parser, Result, Slice};
use crate::ast;

#[derive(Debug, PartialEq, Clone, Copy)]
pub(super) enum Level {
    None = 0,
    Regular = 1,
    Group = 2,
    Resource = 3,
}

impl<'s, S> Parser<S>
where
    S: Slice<'s>,
{
    pub(super) fn get_comment(&mut self) -> Result<(ast::Comment<S>, Level)> {
        let mut level = Level::None;
        let mut content = vec![];

        while self.ptr < self.length {
            let line_level = self.get_comment_level();
            if line_level == Level::None {
                self.ptr -= 1;
                break;
            } else if level != Level::None && line_level != level {
                self.ptr -= line_level as usize;
                break;
            }

            level = line_level;

            if self.ptr == self.length {
                break;
            } else if self.is_current_byte(b'\n') {
                content.push(self.get_comment_line()?);
            } else {
                if let Err(e) = self.expect_byte(b' ') {
                    if content.is_empty() {
                        return Err(e);
                    } else {
                        self.ptr -= line_level as usize;
                        break;
                    }
                }
                content.push(self.get_comment_line()?);
            }
            self.skip_eol();
        }

        Ok((ast::Comment { content }, level))
    }

    fn get_comment_level(&mut self) -> Level {
        let mut chars = 0;

        for _ in 0..3 {
            if self.take_byte_if(b'#') {
                chars += 1;
            }
        }

        match chars {
            0 => Level::None,
            1 => Level::Regular,
            2 => Level::Group,
            3 => Level::Resource,
            _ => unreachable!(),
        }
    }

    fn get_comment_line(&mut self) -> Result<S> {
        let start_pos = self.ptr;

        while self.ptr < self.length && !self.is_eol() {
            self.ptr += 1;
        }

        Ok(self.source.slice(start_pos..self.ptr))
    }
}
