// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::huffman::decode_huffman;
use crate::prefix::Prefix;
use crate::{Error, Res};
use neqo_common::{qdebug, qerror};
use neqo_transport::Connection;
use std::convert::TryInto;
use std::mem;
use std::str;

pub trait ReadByte {
    /// # Errors
    ///    Return error occurred while reading a byte.
    ///    The exact error depends on trait implementation.
    fn read_byte(&mut self) -> Res<u8>;
}

pub trait Reader {
    /// # Errors
    ///    Return error occurred while reading date into a buffer.
    ///    The exact error depends on trait implementation.
    fn read(&mut self, buf: &mut [u8]) -> Res<usize>;
}

pub(crate) struct ReceiverConnWrapper<'a> {
    conn: &'a mut Connection,
    stream_id: u64,
}

impl<'a> ReadByte for ReceiverConnWrapper<'a> {
    fn read_byte(&mut self) -> Res<u8> {
        let mut b = [0];
        match self.conn.stream_recv(self.stream_id, &mut b)? {
            (_, true) => Err(Error::ClosedCriticalStream),
            (0, false) => Err(Error::NeedMoreData),
            _ => Ok(b[0]),
        }
    }
}

impl<'a> Reader for ReceiverConnWrapper<'a> {
    fn read(&mut self, buf: &mut [u8]) -> Res<usize> {
        match self.conn.stream_recv(self.stream_id, buf)? {
            (_, true) => Err(Error::ClosedCriticalStream),
            (amount, false) => Ok(amount),
        }
    }
}

impl<'a> ReceiverConnWrapper<'a> {
    pub fn new(conn: &'a mut Connection, stream_id: u64) -> Self {
        Self { conn, stream_id }
    }
}

/// This is only used by header decoder therefore all errors are `DecompressionFailed`.
/// A header block is read entirely before decoding it, therefore if there is not enough
/// data in the buffer an error `DecompressionFailed` will be return.
pub(crate) struct ReceiverBufferWrapper<'a> {
    buf: &'a [u8],
    offset: usize,
}

impl<'a> ReadByte for ReceiverBufferWrapper<'a> {
    fn read_byte(&mut self) -> Res<u8> {
        if self.offset == self.buf.len() {
            Err(Error::DecompressionFailed)
        } else {
            let b = self.buf[self.offset];
            self.offset += 1;
            Ok(b)
        }
    }
}

impl<'a> ReceiverBufferWrapper<'a> {
    pub fn new(buf: &'a [u8]) -> Self {
        Self { buf, offset: 0 }
    }

    pub fn peek(&self) -> Res<u8> {
        if self.offset == self.buf.len() {
            Err(Error::DecompressionFailed)
        } else {
            Ok(self.buf[self.offset])
        }
    }

    pub fn done(&self) -> bool {
        self.offset == self.buf.len()
    }

    /// The function decodes varint with a prefixed, i.e. ignores `prefix_len` bits of the first
    /// byte.
    /// `ReceiverBufferWrapper` is only used for decoding header blocks. The header blocks are read
    /// entirely before a decoding starts, therefore any incomplete varint because of reaching the
    /// end of a buffer will be treated as the `DecompressionFailed` error.
    pub fn read_prefixed_int(&mut self, prefix_len: u8) -> Res<u64> {
        debug_assert!(prefix_len < 8);

        let first_byte = self.read_byte()?;
        let mut reader = IntReader::new(first_byte, prefix_len);
        reader.read(self)
    }

