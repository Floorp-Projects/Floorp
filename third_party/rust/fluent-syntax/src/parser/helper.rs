use super::errors::{ErrorKind, ParserError};
use super::{Parser, Result, Slice};

impl<'s, S> Parser<S>
where
    S: Slice<'s>,
{
    pub(super) fn is_current_byte(&self, b: u8) -> bool {
        self.source.as_ref().as_bytes().get(self.ptr) == Some(&b)
    }

    pub(super) fn is_byte_at(&self, b: u8, pos: usize) -> bool {
        self.source.as_ref().as_bytes().get(pos) == Some(&b)
    }

    pub(super) fn skip_to_next_entry_start(&mut self) {
        while let Some(b) = self.source.as_ref().as_bytes().get(self.ptr) {
            let new_line =
                self.ptr == 0 || self.source.as_ref().as_bytes().get(self.ptr - 1) == Some(&b'\n');

            if new_line && (b.is_ascii_alphabetic() || [b'-', b'#'].contains(b)) {
                break;
            }

            self.ptr += 1;
        }
    }

    pub(super) fn skip_eol(&mut self) -> bool {
        match self.source.as_ref().as_bytes().get(self.ptr) {
            Some(b'\n') => {
                self.ptr += 1;
                true
            }
            Some(b'\r') if self.is_byte_at(b'\n', self.ptr + 1) => {
                self.ptr += 2;
                true
            }
            _ => false,
        }
    }

    pub(super) fn skip_unicode_escape_sequence(&mut self, length: usize) -> Result<()> {
        let start = self.ptr;
        for _ in 0..length {
            match self.source.as_ref().as_bytes().get(self.ptr) {
                Some(b) if b.is_ascii_hexdigit() => self.ptr += 1,
                _ => break,
            }
        }
        if self.ptr - start != length {
            let end = if self.ptr >= self.length {
                self.ptr
            } else {
                self.ptr + 1
            };
            let seq = self.source.slice(start..end).as_ref().to_owned();
            return error!(ErrorKind::InvalidUnicodeEscapeSequence(seq), self.ptr);
        }
        Ok(())
    }

    pub(super) fn is_identifier_start(&self) -> bool {
        matches!(self.source.as_ref().as_bytes().get(self.ptr), Some(b) if b.is_ascii_alphabetic())
    }

    pub(super) fn take_byte_if(&mut self, b: u8) -> bool {
        if self.is_current_byte(b) {
            self.ptr += 1;
            true
        } else {
            false
        }
    }

    pub(super) fn skip_blank_block(&mut self) -> usize {
        let mut count = 0;
        loop {
            let start = self.ptr;
            self.skip_blank_inline();
            if !self.skip_eol() {
                self.ptr = start;
                break;
            }
            count += 1;
        }
        count
    }

    pub(super) fn skip_blank(&mut self) {
        loop {
            match self.source.as_ref().as_bytes().get(self.ptr) {
                Some(b' ') | Some(b'\n') => self.ptr += 1,
                Some(b'\r')
                    if self.source.as_ref().as_bytes().get(self.ptr + 1) == Some(&b'\n') =>
                {
                    self.ptr += 2
                }
                _ => break,
            }
        }
    }

    pub(super) fn skip_blank_inline(&mut self) -> usize {
        let start = self.ptr;
        while let Some(b' ') = self.source.as_ref().as_bytes().get(self.ptr) {
            self.ptr += 1;
        }
        self.ptr - start
    }

    pub(super) fn is_byte_pattern_continuation(b: u8) -> bool {
        ![b'}', b'.', b'[', b'*'].contains(&b)
    }

    pub(super) fn is_callee(name: &[u8]) -> bool {
        name.iter()
            .all(|c| c.is_ascii_uppercase() || c.is_ascii_digit() || *c == b'_' || *c == b'-')
    }

    pub(super) fn expect_byte(&mut self, b: u8) -> Result<()> {
        if !self.is_current_byte(b) {
            return error!(ErrorKind::ExpectedToken(b as char), self.ptr);
        }
        self.ptr += 1;
        Ok(())
    }

    pub(super) fn is_number_start(&self) -> bool {
        matches!(self.source.as_ref().as_bytes().get(self.ptr), Some(b) if (b == &b'-') || b.is_ascii_digit())
    }

    pub(super) fn is_eol(&self) -> bool {
        match self.source.as_ref().as_bytes().get(self.ptr) {
            Some(b'\n') => true,
            Some(b'\r') if self.is_byte_at(b'\n', self.ptr + 1) => true,
            _ => false,
        }
    }

    pub(super) fn skip_digits(&mut self) -> Result<()> {
        let start = self.ptr;
        loop {
            match self.source.as_ref().as_bytes().get(self.ptr) {
                Some(b) if b.is_ascii_digit() => self.ptr += 1,
                _ => break,
            }
        }
        if start == self.ptr {
            error!(
                ErrorKind::ExpectedCharRange {
                    range: "0-9".to_string()
                },
                self.ptr
            )
        } else {
            Ok(())
        }
    }

    pub(super) fn get_number_literal(&mut self) -> Result<S> {
        let start = self.ptr;
        self.take_byte_if(b'-');
        self.skip_digits()?;
        if self.take_byte_if(b'.') {
            self.skip_digits()?;
        }

        Ok(self.source.slice(start..self.ptr))
    }
}
