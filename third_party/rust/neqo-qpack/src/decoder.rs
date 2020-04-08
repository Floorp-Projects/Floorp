// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::decoder_instructions::DecoderInstruction;
use crate::encoder_instructions::{EncoderInstruction, EncoderInstructionReader};
use crate::header_block::{HeaderDecoder, HeaderDecoderResult};
use crate::qpack_send_buf::QPData;
use crate::reader::ReceiverConnWrapper;
use crate::table::HeaderTable;
use crate::Header;
use crate::{Error, Res};
use neqo_common::qdebug;
use neqo_transport::Connection;
use std::convert::TryInto;

pub const QPACK_UNI_STREAM_TYPE_DECODER: u64 = 0x3;

#[derive(Debug)]
pub struct QPackDecoder {
    instruction_reader: EncoderInstructionReader,
    table: HeaderTable,
    total_num_of_inserts: u64,
    max_entries: u64,
    send_buf: QPData,
    local_stream_id: Option<u64>,
    remote_stream_id: Option<u64>,
    max_table_size: u64,
    max_blocked_streams: usize,
    blocked_streams: Vec<(u64, u64)>, //stream_id and requested inserts count.
}

impl QPackDecoder {
    #[must_use]
    pub fn new(max_table_size: u64, max_blocked_streams: u16) -> Self {
        qdebug!("Decoder: creating a new qpack decoder.");
        Self {
            instruction_reader: EncoderInstructionReader::new(),
            table: HeaderTable::new(false),
            total_num_of_inserts: 0,
            max_entries: max_table_size >> 5,
            send_buf: QPData::default(),
            local_stream_id: None,
            remote_stream_id: None,
            max_table_size,
            max_blocked_streams: max_blocked_streams.try_into().unwrap(),
            blocked_streams: Vec::new(),
        }
    }

    #[must_use]
    fn capacity(&self) -> u64 {
        self.table.capacity()
    }

    #[must_use]
    pub fn get_max_table_size(&self) -> u64 {
        self.max_table_size
    }

    #[must_use]
    pub fn get_blocked_streams(&self) -> u16 {
        self.max_blocked_streams.try_into().unwrap()
    }

    /// returns a list of unblocked streams
    /// # Errors
    /// May return: `ClosedCriticalStream` if stream has been closed or `EncoderStream`
    /// in case of any other transport error.
    pub fn receive(&mut self, conn: &mut Connection, stream_id: u64) -> Res<Vec<u64>> {
        self.read_instructions(conn, stream_id)?;
        let base = self.table.base();
        let r = self
            .blocked_streams
            .iter()
            .filter_map(|(id, req)| if *req <= base { Some(*id) } else { None })
            .collect::<Vec<_>>();
        self.blocked_streams.retain(|(_, req)| *req > base);
        Ok(r)
    }

    fn read_instructions(&mut self, conn: &mut Connection, stream_id: u64) -> Res<()> {
        loop {
            let mut recv = ReceiverConnWrapper::new(conn, stream_id);
            match self.instruction_reader.read_instructions(&mut recv)? {
                Some(instruction) => self.execute_instruction(instruction)?,
                None => break Ok(()),
            }
        }
    }

    fn execute_instruction(&mut self, instruction: EncoderInstruction) -> Res<()> {
        match instruction {
            EncoderInstruction::Capacity { value } => self.set_capacity(value)?,
            EncoderInstruction::InsertWithNameRefStatic { index, value } => {
                self.table.insert_with_name_ref(true, index, &value)?;
                self.total_num_of_inserts += 1;
            }
            EncoderInstruction::InsertWithNameRefDynamic { index, value } => {
                self.table.insert_with_name_ref(false, index, &value)?;
                self.total_num_of_inserts += 1;
            }
            EncoderInstruction::InsertWithNameLiteral { name, value } => {
                self.table.insert(&name, &value).map(|_| ())?;
                self.total_num_of_inserts += 1;
            }
            EncoderInstruction::Duplicate { index } => {
                self.table.duplicate(index)?;
                self.total_num_of_inserts += 1;
            }
            EncoderInstruction::NoInstruction => {
                unreachable!("This can be call only with an instruction.")
            }
        }
        Ok(())
    }

    fn set_capacity(&mut self, cap: u64) -> Res<()> {
        qdebug!([self], "received instruction capacity cap={}", cap);
        if cap > self.max_table_size {
            return Err(Error::EncoderStream);
        }
        self.table
            .set_capacity(cap)
            .map_err(|_| Error::EncoderStream)
    }