    /// Do not use `LiteralReader` here to avoid copying data.
    /// The function decoded a literal with a prefix:
    ///   1) ignores `prefix_len` bits of the first byte,
    ///   2) reads "huffman bit"
    ///   3) decode varint that is the length of a literal
    ///   4) reads the literal
    ///   5) performs huffman decoding if needed.
    ///
    /// `ReceiverBufferWrapper` is only used for decoding header blocks. The header blocks are read
    /// entirely before a decoding starts, therefore any incomplete varint or literal because of
    /// reaching the end of a buffer will be treated as the `DecompressionFailed` error.
    pub fn read_literal_from_buffer(&mut self, prefix_len: u8) -> Res<String> {
        debug_assert!(prefix_len < 7);

        let first_byte = self.read_byte()?;
        let use_huffman = (first_byte & (0x80 >> prefix_len)) != 0;
        let mut int_reader = IntReader::new(first_byte, prefix_len + 1);
        let length: usize = int_reader
            .read(self)?
            .try_into()
            .or(Err(Error::DecompressionFailed))?;
        if use_huffman {
            Ok(to_string(&decode_huffman(self.slice(length)?)?)?)
        } else {
            Ok(to_string(self.slice(length)?)?)
        }
    }

    fn slice(&mut self, len: usize) -> Res<&[u8]> {
        if self.offset + len > self.buf.len() {
            Err(Error::DecompressionFailed)
        } else {
            let start = self.offset;
            self.offset += len;
            Ok(&self.buf[start..self.offset])
        }
    }
}

/// This is varint reader that can take into account a prefix.
#[derive(Debug)]
pub struct IntReader {
    value: u64,
    cnt: u8,
    done: bool,
}

impl IntReader {
    /// `IntReader` is created by suppling the first byte anf prefix length.
    /// A varint may take only one byte, In that case already the first by has set state to done.
    /// # Panics
    /// When `prefix_len` is 8 or larger.
    #[must_use]
    pub fn new(first_byte: u8, prefix_len: u8) -> Self {
        debug_assert!(prefix_len < 8, "prefix cannot larger than 7.");
        let mask = if prefix_len == 0 {
            0xff
        } else {
            (1 << (8 - prefix_len)) - 1
        };
        let value = u64::from(first_byte & mask);

        Self {
            value,
            cnt: 0,
            done: value < u64::from(mask),
        }
    }

    /// # Panics
    /// Never, but rust doesn't know that.
    #[must_use]
    pub fn make(first_byte: u8, prefixes: &[Prefix]) -> Self {
        for prefix in prefixes {
            if prefix.cmp_prefix(first_byte) {
                return Self::new(first_byte, prefix.len());
            }
        }
        unreachable!();
    }

    /// This function reads bytes until the varint is decoded or until stream/buffer does not
    /// have any more date.
    /// # Errors
    /// Possible errors are:
    ///  1) `NeedMoreData` if the reader needs more data,
    ///  2) `IntegerOverflow`,
    ///  3) Any `ReadByte`'s error
    pub fn read<R: ReadByte>(&mut self, s: &mut R) -> Res<u64> {
        let mut b: u8;
        while !self.done {
            b = s.read_byte()?;

            if (self.cnt == 63) && (b > 1 || (b == 1 && ((self.value >> 63) == 1))) {
                qerror!("Error decoding prefixed encoded int - IntegerOverflow");
                return Err(Error::IntegerOverflow);
            }
            self.value += u64::from(b & 0x7f) << self.cnt;
            if (b & 0x80) == 0 {
                self.done = true;
            }
            self.cnt += 7;
            if self.cnt >= 64 {
                self.done = true;
            }
        }
        Ok(self.value)
    }
}

#[derive(Debug)]
enum LiteralReaderState {
    ReadHuffman,
    ReadLength { reader: IntReader },
    ReadLiteral { offset: usize },
    Done,
}

impl Default for LiteralReaderState {
    fn default() -> Self {
        Self::ReadHuffman
    }
}

/// This is decoder of a literal with a prefix:
///   1) ignores `prefix_len` bits of the first byte,
///   2) reads "huffman bit"
///   3) decode varint that is the length of a literal
///   4) reads the literal
///   5) performs huffman decoding if needed.
#[derive(Debug, Default)]
pub struct LiteralReader {
    state: LiteralReaderState,
    literal: Vec<u8>,
    use_huffman: bool,
}

