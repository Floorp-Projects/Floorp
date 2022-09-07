// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::prefix::{
    DECODER_HEADER_ACK, DECODER_INSERT_COUNT_INCREMENT, DECODER_STREAM_CANCELLATION,
};
use crate::qpack_send_buf::QpackData;
use crate::reader::{IntReader, ReadByte};
use crate::Res;
use neqo_common::{qdebug, qtrace};
use neqo_transport::StreamId;
use std::mem;

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum DecoderInstruction {
    InsertCountIncrement { increment: u64 },
    HeaderAck { stream_id: StreamId },
    StreamCancellation { stream_id: StreamId },
    NoInstruction,
}

impl DecoderInstruction {
    fn get_instruction(b: u8) -> Self {
        if DECODER_HEADER_ACK.cmp_prefix(b) {
            Self::HeaderAck {
                stream_id: StreamId::from(0),
            }
        } else if DECODER_STREAM_CANCELLATION.cmp_prefix(b) {
            Self::StreamCancellation {
                stream_id: StreamId::from(0),
            }
        } else if DECODER_INSERT_COUNT_INCREMENT.cmp_prefix(b) {
            Self::InsertCountIncrement { increment: 0 }
        } else {
            unreachable!();
        }
    }

    pub(crate) fn marshal(&self, enc: &mut QpackData) {
        match self {
            Self::InsertCountIncrement { increment } => {
                enc.encode_prefixed_encoded_int(DECODER_INSERT_COUNT_INCREMENT, *increment);
            }
            Self::HeaderAck { stream_id } => {
                enc.encode_prefixed_encoded_int(DECODER_HEADER_ACK, stream_id.as_u64());
            }
            Self::StreamCancellation { stream_id } => {
                enc.encode_prefixed_encoded_int(DECODER_STREAM_CANCELLATION, stream_id.as_u64());
            }
            Self::NoInstruction => {}
        }
    }
}

#[derive(Debug)]
enum DecoderInstructionReaderState {
    ReadInstruction,
    ReadInt { reader: IntReader },
}

#[derive(Debug)]
pub struct DecoderInstructionReader {
    state: DecoderInstructionReaderState,
    instruction: DecoderInstruction,
}

impl ::std::fmt::Display for DecoderInstructionReader {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "InstructionReader")
    }
}

impl DecoderInstructionReader {
    pub fn new() -> Self {
        Self {
            state: DecoderInstructionReaderState::ReadInstruction,
            instruction: DecoderInstruction::NoInstruction,
        }
    }

    /// ### Errors
    ///  1) `NeedMoreData` if the reader needs more data
    ///  2) `ClosedCriticalStream`
    ///  3) other errors will be translated to `DecoderStream` by the caller of this function.
    pub fn read_instructions<R: ReadByte>(&mut self, recv: &mut R) -> Res<DecoderInstruction> {
        qdebug!([self], "read a new instraction");
        loop {
            match &mut self.state {
                DecoderInstructionReaderState::ReadInstruction => {
                    let b = recv.read_byte()?;
                    self.instruction = DecoderInstruction::get_instruction(b);
                    self.state = DecoderInstructionReaderState::ReadInt {
                        reader: IntReader::make(
                            b,
                            &[
                                DECODER_HEADER_ACK,
                                DECODER_STREAM_CANCELLATION,
                                DECODER_INSERT_COUNT_INCREMENT,
                            ],
                        ),
                    };
                }
                DecoderInstructionReaderState::ReadInt { reader } => {
                    let val = reader.read(recv)?;
                    qtrace!([self], "varint read {}", val);
                    match &mut self.instruction {
                        DecoderInstruction::InsertCountIncrement { increment: v } => {
                            *v = val;
                            self.state = DecoderInstructionReaderState::ReadInstruction;
                            break Ok(mem::replace(
                                &mut self.instruction,
                                DecoderInstruction::NoInstruction,
                            ));
                        }
                        DecoderInstruction::HeaderAck { stream_id: v }
                        | DecoderInstruction::StreamCancellation { stream_id: v } => {
                            *v = StreamId::from(val);
                            self.state = DecoderInstructionReaderState::ReadInstruction;
                            break Ok(mem::replace(
                                &mut self.instruction,
                                DecoderInstruction::NoInstruction,
                            ));
                        }
                        DecoderInstruction::NoInstruction => {
                            unreachable!("This instruction cannot be in this state.");
                        }
                    }
                }
            }
        }
    }
}