    fn header_ack(&mut self, stream_id: u64, required_inserts: u64) {
        DecoderInstruction::HeaderAck { stream_id }.marshal(&mut self.send_buf);
        if required_inserts > self.table.get_acked_inserts_cnt() {
            let ack_increment_delta = required_inserts - self.table.get_acked_inserts_cnt();
            self.table
                .increment_acked(ack_increment_delta)
                .expect("This should never happen");
        }
    }

    pub fn cancel_stream(&mut self, stream_id: u64) {
        DecoderInstruction::StreamCancellation { stream_id }.marshal(&mut self.send_buf);
    }

    /// # Errors
    ///     May return DecoderStream in case of any transport error.
    pub fn send(&mut self, conn: &mut Connection) -> Res<()> {
        // Encode increment instruction if needed.
        let increment = self.total_num_of_inserts - self.table.get_acked_inserts_cnt();
        if increment > 0 {
            DecoderInstruction::InsertCountIncrement { increment }.marshal(&mut self.send_buf);
            self.table
                .increment_acked(increment)
                .expect("This should never happen");
        }
        if self.send_buf.len() == 0 {
            Ok(())
        } else if let Some(stream_id) = self.local_stream_id {
            match conn.stream_send(stream_id, &self.send_buf[..]) {
                Err(_) => Err(Error::DecoderStream),
                Ok(r) => {
                    qdebug!([self], "{} bytes sent.", r);
                    self.send_buf.read(r as usize);
                    Ok(())
                }
            }
        } else {
            Ok(())
        }
    }

    /// This function returns None if the stream is blocked waiting for table insertions.
    /// 'buf' must contain the complete header block.
    /// # Errors
    /// May return `DecompressionFailed` if header block is incorrect or incomplete.
    pub fn decode_header_block(&mut self, buf: &[u8], stream_id: u64) -> Res<Option<Vec<Header>>> {
        qdebug!([self], "decode header block.");
        let mut decoder = HeaderDecoder::new(buf);

        match decoder.decode_header_block(
            &self.table,
            self.max_entries,
            self.total_num_of_inserts,
        )? {
            HeaderDecoderResult::Blocked(req_insert_cnt) => {
                self.blocked_streams.push((stream_id, req_insert_cnt));
                if self.blocked_streams.len() > self.max_blocked_streams {
                    Err(Error::DecompressionFailed)
                } else {
                    Ok(None)
                }
            }
            HeaderDecoderResult::Headers(h) => {
                if decoder.get_req_insert_cnt() != 0 {
                    self.header_ack(stream_id, decoder.get_req_insert_cnt());
                }
                Ok(Some(h))
            }
        }
    }

    #[must_use]
    pub fn is_recv_stream(&self, stream_id: u64) -> bool {
        match self.remote_stream_id {
            Some(id) => id == stream_id,
            None => false,
        }
    }

    pub fn add_send_stream(&mut self, stream_id: u64) {
        if self.local_stream_id.is_some() {
            panic!("Adding multiple local streams");
        }
        self.local_stream_id = Some(stream_id);
        self.send_buf.encode_varint(QPACK_UNI_STREAM_TYPE_DECODER);
    }

    /// # Errors
    ///     May return WrongStreamCount if Http3 has received multiple encoder streams.
    pub fn add_recv_stream(&mut self, stream_id: u64) -> Res<()> {
        if self.remote_stream_id.is_some() {
            Err(Error::WrongStreamCount)
        } else {
            self.remote_stream_id = Some(stream_id);
            Ok(())
        }
    }
}

impl ::std::fmt::Display for QPackDecoder {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "QPackDecoder {}", self.capacity())
    }
}

#[cfg(test)]
mod tests {
    use super::{Connection, Error, Header, QPackDecoder, Res};
    use neqo_transport::StreamType;
    use std::convert::TryInto;
    use test_fixture::now;

    struct TestDecoder {
        decoder: QPackDecoder,
        send_stream_id: u64,
        recv_stream_id: u64,
        conn: Connection,
        peer_conn: Connection,
    }

    fn connect() -> TestDecoder {
        let (mut conn, mut peer_conn) = test_fixture::connect();

        // create a stream
        let recv_stream_id = peer_conn.stream_create(StreamType::UniDi).unwrap();
        let send_stream_id = conn.stream_create(StreamType::UniDi).unwrap();

        // create a decoder
        let mut decoder = QPackDecoder::new(300, 100);
        decoder.add_send_stream(send_stream_id);

        TestDecoder {
            decoder,
            send_stream_id,
            recv_stream_id,
            conn,
            peer_conn,
        }
    }

