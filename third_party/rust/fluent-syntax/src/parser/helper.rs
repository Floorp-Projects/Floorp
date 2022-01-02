use super::errors::{ErrorKind, ParserError};
use super::{core::Parser, core::Result, slice::Slice};

impl<'s, S> Parser<S>
where
    S: Slice<'s>,
{
    pub(super) fn is_current_byte(&self, b: u8) -> bool {
        get_current_byte!(self) == Some(&b)
    }

    pub(super) fn is_byte_at(&self, b: u8, pos: usize) -> bool {
        get_byte!(self, pos) == Some(&b)
    }

    pub(super) fn skip_to_next_entry_start(&mut self) {
        while let Some(b) = get_current_byte!(self) {
            let new_line = self.ptr == 0 || get_byte!(self, self.ptr - 1) == Some(&b'\n');

            if new_line && (b.is_ascii_alphabetic() || [b'-', b'#'].contains(b)) {
                break;
            }

            self.ptr += 1;
        }
    }

    pub(super) fn skip_eol(&mut self) -> bool {
        match get_current_byte!(self) {
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
            match get_current_byte!(self) {
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
        matches!(get_current_byte!(self), Some(b) if b.is_ascii_alphabetic())
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
            match get_current_byte!(self) {
                Some(b' ') | Some(b'\n') => self.ptr += 1,
                Some(b'\r') if get_byte!(self, self.ptr + 1) == Some(&b'\n') => self.ptr += 2,
                _ => break,
            }
        }
    }

    pub(super) fn skip_blank_inline(&mut self) -> usize {
        let start = self.ptr;
        while let Some(b' ') = get_current_byte!(self) {
            self.ptr += 1;
        }
        self.ptr - start
    }

    pub(super) fn is_byte_pattern_continuation(b: u8) -> bool {
        !matches!(b, b'.' | b'}' | b'[' | b'*')
    }

    pub(super) fn is_callee(name: &S) -> bool {
        name.as_ref()
            .as_bytes()
            .iter()
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
        matches!(get_current_byte!(self), Some(b) if b.is_ascii_digit() || b == &b'-')
    }

    pub(super) fn is_eol(&self) -> bool {
        match get_current_byte!(self) {
            Some(b'\n') => true,
            Some(b'\r') if self.is_byte_at(b'\n', self.ptr + 1) => true,
            None => true,
            _ => false,
        }
    }

    pub(super) fn skip_digits(&mut self) -> Result<()> {
        let start = self.ptr;
        loop {
            match get_current_byte!(self) {
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