impl LiteralReader {
    /// Creates `LiteralReader` with the first byte. This constructor is always used
    /// when a litreral has a prefix.
    /// For literals without a prefix please use the default constructor.
    /// # Panics
    /// If `prefix_len` is 8 or more.
    #[must_use]
    pub fn new_with_first_byte(first_byte: u8, prefix_len: u8) -> Self {
        assert!(prefix_len < 8);
        Self {
            state: LiteralReaderState::ReadLength {
                reader: IntReader::new(first_byte, prefix_len + 1),
            },
            literal: Vec::new(),
            use_huffman: (first_byte & (0x80 >> prefix_len)) != 0,
        }
    }

    /// This function reads bytes until the literal is decoded or until stream/buffer does not
    /// have any more date ready.
    /// # Errors
    /// Possible errors are:
    ///  1) `NeedMoreData` if the reader needs more data,
    ///  2) `IntegerOverflow`
    ///  3) Any `ReadByte`'s error
    /// It returns value if reading the literal is done or None if it needs more data.
    /// # Panics
    /// When this object is complete.
    pub fn read<T: ReadByte + Reader>(&mut self, s: &mut T) -> Res<Vec<u8>> {
        loop {
            qdebug!("state = {:?}", self.state);
            match &mut self.state {
                LiteralReaderState::ReadHuffman => {
                    let b = s.read_byte()?;

                    self.use_huffman = (b & 0x80) != 0;
                    self.state = LiteralReaderState::ReadLength {
                        reader: IntReader::new(b, 1),
                    };
                }
                LiteralReaderState::ReadLength { reader } => {
                    let v = reader.read(s)?;
                    self.literal
                        .resize(v.try_into().or(Err(Error::Decoding))?, 0x0);
                    self.state = LiteralReaderState::ReadLiteral { offset: 0 };
                }
                LiteralReaderState::ReadLiteral { offset } => {
                    let amount = s.read(&mut self.literal[*offset..])?;
                    *offset += amount;
                    if *offset == self.literal.len() {
                        self.state = LiteralReaderState::Done;
                        if self.use_huffman {
                            break Ok(decode_huffman(&self.literal)?);
                        }
                        break Ok(mem::replace(&mut self.literal, Vec::new()));
                    }
                    break Err(Error::NeedMoreData);
                }
                LiteralReaderState::Done => {
                    panic!("Should not call read() in this state.");
                }
            }
        }
    }
}

/// This is a helper function used only by `ReceiverBufferWrapper`, therefore it returns
/// `DecompressionFailed` if any error happens.
/// # Errors
/// If an parsing error occurred, the function returns `ToStringFailed`.
pub fn to_string(v: &[u8]) -> Res<String> {
    match str::from_utf8(v) {
        Ok(s) => Ok(s.to_string()),
        Err(_) => Err(Error::ToStringFailed),
    }
}

#[cfg(test)]
pub(crate) mod test_receiver {

    use super::{Error, ReadByte, Reader, Res};
    use std::collections::VecDeque;

    pub struct TestReceiver {
        buf: VecDeque<u8>,
    }

    impl Default for TestReceiver {
        fn default() -> Self {
            Self {
                buf: VecDeque::new(),
            }
        }
    }

    impl ReadByte for TestReceiver {
        fn read_byte(&mut self) -> Res<u8> {
            self.buf.pop_back().ok_or(Error::NeedMoreData)
        }
    }

    impl Reader for TestReceiver {
        fn read(&mut self, buf: &mut [u8]) -> Res<usize> {
            let len = if buf.len() > self.buf.len() {
                self.buf.len()
            } else {
                buf.len()
            };
            for item in buf.iter_mut().take(len) {
                *item = self.buf.pop_back().ok_or(Error::NeedMoreData)?;
            }
            Ok(len)
        }
    }

    impl TestReceiver {
        pub fn write(&mut self, buf: &[u8]) {
            for b in buf {
                self.buf.push_front(*b);
            }
        }
    }
}

#[cfg(test)]
mod tests {

    use super::{
        str, test_receiver, to_string, Error, IntReader, LiteralReader, ReadByte,
        ReceiverBufferWrapper, Res,
    };
    use test_receiver::TestReceiver;