    fn recv_instruction(decoder: &mut TestDecoder, encoder_instruction: &[u8], res: &Res<()>) {
        let _ = decoder
            .peer_conn
            .stream_send(decoder.recv_stream_id, encoder_instruction);
        let out = decoder.peer_conn.process(None, now());
        decoder.conn.process(out.dgram(), now());
        assert_eq!(
            decoder
                .decoder
                .read_instructions(&mut decoder.conn, decoder.recv_stream_id),
            *res
        );
    }

    fn send_instructions_and_check(decoder: &mut TestDecoder, decoder_instruction: &[u8]) {
        decoder.decoder.send(&mut decoder.conn).unwrap();
        let out = decoder.conn.process(None, now());
        decoder.peer_conn.process(out.dgram(), now());
        let mut buf = [0_u8; 100];
        let (amount, fin) = decoder
            .peer_conn
            .stream_recv(decoder.send_stream_id, &mut buf)
            .unwrap();
        assert_eq!(fin, false);
        assert_eq!(&buf[..amount], decoder_instruction);
    }

    fn decode_headers(
        decoder: &mut TestDecoder,
        header_block: &[u8],
        headers: &[Header],
        stream_id: u64,
    ) {
        let decoded_headers = decoder
            .decoder
            .decode_header_block(header_block, stream_id)
            .unwrap();
        let h = decoded_headers.unwrap();
        assert_eq!(h, headers);
    }

    fn test_instruction(
        capacity: u64,
        instruction: &[u8],
        res: &Res<()>,
        decoder_instruction: &[u8],
        check_capacity: u64,
    ) {
        let mut decoder = connect();

        if capacity > 0 {
            assert!(decoder.decoder.set_capacity(capacity).is_ok());
        }

        // recv an instruction
        recv_instruction(&mut decoder, instruction, res);

        // send decoder instruction and check that is what we expect.
        send_instructions_and_check(&mut decoder, decoder_instruction);

        if check_capacity > 0 {
            assert_eq!(decoder.decoder.capacity(), check_capacity);
        }
    }

    // test insert_with_name_ref which fails because there is not enough space in the table
    #[test]
    fn test_recv_insert_with_name_ref_1() {
        test_instruction(
            0,
            &[0xc4, 0x04, 0x31, 0x32, 0x33, 0x34],
            &Err(Error::DecoderStream),
            &[0x03],
            0,
        );
    }

    // test insert_name_ref that succeeds
    #[test]
    fn test_recv_insert_with_name_ref_2() {
        test_instruction(
            100,
            &[0xc4, 0x04, 0x31, 0x32, 0x33, 0x34],
            &Ok(()),
            &[0x03, 0x01],
            0,
        );
    }

    // test insert with name literal - succeeds
    #[test]
    fn test_recv_insert_with_name_litarel_2() {
        test_instruction(
            200,
            &[
                0x4e, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x6c, 0x65, 0x6e, 0x67, 0x74,
                0x68, 0x04, 0x31, 0x32, 0x33, 0x34,
            ],
            &Ok(()),
            &[0x03, 0x01],
            0,
        );
    }

    #[test]
    fn test_recv_change_capacity() {
        test_instruction(0, &[0x3f, 0xa9, 0x01], &Ok(()), &[0x03], 200);
    }

    #[test]
    fn test_recv_change_capacity_too_big() {
        test_instruction(
            0,
            &[0x3f, 0xf1, 0x02],
            &Err(Error::EncoderStream),
            &[0x03],
            0,
        );
    }

    // this test tests header decoding, the header acks command and the insert count increment command.
    #[test]
    fn test_duplicate() {
        let mut decoder = connect();

        assert!(decoder.decoder.set_capacity(100).is_ok());

        // receive an instruction
        recv_instruction(
            &mut decoder,
            &[
                0x4e, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x6c, 0x65, 0x6e, 0x67, 0x74,
                0x68, 0x04, 0x31, 0x32, 0x33, 0x34,
            ],
            &Ok(()),
        );

        // receive the second instruction, a duplicate instruction.
        recv_instruction(&mut decoder, &[0x00], &Ok(()));

        send_instructions_and_check(&mut decoder, &[0x03, 0x02]);
    }

    struct TestElement {
        pub headers: Vec<Header>,
        pub header_block: &'static [u8],
        pub encoder_inst: &'static [u8],
    }

