// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::prefix::{
    ENCODER_CAPACITY, ENCODER_DUPLICATE, ENCODER_INSERT_WITH_NAME_LITERAL,
    ENCODER_INSERT_WITH_NAME_REF_DYNAMIC, ENCODER_INSERT_WITH_NAME_REF_STATIC, NO_PREFIX,
};
use crate::qpack_send_buf::QPData;
use crate::reader::{IntReader, LiteralReader, ReadByte, Reader};
use crate::{Error, Res};
use neqo_common::{matches, qdebug, qtrace};
use std::mem;

#[derive(Debug, PartialEq)]
pub enum EncoderInstruction {
    Capacity { value: u64 },
    InsertWithNameRefStatic { index: u64, value: Vec<u8> },
    InsertWithNameRefDynamic { index: u64, value: Vec<u8> },
    InsertWithNameLiteral { name: Vec<u8>, value: Vec<u8> },
    Duplicate { index: u64 },
    NoInstruction,
}

impl EncoderInstruction {
    pub(crate) fn marshal(&self, enc: &mut QPData, use_huffman: bool) {
        match self {
            Self::Capacity { value } => {
                enc.encode_prefixed_encoded_int(ENCODER_CAPACITY, *value);
            }
            Self::InsertWithNameRefStatic { index, value } => {
                enc.encode_prefixed_encoded_int(ENCODER_INSERT_WITH_NAME_REF_STATIC, *index);
                enc.encode_literal(use_huffman, NO_PREFIX, value);
            }
            Self::InsertWithNameRefDynamic { index, value } => {
                enc.encode_prefixed_encoded_int(ENCODER_INSERT_WITH_NAME_REF_DYNAMIC, *index);
                enc.encode_literal(use_huffman, NO_PREFIX, value);
            }
            Self::InsertWithNameLiteral { name, value } => {
                enc.encode_literal(use_huffman, ENCODER_INSERT_WITH_NAME_LITERAL, name);
                enc.encode_literal(use_huffman, NO_PREFIX, value);
            }
            Self::Duplicate { index } => {
                enc.encode_prefixed_encoded_int(ENCODER_DUPLICATE, *index);
            }
            Self::NoInstruction => {}
        }
    }
}

#[derive(Debug)]
enum EncoderInstructionReaderState {
    ReadInstruction,
    ReadFirstInt { reader: IntReader },
    ReadFirstLiteral { reader: LiteralReader },
    ReadSecondLiteral { reader: LiteralReader },
    Done,
}

#[derive(Debug)]
pub struct EncoderInstructionReader {
    state: EncoderInstructionReaderState,
    instruction: EncoderInstruction,
}

impl ::std::fmt::Display for EncoderInstructionReader {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(
            f,
            "EncoderInstructionReader state={:?} instruction:{:?}",
            self.state, self.instruction
        )
    }
}

impl EncoderInstructionReader {
    pub fn new() -> Self {
        Self {
            state: EncoderInstructionReaderState::ReadInstruction,
            instruction: EncoderInstruction::NoInstruction,
        }
    }

    fn decode_instruction_from_byte(&mut self, b: u8) {
        self.instruction = if ENCODER_INSERT_WITH_NAME_REF_STATIC.cmp_prefix(b) {
            EncoderInstruction::InsertWithNameRefStatic {
                index: 0,
                value: Vec::new(),
            }
        } else if ENCODER_INSERT_WITH_NAME_REF_DYNAMIC.cmp_prefix(b) {
            EncoderInstruction::InsertWithNameRefDynamic {
                index: 0,
                value: Vec::new(),
            }
        } else if ENCODER_INSERT_WITH_NAME_LITERAL.cmp_prefix(b) {
            EncoderInstruction::InsertWithNameLiteral {
                name: Vec::new(),
                value: Vec::new(),
            }
        } else if ENCODER_CAPACITY.cmp_prefix(b) {
            EncoderInstruction::Capacity { value: 0 }
        } else if ENCODER_DUPLICATE.cmp_prefix(b) {
            EncoderInstruction::Duplicate { index: 0 }
        } else {
            unreachable!("The above patterns match everything.");
        };
        qdebug!([self], "instruction decoded");
    }