    const TEST_CASES_NUMBERS: [(&[u8], u8, u64); 7] = [
        (&[0xEA], 3, 10),
        (&[0x0A], 3, 10),
        (&[0x8A], 3, 10),
        (&[0xFF, 0x9A, 0x0A], 3, 1337),
        (&[0x1F, 0x9A, 0x0A], 3, 1337),
        (&[0x9F, 0x9A, 0x0A], 3, 1337),
        (&[0x2A], 0, 42),
    ];

    #[test]
    fn read_prefixed_int() {
        for (buf, prefix_len, value) in &TEST_CASES_NUMBERS {
            let mut reader = IntReader::new(buf[0], *prefix_len);
            let mut test_receiver: TestReceiver = TestReceiver::default();
            test_receiver.write(&buf[1..]);
            assert_eq!(reader.read(&mut test_receiver), Ok(*value));
        }
    }

    #[test]
    fn read_prefixed_int_with_more_data_in_buffer() {
        for (buf, prefix_len, value) in &TEST_CASES_NUMBERS {
            let mut reader = IntReader::new(buf[0], *prefix_len);
            let mut test_receiver: TestReceiver = TestReceiver::default();
            test_receiver.write(&buf[1..]);
            // add some more data
            test_receiver.write(&[0x0, 0x0, 0x0]);
            assert_eq!(reader.read(&mut test_receiver), Ok(*value));
        }
    }

    #[test]
    fn read_prefixed_int_slow_writer() {
        let (buf, prefix_len, value) = &TEST_CASES_NUMBERS[4];
        let mut reader = IntReader::new(buf[0], *prefix_len);
        let mut test_receiver: TestReceiver = TestReceiver::default();

        // data has not been received yet, reading IntReader will return Err(Error::NeedMoreData).
        assert_eq!(reader.read(&mut test_receiver), Err(Error::NeedMoreData));

        // Write one byte.
        test_receiver.write(&buf[1..2]);
        // data has not been received yet, reading IntReader will return Err(Error::NeedMoreData).
        assert_eq!(reader.read(&mut test_receiver), Err(Error::NeedMoreData));

        // Write one byte.
        test_receiver.write(&buf[2..]);
        // Now prefixed int is complete.
        assert_eq!(reader.read(&mut test_receiver), Ok(*value));
    }