    #[test]
    fn test_encode_incr_encode_header_ack_some() {
        // 1. Decoder receives an instruction (header and value both as literal)
        // 2. Decoder process the instruction and sends an increment instruction.
        // 3. Decoder receives another two instruction (header and value both as literal) and
        //    a header block.
        // 4. Now it sends only a header ack and an increment instruction with increment==1.
        let headers = vec![
            (String::from("my-headera"), String::from("my-valuea")),
            (String::from("my-headerb"), String::from("my-valueb")),
        ];
        let header_block = &[0x03, 0x81, 0x10, 0x11];
        let first_encoder_inst = &[
            0x4a, 0x6d, 0x79, 0x2d, 0x68, 0x65, 0x61, 0x64, 0x65, 0x72, 0x61, 0x09, 0x6d, 0x79,
            0x2d, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x61,
        ];
        let second_encoder_inst = &[
            0x4a, 0x6d, 0x79, 0x2d, 0x68, 0x65, 0x61, 0x64, 0x65, 0x72, 0x62, 0x09, 0x6d, 0x79,
            0x2d, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x62, 0x4a, 0x6d, 0x79, 0x2d, 0x68, 0x65, 0x61,
            0x64, 0x65, 0x72, 0x63, 0x09, 0x6d, 0x79, 0x2d, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x63,
        ];

        let mut decoder = connect();

        assert!(decoder.decoder.set_capacity(200).is_ok());

        recv_instruction(&mut decoder, first_encoder_inst, &Ok(()));

        send_instructions_and_check(&mut decoder, &[0x03, 0x1]);

        recv_instruction(&mut decoder, second_encoder_inst, &Ok(()));

        decode_headers(&mut decoder, header_block, &headers, 0);

        send_instructions_and_check(&mut decoder, &[0x80, 0x1]);
    }

    #[test]
    fn test_encode_incr_encode_header_ack_all() {
        // 1. Decoder receives an instruction (header and value both as literal)
        // 2. Decoder process the instruction and sends an increment instruction.
        // 3. Decoder receives another instruction (header and value both as literal) and
        //    a header block.
        // 4. Now it sends only a header ack.
        let headers = vec![
            (String::from("my-headera"), String::from("my-valuea")),
            (String::from("my-headerb"), String::from("my-valueb")),
        ];
        let header_block = &[0x03, 0x81, 0x10, 0x11];
        let first_encoder_inst = &[
            0x4a, 0x6d, 0x79, 0x2d, 0x68, 0x65, 0x61, 0x64, 0x65, 0x72, 0x61, 0x09, 0x6d, 0x79,
            0x2d, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x61,
        ];
        let second_encoder_inst = &[
            0x4a, 0x6d, 0x79, 0x2d, 0x68, 0x65, 0x61, 0x64, 0x65, 0x72, 0x62, 0x09, 0x6d, 0x79,
            0x2d, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x62,
        ];

        let mut decoder = connect();

        assert!(decoder.decoder.set_capacity(200).is_ok());

        recv_instruction(&mut decoder, first_encoder_inst, &Ok(()));

        send_instructions_and_check(&mut decoder, &[0x03, 0x1]);

        recv_instruction(&mut decoder, second_encoder_inst, &Ok(()));

        decode_headers(&mut decoder, header_block, &headers, 0);

        send_instructions_and_check(&mut decoder, &[0x80]);
    }

    #[test]
    fn test_header_ack_all() {
        // Send two instructions to insert values into the dynamic table and then send a header
        // that references them both. The result should be only a header acknowledgement.
        let headers = vec![
            (String::from("my-headera"), String::from("my-valuea")),
            (String::from("my-headerb"), String::from("my-valueb")),
        ];
        let header_block = &[0x03, 0x81, 0x10, 0x11];
        let encoder_inst = &[
            0x4a, 0x6d, 0x79, 0x2d, 0x68, 0x65, 0x61, 0x64, 0x65, 0x72, 0x61, 0x09, 0x6d, 0x79,
            0x2d, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x61, 0x4a, 0x6d, 0x79, 0x2d, 0x68, 0x65, 0x61,
            0x64, 0x65, 0x72, 0x62, 0x09, 0x6d, 0x79, 0x2d, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x62,
        ];

        let mut decoder = connect();

        assert!(decoder.decoder.set_capacity(200).is_ok());

        recv_instruction(&mut decoder, encoder_inst, &Ok(()));

        decode_headers(&mut decoder, header_block, &headers, 0);

        send_instructions_and_check(&mut decoder, &[0x03, 0x80]);
    }