#[cfg(test)]
mod test {

    use super::{DecoderInstruction, DecoderInstructionReader, QpackData};
    use crate::reader::test_receiver::TestReceiver;
    use crate::Error;
    use neqo_transport::StreamId;

    fn test_encoding_decoding(instruction: DecoderInstruction) {
        let mut buf = QpackData::default();
        instruction.marshal(&mut buf);
        let mut test_receiver: TestReceiver = TestReceiver::default();
        test_receiver.write(&buf);
        let mut decoder = DecoderInstructionReader::new();
        assert_eq!(
            decoder.read_instructions(&mut test_receiver).unwrap(),
            instruction
        );
    }

    #[test]
    fn test_encoding_decoding_instructions() {
        test_encoding_decoding(DecoderInstruction::InsertCountIncrement { increment: 1 });
        test_encoding_decoding(DecoderInstruction::InsertCountIncrement { increment: 10_000 });

        test_encoding_decoding(DecoderInstruction::HeaderAck {
            stream_id: StreamId::new(1),
        });
        test_encoding_decoding(DecoderInstruction::HeaderAck {
            stream_id: StreamId::new(10_000),
        });

        test_encoding_decoding(DecoderInstruction::StreamCancellation {
            stream_id: StreamId::new(1),
        });
        test_encoding_decoding(DecoderInstruction::StreamCancellation {
            stream_id: StreamId::new(10_000),
        });
    }

    fn test_encoding_decoding_slow_reader(instruction: DecoderInstruction) {
        let mut buf = QpackData::default();
        instruction.marshal(&mut buf);
        let mut test_receiver: TestReceiver = TestReceiver::default();
        let mut decoder = DecoderInstructionReader::new();
        for i in 0..buf.len() - 1 {
            test_receiver.write(&buf[i..=i]);
            assert_eq!(
                decoder.read_instructions(&mut test_receiver),
                Err(Error::NeedMoreData)
            );
        }
        test_receiver.write(&buf[buf.len() - 1..buf.len()]);
        assert_eq!(
            decoder.read_instructions(&mut test_receiver).unwrap(),
            instruction
        );
    }

    #[test]
    fn test_encoding_decoding_instructions_slow_reader() {
        test_encoding_decoding_slow_reader(DecoderInstruction::InsertCountIncrement {
            increment: 10_000,
        });
        test_encoding_decoding_slow_reader(DecoderInstruction::HeaderAck {
            stream_id: StreamId::new(10_000),
        });
        test_encoding_decoding_slow_reader(DecoderInstruction::StreamCancellation {
            stream_id: StreamId::new(10_000),
        });
    }

    #[test]
    fn test_decoding_error() {
        let mut test_receiver: TestReceiver = TestReceiver::default();
        // InsertCountIncrement with overflow
        test_receiver.write(&[
            0x3f, 0xc1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xff, 0x02,
        ]);
        let mut decoder = DecoderInstructionReader::new();
        assert_eq!(
            decoder.read_instructions(&mut test_receiver),
            Err(Error::IntegerOverflow)
        );

        let mut test_receiver: TestReceiver = TestReceiver::default();
        // StreamCancellation with overflow
        test_receiver.write(&[
            0x7f, 0xc1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xff, 0x02,
        ]);
        let mut decoder = DecoderInstructionReader::new();
        assert_eq!(
            decoder.read_instructions(&mut test_receiver),
            Err(Error::IntegerOverflow)
        );

        let mut test_receiver: TestReceiver = TestReceiver::default();
        // HeaderAck with overflow
        test_receiver.write(&[
            0x7f, 0xc1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xff, 0x02,
        ]);
        let mut decoder = DecoderInstructionReader::new();
        assert_eq!(
            decoder.read_instructions(&mut test_receiver),
            Err(Error::IntegerOverflow)
        );
    }
}