    fn decode_instruction_type<T: ReadByte + Reader>(&mut self, recv: &mut T) -> Res<bool> {
        match recv.read_byte() {
            Ok(b) => {
                self.decode_instruction_from_byte(b);
                match self.instruction {
                    EncoderInstruction::Capacity { .. } | EncoderInstruction::Duplicate { .. } => {
                        self.state = EncoderInstructionReaderState::ReadFirstInt {
                            reader: IntReader::new(b, ENCODER_CAPACITY.len()),
                        }
                    }
                    EncoderInstruction::InsertWithNameRefStatic { .. }
                    | EncoderInstruction::InsertWithNameRefDynamic { .. } => {
                        self.state = EncoderInstructionReaderState::ReadFirstInt {
                            reader: IntReader::new(b, ENCODER_INSERT_WITH_NAME_REF_STATIC.len()),
                        }
                    }
                    EncoderInstruction::InsertWithNameLiteral { .. } => {
                        self.state = EncoderInstructionReaderState::ReadFirstLiteral {
                            reader: LiteralReader::new_with_first_byte(
                                b,
                                ENCODER_INSERT_WITH_NAME_LITERAL.len(),
                            ),
                        }
                    }
                    EncoderInstruction::NoInstruction => {
                        unreachable!("We must have instruction at this point.")
                    }
                }
                Ok(true)
            }
            Err(Error::NoMoreData) => Ok(false),
            Err(Error::ClosedCriticalStream) => Err(Error::ClosedCriticalStream),
            Err(_) => Err(Error::EncoderStream),
        }
    }

    pub fn read_instructions<T: ReadByte + Reader>(
        &mut self,
        recv: &mut T,
    ) -> Res<Option<EncoderInstruction>> {
        qdebug!([self], "reading instructions");
        loop {
            match &mut self.state {
                EncoderInstructionReaderState::ReadInstruction => {
                    if !self.decode_instruction_type(recv)? {
                        break Ok(None);
                    }
                }
                EncoderInstructionReaderState::ReadFirstInt { reader } => match reader.read(recv) {
                    Ok(Some(val)) => {
                        qtrace!([self], "First varint read {}", val);
                        match &mut self.instruction {
                            EncoderInstruction::Capacity { value: v, .. }
                            | EncoderInstruction::Duplicate { index: v } => {
                                *v = val;
                                self.state = EncoderInstructionReaderState::Done;
                            }
                            EncoderInstruction::InsertWithNameRefStatic { index, .. }
                            | EncoderInstruction::InsertWithNameRefDynamic { index, .. } => {
                                *index = val;
                                self.state = EncoderInstructionReaderState::ReadFirstLiteral {
                                    reader: LiteralReader::default(),
                                };
                            }
                            _ => unreachable!("This instruction cannot be in this state."),
                        }
                    }
                    Ok(None) => break Ok(None),
                    Err(Error::ClosedCriticalStream) => break Err(Error::ClosedCriticalStream),
                    Err(_) => break Err(Error::EncoderStream),
                },
                EncoderInstructionReaderState::ReadFirstLiteral { reader } => {
                    match reader.read(recv) {
                        Ok(Some(val)) => {
                            qtrace!([self], "first literal read {:?}", val);
                            match &mut self.instruction {
                                EncoderInstruction::InsertWithNameRefStatic { value, .. }
                                | EncoderInstruction::InsertWithNameRefDynamic { value, .. } => {
                                    *value = val;
                                    self.state = EncoderInstructionReaderState::Done;
                                }
                                EncoderInstruction::InsertWithNameLiteral { name, .. } => {
                                    *name = val;
                                    self.state = EncoderInstructionReaderState::ReadSecondLiteral {
                                        reader: LiteralReader::default(),
                                    };
                                }
                                _ => unreachable!("This instruction cannot be in this state."),
                            }
                        }
                        Ok(None) => break Ok(None),
                        Err(Error::ClosedCriticalStream) => break Err(Error::ClosedCriticalStream),
                        Err(_) => break Err(Error::EncoderStream),
                    }
                }
                EncoderInstructionReaderState::ReadSecondLiteral { reader } => {
                    match reader.read(recv) {
                        Ok(Some(val)) => {
                            qtrace!([self], "second literal read {:?}", val);
                            match &mut self.instruction {
                                EncoderInstruction::InsertWithNameLiteral { value, .. } => {
                                    *value = val;
                                    self.state = EncoderInstructionReaderState::Done;
                                }
                                _ => unreachable!("This instruction cannot be in this state."),
                            }
                        }
                        Ok(None) => break Ok(None),
                        Err(Error::ClosedCriticalStream) => break Err(Error::ClosedCriticalStream),
                        Err(_) => break Err(Error::EncoderStream),
                    }
                }
                EncoderInstructionReaderState::Done => {}
            }
            if matches!(self.state, EncoderInstructionReaderState::Done) {
                self.state = EncoderInstructionReaderState::ReadInstruction;
                break Ok(Some(mem::replace(
                    &mut self.instruction,
                    EncoderInstruction::NoInstruction,
                )));
            }
        }
    }
}