    #[test]
    fn test_header_ack_and_incr_instruction() {
        // Send two instructions to insert values into the dynamic table and then send a header
        // that references only the first. The result should be a header acknowledgement and a
        // increment instruction.
        let headers = vec![(String::from("my-headera"), String::from("my-valuea"))];
        let header_block = &[0x02, 0x80, 0x10];
        let encoder_inst = &[
            0x4a, 0x6d, 0x79, 0x2d, 0x68, 0x65, 0x61, 0x64, 0x65, 0x72, 0x61, 0x09, 0x6d, 0x79,
            0x2d, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x61, 0x4a, 0x6d, 0x79, 0x2d, 0x68, 0x65, 0x61,
            0x64, 0x65, 0x72, 0x62, 0x09, 0x6d, 0x79, 0x2d, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x62,
        ];

        let mut decoder = connect();

        assert!(decoder.decoder.set_capacity(200).is_ok());

        recv_instruction(&mut decoder, encoder_inst, &Ok(()));

        decode_headers(&mut decoder, header_block, &headers, 0);

        send_instructions_and_check(&mut decoder, &[0x03, 0x80, 0x01]);
    }

    #[test]
    fn test_header_block_decoder() {
        let test_cases: [TestElement; 6] = [
            // test a header with ref to static - encode_indexed
            TestElement {
                headers: vec![(String::from(":method"), String::from("GET"))],
                header_block: &[0x00, 0x00, 0xd1],
                encoder_inst: &[],
            },
            // test encode_literal_with_name_ref
            TestElement {
                headers: vec![(String::from(":path"), String::from("/somewhere"))],
                header_block: &[
                    0x00, 0x00, 0x51, 0x0a, 0x2f, 0x73, 0x6f, 0x6d, 0x65, 0x77, 0x68, 0x65, 0x72,
                    0x65,
                ],
                encoder_inst: &[],
            },
            // test adding a new header and encode_post_base_index, also test fix_header_block_prefix
            TestElement {
                headers: vec![(String::from("my-header"), String::from("my-value"))],
                header_block: &[0x02, 0x80, 0x10],
                encoder_inst: &[
                    0x49, 0x6d, 0x79, 0x2d, 0x68, 0x65, 0x61, 0x64, 0x65, 0x72, 0x08, 0x6d, 0x79,
                    0x2d, 0x76, 0x61, 0x6c, 0x75, 0x65,
                ],
            },
            // test encode_indexed with a ref to dynamic table.
            TestElement {
                headers: vec![(String::from("my-header"), String::from("my-value"))],
                header_block: &[0x02, 0x00, 0x80],
                encoder_inst: &[],
            },
            // test encode_literal_with_name_ref.
            TestElement {
                headers: vec![(String::from("my-header"), String::from("my-value2"))],
                header_block: &[
                    0x02, 0x00, 0x40, 0x09, 0x6d, 0x79, 0x2d, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x32,
                ],
                encoder_inst: &[],
            },
            // test multiple headers
            TestElement {
                headers: vec![
                    (String::from(":method"), String::from("GET")),
                    (String::from(":path"), String::from("/somewhere")),
                    (String::from(":authority"), String::from("example.com")),
                    (String::from(":scheme"), String::from("https")),
                ],
                header_block: &[
                    0x00, 0x01, 0xd1, 0x51, 0x0a, 0x2f, 0x73, 0x6f, 0x6d, 0x65, 0x77, 0x68, 0x65,
                    0x72, 0x65, 0x50, 0x0b, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63,
                    0x6f, 0x6d, 0xd7,
                ],
                encoder_inst: &[],
            },
        ];

        let mut decoder = connect();

        assert!(decoder.decoder.set_capacity(200).is_ok());

        for (i, t) in test_cases.iter().enumerate() {
            // receive an instruction
            if !t.encoder_inst.is_empty() {
                recv_instruction(&mut decoder, t.encoder_inst, &Ok(()));
            }

            decode_headers(
                &mut decoder,
                t.header_block,
                &t.headers,
                i.try_into().unwrap(),
            );
        }

        // test header acks and the insert count increment command
        send_instructions_and_check(&mut decoder, &[0x03, 0x82, 0x83, 0x84]);
    }