    type TestSetup = (&'static [u8], u8, Res<u64>);
    const TEST_CASES_BIG_NUMBERS: [TestSetup; 3] = [
        (
            &[
                0xFF, 0x80, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
            ],
            0,
            Ok(0xFFFF_FFFF_FFFF_FFFF),
        ),
        (
            &[
                0xFF, 0x81, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
            ],
            0,
            Err(Error::IntegerOverflow),
        ),
        (
            &[
                0xFF, 0x80, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02,
            ],
            0,
            Err(Error::IntegerOverflow),
        ),
    ];

    #[test]
    fn read_prefixed_int_big_number() {
        for (buf, prefix_len, value) in &TEST_CASES_BIG_NUMBERS {
            let mut reader = IntReader::new(buf[0], *prefix_len);
            let mut test_receiver: TestReceiver = TestReceiver::default();
            test_receiver.write(&buf[1..]);
            assert_eq!(reader.read(&mut test_receiver), *value);
        }
    }

    const TEST_CASES_LITERAL: [(&[u8], u8, &str); 9] = [
        // No Huffman
        (
            &[
                0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65, 0x79,
            ],
            1,
            "custom-key",
        ),
        (
            &[
                0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65, 0x79,
            ],
            3,
            "custom-key",
        ),
        (
            &[
                0xea, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65, 0x79,
            ],
            3,
            "custom-key",
        ),
        (
            &[
                0x0d, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x68, 0x65, 0x61, 0x64, 0x65, 0x72,
            ],
            1,
            "custom-header",
        ),
        // With Huffman
        (&[0x15, 0xae, 0xc3, 0x77, 0x1a, 0x4b], 3, "private"),
        (
            &[
                0x56, 0xd0, 0x7a, 0xbe, 0x94, 0x10, 0x54, 0xd4, 0x44, 0xa8, 0x20, 0x05, 0x95, 0x04,
                0x0b, 0x81, 0x66, 0xe0, 0x82, 0xa6, 0x2d, 0x1b, 0xff,
            ],
            1,
            "Mon, 21 Oct 2013 20:13:21 GMT",
        ),
        (
            &[
                0xff, 0x0f, 0xd0, 0x7a, 0xbe, 0x94, 0x10, 0x54, 0xd4, 0x44, 0xa8, 0x20, 0x05, 0x95,
                0x04, 0x0b, 0x81, 0x66, 0xe0, 0x82, 0xa6, 0x2d, 0x1b, 0xff,
            ],
            4,
            "Mon, 21 Oct 2013 20:13:21 GMT",
        ),
        (
            &[
                0x51, 0x9d, 0x29, 0xad, 0x17, 0x18, 0x63, 0xc7, 0x8f, 0x0b, 0x97, 0xc8, 0xe9, 0xae,
                0x82, 0xae, 0x43, 0xd3,
            ],
            1,
            "https://www.example.com",
        ),
        (
            &[
                0x91, 0x9d, 0x29, 0xad, 0x17, 0x18, 0x63, 0xc7, 0x8f, 0x0b, 0x97, 0xc8, 0xe9, 0xae,
                0x82, 0xae, 0x43, 0xd3,
            ],
            0,
            "https://www.example.com",
        ),
    ];

    #[test]
    fn read_literal() {
        for (buf, prefix_len, value) in &TEST_CASES_LITERAL {
            let mut reader = LiteralReader::new_with_first_byte(buf[0], *prefix_len);
            let mut test_receiver: TestReceiver = TestReceiver::default();
            test_receiver.write(&buf[1..]);
            assert_eq!(
                to_string(&reader.read(&mut test_receiver).unwrap()).unwrap(),
                *value
            );
        }
    }

    #[test]
    fn read_prefixed_int_receiver_buffer_wrapper() {
        for (buf, prefix_len, value) in &TEST_CASES_NUMBERS {
            let mut buffer = ReceiverBufferWrapper::new(buf);
            let mut reader = IntReader::new(buffer.read_byte().unwrap(), *prefix_len);
            assert_eq!(reader.read(&mut buffer), Ok(*value));
        }
    }

    #[test]
    fn read_prefixed_int_big_receiver_buffer_wrapper() {
        for (buf, prefix_len, value) in &TEST_CASES_BIG_NUMBERS {
            let mut buffer = ReceiverBufferWrapper::new(buf);
            let mut reader = IntReader::new(buffer.read_byte().unwrap(), *prefix_len);
            assert_eq!(reader.read(&mut buffer), *value);
        }
    }

    #[test]
    fn read_literal_receiver_buffer_wrapper() {
        for (buf, prefix_len, value) in &TEST_CASES_LITERAL {
            let mut buffer = ReceiverBufferWrapper::new(buf);
            assert_eq!(
                buffer.read_literal_from_buffer(*prefix_len).unwrap(),
                *value
            );
        }
    }

    #[test]
    fn read_failure_receiver_buffer_wrapper_number() {
        let (buf, prefix_len, _) = &TEST_CASES_NUMBERS[4];
        let mut buffer = ReceiverBufferWrapper::new(&buf[..1]);
        let mut reader = IntReader::new(buffer.read_byte().unwrap(), *prefix_len);
        assert_eq!(reader.read(&mut buffer), Err(Error::DecompressionFailed));
    }

    #[test]
    fn read_failure_receiver_buffer_wrapper_literal() {
        let (buf, prefix_len, _) = &TEST_CASES_LITERAL[0];
        let mut buffer = ReceiverBufferWrapper::new(&buf[..6]);
        assert_eq!(
            buffer.read_literal_from_buffer(*prefix_len),
            Err(Error::DecompressionFailed)
        );
    }
}