#[cfg(test)]
mod test {

    use super::{EncoderInstruction, EncoderInstructionReader, Error, QPData};
    use crate::reader::test_receiver::TestReceiver;

    fn test_encoding_decoding(instruction: &EncoderInstruction, use_huffman: bool) {
        let mut buf = QPData::default();
        instruction.marshal(&mut buf, use_huffman);
        let mut test_receiver: TestReceiver = TestReceiver::default();
        test_receiver.write(&buf);
        let mut reader = EncoderInstructionReader::new();
        assert_eq!(
            reader
                .read_instructions(&mut test_receiver)
                .unwrap()
                .unwrap(),
            *instruction
        );
    }

    #[test]
    fn test_encoding_decoding_instructions() {
        test_encoding_decoding(&EncoderInstruction::Capacity { value: 1 }, false);
        test_encoding_decoding(&EncoderInstruction::Capacity { value: 10_000 }, false);

        test_encoding_decoding(
            &EncoderInstruction::InsertWithNameRefStatic {
                index: 1,
                value: vec![0x62, 0x64, 0x65],
            },
            false,
        );
        test_encoding_decoding(
            &EncoderInstruction::InsertWithNameRefStatic {
                index: 1,
                value: vec![0x62, 0x64, 0x65],
            },
            true,
        );
        test_encoding_decoding(
            &EncoderInstruction::InsertWithNameRefStatic {
                index: 10_000,
                value: vec![0x62, 0x64, 0x65],
            },
            false,
        );
        test_encoding_decoding(
            &EncoderInstruction::InsertWithNameRefStatic {
                index: 10_000,
                value: vec![0x62, 0x64, 0x65],
            },
            true,
        );

        test_encoding_decoding(
            &EncoderInstruction::InsertWithNameRefDynamic {
                index: 1,
                value: vec![0x62, 0x64, 0x65],
            },
            false,
        );
        test_encoding_decoding(
            &EncoderInstruction::InsertWithNameRefDynamic {
                index: 1,
                value: vec![0x62, 0x64, 0x65],
            },
            true,
        );
        test_encoding_decoding(
            &EncoderInstruction::InsertWithNameRefDynamic {
                index: 10_000,
                value: vec![0x62, 0x64, 0x65],
            },
            false,
        );
        test_encoding_decoding(
            &EncoderInstruction::InsertWithNameRefDynamic {
                index: 10_000,
                value: vec![0x62, 0x64, 0x65],
            },
            true,
        );

        test_encoding_decoding(
            &EncoderInstruction::InsertWithNameLiteral {
                name: vec![0x62, 0x64, 0x65],
                value: vec![0x62, 0x64, 0x65],
            },
            false,
        );
        test_encoding_decoding(
            &EncoderInstruction::InsertWithNameLiteral {
                name: vec![0x62, 0x64, 0x65],
                value: vec![0x62, 0x64, 0x65],
            },
            true,
        );

        test_encoding_decoding(&EncoderInstruction::Duplicate { index: 1 }, false);
        test_encoding_decoding(&EncoderInstruction::Duplicate { index: 10_000 }, false);
    }

    fn test_encoding_decoding_slow_reader(instruction: &EncoderInstruction, use_huffman: bool) {
        let mut buf = QPData::default();
        instruction.marshal(&mut buf, use_huffman);
        let mut test_receiver: TestReceiver = TestReceiver::default();
        let mut decoder = EncoderInstructionReader::new();
        for i in 0..buf.len() - 1 {
            test_receiver.write(&buf[i..=i]);
            assert!(decoder
                .read_instructions(&mut test_receiver)
                .unwrap()
                .is_none());
        }
        test_receiver.write(&buf[buf.len() - 1..buf.len()]);
        assert_eq!(
            decoder
                .read_instructions(&mut test_receiver)
                .unwrap()
                .unwrap(),
            *instruction
        );
    }