    #[test]
    fn test_header_block_decoder_huffman() {
        let test_cases: [TestElement; 6] = [
            // test a header with ref to static - encode_indexed
            TestElement {
                headers: vec![(String::from(":method"), String::from("GET"))],
                header_block: &[0x00, 0x00, 0xd1],
                encoder_inst: &[],
            },
            // test encode_literal_with_name_ref
            TestElement {
                headers: vec![(String::from(":path"), String::from("/somewhere"))],
                header_block: &[
                    0x00, 0x00, 0x51, 0x87, 0x61, 0x07, 0xa4, 0xbe, 0x27, 0x2d, 0x85,
                ],
                encoder_inst: &[],
            },
            // test adding a new header and encode_post_base_index, also test fix_header_block_prefix
            TestElement {
                headers: vec![(String::from("my-header"), String::from("my-value"))],
                header_block: &[0x02, 0x80, 0x10],
                encoder_inst: &[
                    0x67, 0xa7, 0xd2, 0xd3, 0x94, 0x72, 0x16, 0xcf, 0x86, 0xa7, 0xd2, 0xdd, 0xc7,
                    0x45, 0xa5,
                ],
            },
            // test encode_indexed with a ref to dynamic table.
            TestElement {
                headers: vec![(String::from("my-header"), String::from("my-value"))],
                header_block: &[0x02, 0x00, 0x80],
                encoder_inst: &[],
            },
            // test encode_literal_with_name_ref.
            TestElement {
                headers: vec![(String::from("my-header"), String::from("my-value2"))],
                header_block: &[
                    0x02, 0x00, 0x40, 0x87, 0xa7, 0xd2, 0xdd, 0xc7, 0x45, 0xa5, 0x17,
                ],
                encoder_inst: &[],
            },
            // test multiple headers
            TestElement {
                headers: vec![
                    (String::from(":method"), String::from("GET")),
                    (String::from(":path"), String::from("/somewhere")),
                    (String::from(":authority"), String::from("example.com")),
                    (String::from(":scheme"), String::from("https")),
                ],
                header_block: &[
                    0x00, 0x01, 0xd1, 0x51, 0x87, 0x61, 0x07, 0xa4, 0xbe, 0x27, 0x2d, 0x85, 0x50,
                    0x88, 0x2f, 0x91, 0xd3, 0x5d, 0x05, 0x5c, 0x87, 0xa7, 0xd7,
                ],
                encoder_inst: &[],
            },
        ];

        let mut decoder = connect();

        assert!(decoder.decoder.set_capacity(200).is_ok());

        for (i, t) in test_cases.iter().enumerate() {
            // receive an instruction.
            if !t.encoder_inst.is_empty() {
                recv_instruction(&mut decoder, t.encoder_inst, &Ok(()));
            }

            decode_headers(
                &mut decoder,
                t.header_block,
                &t.headers,
                i.try_into().unwrap(),
            );
        }

        // test header acks and the insert count increment command
        send_instructions_and_check(&mut decoder, &[0x03, 0x82, 0x83, 0x84]);
    }

    #[test]
    fn test_subtract_overflow_in_header_ack() {
        const HEADER_BLOCK_1: &[u8] = &[0x03, 0x81, 0x10, 0x11];
        const ENCODER_INST: &[u8] = &[
            0x4a, 0x6d, 0x79, 0x2d, 0x68, 0x65, 0x61, 0x64, 0x65, 0x72, 0x61, 0x09, 0x6d, 0x79,
            0x2d, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x61, 0x4a, 0x6d, 0x79, 0x2d, 0x68, 0x65, 0x61,
            0x64, 0x65, 0x72, 0x62, 0x09, 0x6d, 0x79, 0x2d, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x62,
        ];
        const HEADER_BLOCK_2: &[u8] = &[0x02, 0x80, 0x10];
        // Test for issue https://github.com/mozilla/neqo/issues/475
        // Send two instructions to insert values into the dynamic table and send a header
        // that references them both. This will increase number of acked inserts in the table
        // to 2. Then send a header that references only one of them which shouldn't increase
        // number of acked inserts.
        let headers = vec![
            (String::from("my-headera"), String::from("my-valuea")),
            (String::from("my-headerb"), String::from("my-valueb")),
        ];

        let mut decoder = connect();

        assert!(decoder.decoder.set_capacity(200).is_ok());

        recv_instruction(&mut decoder, ENCODER_INST, &Ok(()));

        decode_headers(&mut decoder, HEADER_BLOCK_1, &headers, 0);

        let headers = vec![(String::from("my-headera"), String::from("my-valuea"))];

        decode_headers(&mut decoder, HEADER_BLOCK_2, &headers, 0);
    }
}