    #[test]
    fn test_encoding_decoding_instructions_slow_reader() {
        test_encoding_decoding_slow_reader(&EncoderInstruction::Capacity { value: 1 }, false);
        test_encoding_decoding_slow_reader(&EncoderInstruction::Capacity { value: 10_000 }, false);

        test_encoding_decoding_slow_reader(
            &EncoderInstruction::InsertWithNameRefStatic {
                index: 1,
                value: vec![0x62, 0x64, 0x65],
            },
            false,
        );
        test_encoding_decoding_slow_reader(
            &EncoderInstruction::InsertWithNameRefStatic {
                index: 1,
                value: vec![0x62, 0x64, 0x65],
            },
            true,
        );
        test_encoding_decoding_slow_reader(
            &EncoderInstruction::InsertWithNameRefStatic {
                index: 10_000,
                value: vec![0x62, 0x64, 0x65],
            },
            false,
        );
        test_encoding_decoding_slow_reader(
            &EncoderInstruction::InsertWithNameRefStatic {
                index: 10_000,
                value: vec![0x62, 0x64, 0x65],
            },
            true,
        );

        test_encoding_decoding_slow_reader(
            &EncoderInstruction::InsertWithNameRefDynamic {
                index: 1,
                value: vec![0x62, 0x64, 0x65],
            },
            false,
        );
        test_encoding_decoding_slow_reader(
            &EncoderInstruction::InsertWithNameRefDynamic {
                index: 1,
                value: vec![0x62, 0x64, 0x65],
            },
            true,
        );
        test_encoding_decoding_slow_reader(
            &EncoderInstruction::InsertWithNameRefDynamic {
                index: 10_000,
                value: vec![0x62, 0x64, 0x65],
            },
            false,
        );
        test_encoding_decoding_slow_reader(
            &EncoderInstruction::InsertWithNameRefDynamic {
                index: 10_000,
                value: vec![0x62, 0x64, 0x65],
            },
            true,
        );

        test_encoding_decoding_slow_reader(
            &EncoderInstruction::InsertWithNameLiteral {
                name: vec![0x62, 0x64, 0x65],
                value: vec![0x62, 0x64, 0x65],
            },
            false,
        );
        test_encoding_decoding_slow_reader(
            &EncoderInstruction::InsertWithNameLiteral {
                name: vec![0x62, 0x64, 0x65],
                value: vec![0x62, 0x64, 0x65],
            },
            true,
        );

        test_encoding_decoding_slow_reader(&EncoderInstruction::Duplicate { index: 1 }, false);
        test_encoding_decoding_slow_reader(&EncoderInstruction::Duplicate { index: 10_000 }, false);
    }

    #[test]
    fn test_decoding_error() {
        let mut test_receiver: TestReceiver = TestReceiver::default();
        // EncoderInstruction::Capacity with overflow
        test_receiver.write(&[
            0x3f, 0xc1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xff, 0x02,
        ]);
        let mut decoder = EncoderInstructionReader::new();
        assert_eq!(
            decoder.read_instructions(&mut test_receiver),
            Err(Error::EncoderStream)
        );

        let mut test_receiver: TestReceiver = TestReceiver::default();
        // EncoderInstruction::InsertWithNameRefStatic with overflow of index value.
        test_receiver.write(&[
            0xff, 0xc1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xff, 0x02, 0x00, 0x00,
        ]);
        let mut decoder = EncoderInstructionReader::new();
        assert_eq!(
            decoder.read_instructions(&mut test_receiver),
            Err(Error::EncoderStream)
        );

        let mut test_receiver: TestReceiver = TestReceiver::default();
        // EncoderInstruction::InsertWithNameRefStatic with overflow of garbage value.
        test_receiver.write(&[0xc1, 0x81, 0x00]);
        let mut decoder = EncoderInstructionReader::new();
        assert_eq!(
            decoder.read_instructions(&mut test_receiver),
            Err(Error::EncoderStream)
        );
    }
}
