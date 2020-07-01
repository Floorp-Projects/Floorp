// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::decoder_instructions::{DecoderInstruction, DecoderInstructionReader};
use crate::encoder_instructions::EncoderInstruction;
use crate::header_block::HeaderEncoder;
use crate::qlog;
use crate::qpack_send_buf::QPData;
use crate::reader::ReceiverConnWrapper;
use crate::stats::Stats;
use crate::table::{HeaderTable, LookupResult, ADDITIONAL_TABLE_ENTRY_SIZE};
use crate::Header;
use crate::{Error, QpackSettings, Res};
use neqo_common::{qdebug, qlog::NeqoQlog, qtrace};
use neqo_transport::Connection;
use std::collections::{HashMap, HashSet, VecDeque};
use std::convert::TryFrom;

pub const QPACK_UNI_STREAM_TYPE_ENCODER: u64 = 0x2;

#[derive(Debug)]
pub struct QPackEncoder {
    table: HeaderTable,
    send_buf: QPData,
    max_table_size: u64,
    max_entries: u64,
    instruction_reader: DecoderInstructionReader,
    local_stream_id: Option<u64>,
    remote_stream_id: Option<u64>,
    max_blocked_streams: u16,
    // Remember header blocks that are referring to dynamic table.
    // There can be multiple header blocks in one stream, headers, trailer, push stream request, etc.
    // This HashMap maps a stream ID to a list of header blocks. Each header block is a list of
    // referenced dynamic table entries.
    unacked_header_blocks: HashMap<u64, VecDeque<HashSet<u64>>>,
    blocked_stream_cnt: u16,
    use_huffman: bool,
    stats: Stats,
}

impl QPackEncoder {
    #[must_use]
    pub fn new(qpack_settings: QpackSettings, use_huffman: bool) -> Self {
        Self {
            table: HeaderTable::new(true),
            send_buf: QPData::default(),
            max_table_size: qpack_settings.max_table_size_encoder,
            max_entries: 0,
            instruction_reader: DecoderInstructionReader::new(),
            local_stream_id: None,
            remote_stream_id: None,
            max_blocked_streams: 0,
            unacked_header_blocks: HashMap::new(),
            blocked_stream_cnt: 0,
            use_huffman,
            stats: Stats::default(),
        }
    }

    /// This function is use for setting encoders table max capacity. The value is received as
    /// a `SETTINGS_QPACK_MAX_TABLE_CAPACITY` setting parameter.
    /// # Errors
    /// `EncoderStream` if value is too big.
    /// `ChangeCapacity` if table capacity cannot be reduced.
    pub fn set_max_capacity(&mut self, cap: u64) -> Res<()> {
        if cap > (1 << 30) - 1 {
            return Err(Error::EncoderStream);
        }

        if cap == self.table.capacity() {
            return Ok(());
        }

        qdebug!(
            [self],
            "Set max capacity to new capacity:{} old:{} max_table_size={}.",
            cap,
            self.table.capacity(),
            self.max_table_size,
        );
        let new_cap = std::cmp::min(self.max_table_size, cap);
        self.max_entries = new_cap / 32;
        // we also set our table to the max allowed.
        self.change_capacity(new_cap)
    }

    /// This function is use for setting encoders max blocked streams. The value is received as
    /// a `SETTINGS_QPACK_BLOCKED_STREAMS` setting parameter.
    /// # Errors
    /// `EncoderStream` if value is too big.
    pub fn set_max_blocked_streams(&mut self, blocked_streams: u64) -> Res<()> {
        self.max_blocked_streams = u16::try_from(blocked_streams).or(Err(Error::EncoderStream))?;
        Ok(())
    }

    /// Reads decoder instructions.
    /// # Errors
    /// May return: `ClosedCriticalStream` if stream has been closed or `DecoderStream`
    /// in case of any other transport error.
    pub fn recv_if_encoder_stream(&mut self, conn: &mut Connection, stream_id: u64) -> Res<bool> {
        match self.remote_stream_id {
            Some(id) => {
                if id == stream_id {
                    self.read_instructions(conn, stream_id)
                        .map_err(|e| map_error(&e))?;
                    Ok(true)
                } else {
                    Ok(false)
                }
            }
            None => Ok(false),
        }
    }

    fn read_instructions(&mut self, conn: &mut Connection, stream_id: u64) -> Res<()> {
        qdebug!([self], "read a new instraction");
        loop {
            let mut recv = ReceiverConnWrapper::new(conn, stream_id);
            match self.instruction_reader.read_instructions(&mut recv) {
                Ok(instruction) => {
                    self.call_instruction(instruction, &mut conn.qlog_mut().borrow_mut())?
                }
                Err(Error::NeedMoreData) => break Ok(()),
                Err(e) => break Err(e),
            }
        }
    }

    fn recalculate_blocked_streams(&mut self) {
        let acked_inserts_cnt = self.table.get_acked_inserts_cnt();
        self.blocked_stream_cnt = 0;
        for hb_list in self.unacked_header_blocks.values_mut() {
            debug_assert!(!hb_list.is_empty());
            if hb_list.iter().flatten().any(|e| *e >= acked_inserts_cnt) {
                self.blocked_stream_cnt += 1;
            }
        }
    }

    fn insert_count_instruction(&mut self, increment: u64) -> Res<()> {
        self.table
            .increment_acked(increment)
            .map_err(|_| Error::DecoderStream)?;
        self.recalculate_blocked_streams();
        Ok(())
    }

    fn header_ack(&mut self, stream_id: u64) -> Res<()> {
        let mut new_acked = self.table.get_acked_inserts_cnt();
        if let Some(hb_list) = self.unacked_header_blocks.get_mut(&stream_id) {
            if let Some(ref_list) = hb_list.pop_back() {
                for iter in ref_list {
                    self.table.remove_ref(iter);
                    if iter >= new_acked {
                        new_acked = iter + 1;
                    }
                }
            } else {
                debug_assert!(false, "We should have at least one header block.");
            }
            if hb_list.is_empty() {
                self.unacked_header_blocks.remove(&stream_id);
            }
        }
        if new_acked > self.table.get_acked_inserts_cnt() {
            self.insert_count_instruction(new_acked - self.table.get_acked_inserts_cnt())
                .expect("This should neve happen");
        }
        Ok(())
    }

    fn stream_cancellation(&mut self, stream_id: u64) -> Res<()> {
        let mut was_blocker = false;
        if let Some(hb_list) = self.unacked_header_blocks.get_mut(&stream_id) {
            debug_assert!(!hb_list.is_empty());
            while let Some(ref_list) = hb_list.pop_front() {
                for iter in ref_list {
                    self.table.remove_ref(iter);
                    was_blocker = was_blocker || (iter >= self.table.get_acked_inserts_cnt());
                }
            }
        }
        if was_blocker {
            debug_assert!(self.blocked_stream_cnt > 0);
            self.blocked_stream_cnt -= 1;
        }
        Ok(())
    }

    fn call_instruction(
        &mut self,
        instruction: DecoderInstruction,
        qlog: &mut Option<NeqoQlog>,
    ) -> Res<()> {
        qdebug!([self], "call intruction {:?}", instruction);
        match instruction {
            DecoderInstruction::InsertCountIncrement { increment } => {
                qlog::qpack_read_insert_count_increment_instruction(
                    qlog,
                    increment,
                    &increment.to_be_bytes(),
                )?;

                self.insert_count_instruction(increment)
            }
            DecoderInstruction::HeaderAck { stream_id } => self.header_ack(stream_id),
            DecoderInstruction::StreamCancellation { stream_id } => {
                self.stream_cancellation(stream_id)
            }
            _ => Ok(()),
        }
    }

    /// Inserts a new entry into a table and sends the corresponding instruction to a peer. An entry is added only
    /// if it is possible to send the corresponding instruction immediately, i.e. the encoder stream is not
    /// blocked by the flow control (or stream internal buffer(this is very unlikely)).
    /// ### Errors
    /// `EncoderStreamBlocked` if the encoder stream is blocked by the flow control.
    /// `DynamicTableFull` if the dynamic table does not have enough space for the entry.
    /// The function can return transport errors: `InvalidStreamId`, `InvalidInput` and `FinalSizeError`.
    pub fn send_and_insert(
        &mut self,
        conn: &mut Connection,
        name: &[u8],
        value: &[u8],
    ) -> Res<u64> {
        qdebug!([self], "insert {:?} {:?}.", name, value);
        self.send(conn)?;
        if self.send_buf.len() != 0 {
            return Err(Error::EncoderStreamBlocked);
        }

        let entry_size = name.len() + value.len() + ADDITIONAL_TABLE_ENTRY_SIZE;

        if !self.table.insert_possible(entry_size) {
            return Err(Error::DynamicTableFull);
        }

        let mut buf = QPData::default();
        EncoderInstruction::InsertWithNameLiteral {
            name: &name,
            value: &value,
        }
        .marshal(&mut buf, self.use_huffman);

        let stream_id = self.local_stream_id.ok_or(Error::Internal)?;

        let sent = conn.stream_send_atomic(stream_id, &buf)?;
        if !sent {
            return Err(Error::EncoderStreamBlocked);
        }

        self.stats.dynamic_table_inserts += 1;

        match self.table.insert(name, value) {
            Ok(inx) => Ok(inx),
            Err(e) => {
                debug_assert!(false);
                Err(e)
            }
        }
    }

    fn change_capacity(&mut self, value: u64) -> Res<()> {
        qdebug!([self], "change capacity: {}", value);
        self.table.set_capacity(value)?;
        EncoderInstruction::Capacity { value }.marshal(&mut self.send_buf, self.use_huffman);
        Ok(())
    }

    /// Sends any qpack encoder instructions.
    /// # Errors
    ///   returns `EncoderStream` in case of an error.
    pub fn send(&mut self, conn: &mut Connection) -> Res<()> {
        if self.send_buf.is_empty() {
            Ok(())
        } else if let Some(stream_id) = self.local_stream_id {
            let r = conn
                .stream_send(stream_id, &self.send_buf[..])
                .map_err(|_| Error::EncoderStream)?;
            qdebug!([self], "{} bytes sent.", r);
            self.send_buf.read(r as usize);
            Ok(())
        } else {
            Ok(())
        }
    }

    fn is_stream_blocker(&self, stream_id: u64) -> bool {
        if let Some(hb_list) = self.unacked_header_blocks.get(&stream_id) {
            debug_assert!(!hb_list.is_empty());
            match hb_list.iter().flatten().max() {
                Some(max_ref) => *max_ref >= self.table.get_acked_inserts_cnt(),
                None => false,
            }
        } else {
            false
        }
    }

    /// Encodes headers
    /// ### Errors
    /// This function may return a transport error when a new entry is added to the table and while sending its
    /// instruction an error occurs.
    pub fn encode_header_block(
        &mut self,
        conn: &mut Connection,
        h: &[Header],
        stream_id: u64,
    ) -> Res<HeaderEncoder> {
        qdebug!([self], "encoding headers.");
        let mut encoded_h =
            HeaderEncoder::new(self.table.base(), self.use_huffman, self.max_entries);

        let stream_is_blocker = self.is_stream_blocker(stream_id);
        let can_block = self.blocked_stream_cnt < self.max_blocked_streams || stream_is_blocker;

        let mut ref_entries = HashSet::new();

        let mut encoder_blocked = false;

        for iter in h.iter() {
            let name = iter.0.clone().into_bytes();
            let value = iter.1.clone().into_bytes();
            qtrace!("encoding {:x?} {:x?}.", name, value);

            if let Some(LookupResult {
                index,
                static_table,
                value_matches,
            }) = self.table.lookup(&name, &value, can_block)
            {
                qtrace!(
                    [self],
                    "found a {} entry, value-match={}",
                    if static_table { "static" } else { "dynamic" },
                    value_matches
                );
                if value_matches {
                    if static_table {
                        encoded_h.encode_indexed_static(index);
                    } else {
                        encoded_h.encode_indexed_dynamic(index);
                    }
                } else {
                    encoded_h.encode_literal_with_name_ref(static_table, index, &value);
                }
                if !static_table && ref_entries.insert(index) {
                    self.table.add_ref(index);
                }
            } else if can_block & !encoder_blocked {
                // Insert using an InsertWithNameLiteral instruction. This entry name does not match any name in the
                // tables therefore we cannot use any other instruction.
                match self.send_and_insert(conn, &name, &value) {
                    Ok(index) => {
                        encoded_h.encode_indexed_dynamic(index);
                        ref_entries.insert(index);
                        self.table.add_ref(index);
                    }
                    Err(Error::EncoderStreamBlocked) | Err(Error::DynamicTableFull) => {
                        // As soon as one of the instructions cannot be written or the table is full, do not try again.
                        encoder_blocked = true;
                        encoded_h.encode_literal_with_name_literal(&name, &value)
                    }
                    Err(e) => {
                        // These errors should never happen:
                        // `Internal`, `InvalidStreamId`, `InvalidInput`, `FinalSizeError`
                        debug_assert!(false, "Unexpected error: {:?}", e);
                        return Err(e);
                    }
                }
            } else {
                encoded_h.encode_literal_with_name_literal(&name, &value);
            }
        }

        encoded_h.encode_header_block_prefix();

        if !stream_is_blocker {
            // The streams was not a blocker, check if the stream is a blocker now.
            if let Some(max_ref) = ref_entries.iter().max() {
                if *max_ref >= self.table.get_acked_inserts_cnt() {
                    debug_assert!(self.blocked_stream_cnt <= self.max_blocked_streams);
                    self.blocked_stream_cnt += 1;
                }
            }
        }

        if !ref_entries.is_empty() {
            self.unacked_header_blocks
                .entry(stream_id)
                .or_insert_with(VecDeque::new)
                .push_front(ref_entries);
            self.stats.dynamic_table_references += 1;
        }
        Ok(encoded_h)
    }

    /// Encoder stream has been created. Add the stream id.
    pub fn add_send_stream(&mut self, stream_id: u64) {
        if self.local_stream_id.is_some() {
            panic!("Adding multiple local streams");
        }
        self.local_stream_id = Some(stream_id);
        self.send_buf.encode_varint(QPACK_UNI_STREAM_TYPE_ENCODER);
    }

    /// We have received a remote decoder stream. Remember its stream id.
    /// # Errors
    ///    If we receive multiple decoder streams this function will return `WrongStreamCount`.
    pub fn add_recv_stream(&mut self, stream_id: u64) -> Res<()> {
        if self.remote_stream_id.is_some() {
            Err(Error::WrongStreamCount)
        } else {
            self.remote_stream_id = Some(stream_id);
            Ok(())
        }
    }

    #[must_use]
    pub fn stats(&self) -> &Stats {
        &self.stats
    }

    #[must_use]
    pub fn local_stream_id(&self) -> Option<u64> {
        self.local_stream_id
    }

    #[must_use]
    pub fn remote_stream_id(&self) -> Option<u64> {
        self.remote_stream_id
    }

    #[cfg(test)]
    fn blocked_stream_cnt(&self) -> u16 {
        self.blocked_stream_cnt
    }
}

impl ::std::fmt::Display for QPackEncoder {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "QPackEncoder")
    }
}

fn map_error(err: &Error) -> Error {
    if *err == Error::ClosedCriticalStream {
        Error::ClosedCriticalStream
    } else {
        Error::DecoderStream
    }
}

#[cfg(test)]
mod tests {
    use super::{Connection, Error, Header, QPackEncoder};
    use crate::QpackSettings;
    use neqo_transport::tparams::{self, TransportParameter};
    use neqo_transport::StreamType;
    use test_fixture::{default_client, default_server, handshake, now};

    struct TestEncoder {
        encoder: QPackEncoder,
        send_stream_id: u64,
        recv_stream_id: u64,
        conn: Connection,
        peer_conn: Connection,
    }

    fn connect_generic<F>(huffman: bool, f: F) -> TestEncoder
    where
        F: FnOnce(&mut Connection, &mut Connection),
    {
        let mut conn = default_client();
        let mut peer_conn = default_server();
        f(&mut conn, &mut peer_conn);

        // create a stream
        let recv_stream_id = peer_conn.stream_create(StreamType::UniDi).unwrap();
        let send_stream_id = conn.stream_create(StreamType::UniDi).unwrap();

        // create an encoder
        let mut encoder = QPackEncoder::new(
            QpackSettings {
                max_table_size_encoder: 1500,
                max_table_size_decoder: 0,
                max_blocked_streams: 0,
            },
            huffman,
        );
        encoder.add_send_stream(send_stream_id);

        TestEncoder {
            encoder,
            send_stream_id,
            recv_stream_id,
            conn,
            peer_conn,
        }
    }

    fn connect(huffman: bool) -> TestEncoder {
        connect_generic(huffman, |client, server| {
            handshake(client, server);
        })
    }

    fn connect_flow_control(max_data: u64) -> TestEncoder {
        connect_generic(true, |client, server| {
            server
                .set_local_tparam(
                    tparams::INITIAL_MAX_DATA,
                    TransportParameter::Integer(max_data),
                )
                .unwrap();

            handshake(client, server);
        })
    }

    fn send_instructions(encoder: &mut TestEncoder, encoder_instruction: &[u8]) {
        encoder.encoder.send(&mut encoder.conn).unwrap();
        let out = encoder.conn.process(None, now());
        let out2 = encoder.peer_conn.process(out.dgram(), now());
        let _ = encoder.conn.process(out2.dgram(), now());
        let mut buf = [0_u8; 100];
        let (amount, fin) = encoder
            .peer_conn
            .stream_recv(encoder.send_stream_id, &mut buf)
            .unwrap();
        assert_eq!(fin, false);
        assert_eq!(buf[..amount], encoder_instruction[..]);
    }

    fn recv_instruction(encoder: &mut TestEncoder, decoder_instruction: &[u8]) {
        encoder
            .peer_conn
            .stream_send(encoder.recv_stream_id, decoder_instruction)
            .unwrap();
        let out = encoder.peer_conn.process(None, now());
        let _ = encoder.conn.process(out.dgram(), now());
        assert!(encoder
            .encoder
            .read_instructions(&mut encoder.conn, encoder.recv_stream_id)
            .is_ok());
    }

    const CAP_INSTRUCTION_200: &[u8] = &[0x02, 0x3f, 0xa9, 0x01];
    const CAP_INSTRUCTION_60: &[u8] = &[0x02, 0x3f, 0x1d];
    const CAP_INSTRUCTION_1000: &[u8] = &[0x02, 0x3f, 0xc9, 0x07];
    const CAP_INSTRUCTION_1500: &[u8] = &[0x02, 0x3f, 0xbd, 0x0b];

    const HEADER_CONTENT_LENGTH: &[u8] = &[
        0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x6c, 0x65, 0x6e, 0x67, 0x74, 0x68,
    ];
    const VALUE_1: &[u8] = &[0x31, 0x32, 0x33, 0x34];
    const VALUE_2: &[u8] = &[0x31, 0x32, 0x33, 0x34, 0x35];

    // HEADER_CONTENT_LENGTH and VALUE_1 encoded by instruction insert_with_name_literal.
    const HEADER_CONTENT_LENGTH_VALUE_1_NAME_LITERAL: &[u8] = &[
        0x4e, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x6c, 0x65, 0x6e, 0x67, 0x74, 0x68,
        0x04, 0x31, 0x32, 0x33, 0x34,
    ];

    // HEADER_CONTENT_LENGTH and VALUE_2 encoded by instruction insert_with_name_literal.
    const HEADER_CONTENT_LENGTH_VALUE_2_NAME_LITERAL: &[u8] = &[
        0x4e, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x6c, 0x65, 0x6e, 0x67, 0x74, 0x68,
        0x05, 0x31, 0x32, 0x33, 0x34, 0x35,
    ];

    // Indexed Header Field that refers to the first entry in the dynamic table.
    const ENCODE_INDEXED_REF_DYNAMIC: &[u8] = &[0x02, 0x00, 0x80];

    const HEADER_ACK_STREAM_ID_1: &[u8] = &[0x81];
    const HEADER_ACK_STREAM_ID_2: &[u8] = &[0x82];
    const STREAM_CANCELED_ID_1: &[u8] = &[0x41];

    // test insert_with_name_literal which fails because there is not enough space in the table
    #[test]
    fn test_insert_with_name_literal_1() {
        let mut encoder = connect(false);

        // insert "content-length: 1234
        let res =
            encoder
                .encoder
                .send_and_insert(&mut encoder.conn, HEADER_CONTENT_LENGTH, VALUE_1);
        assert_eq!(Error::DynamicTableFull, res.unwrap_err());
        send_instructions(&mut encoder, &[0x02]);
    }

    // test insert_with_name_literal - succeeds
    #[test]
    fn test_insert_with_name_literal_2() {
        let mut encoder = connect(false);

        assert!(encoder.encoder.set_max_capacity(200).is_ok());
        // test the change capacity instruction.
        send_instructions(&mut encoder, CAP_INSTRUCTION_200);

        // insert "content-length: 1234
        let res =
            encoder
                .encoder
                .send_and_insert(&mut encoder.conn, HEADER_CONTENT_LENGTH, VALUE_1);
        assert!(res.is_ok());
        send_instructions(&mut encoder, HEADER_CONTENT_LENGTH_VALUE_1_NAME_LITERAL);
    }

    #[test]
    fn test_change_capacity() {
        let mut encoder = connect(false);

        assert!(encoder.encoder.set_max_capacity(200).is_ok());
        send_instructions(&mut encoder, CAP_INSTRUCTION_200);
    }

    struct TestElement {
        pub headers: Vec<Header>,
        pub header_block: &'static [u8],
        pub encoder_inst: &'static [u8],
    }

    #[test]
    fn test_header_block_encoder_non() {
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
                header_block: ENCODE_INDEXED_REF_DYNAMIC,
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

        let mut encoder = connect(false);

        encoder.encoder.set_max_blocked_streams(100).unwrap();
        encoder.encoder.set_max_capacity(200).unwrap();

        // test the change capacity instruction.
        send_instructions(&mut encoder, CAP_INSTRUCTION_200);

        for t in &test_cases {
            let buf = encoder
                .encoder
                .encode_header_block(&mut encoder.conn, &t.headers, 1)
                .unwrap();
            assert_eq!(&buf[..], t.header_block);
            send_instructions(&mut encoder, t.encoder_inst);
        }
    }

    #[test]
    fn test_header_block_encoder_huffman() {
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
                header_block: ENCODE_INDEXED_REF_DYNAMIC,
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

        let mut encoder = connect(true);

        encoder.encoder.set_max_blocked_streams(100).unwrap();
        encoder.encoder.set_max_capacity(200).unwrap();

        // test the change capacity instruction.
        send_instructions(&mut encoder, CAP_INSTRUCTION_200);

        for t in &test_cases {
            let buf = encoder
                .encoder
                .encode_header_block(&mut encoder.conn, &t.headers, 1)
                .unwrap();
            assert_eq!(&buf[..], t.header_block);
            send_instructions(&mut encoder, t.encoder_inst);
        }
    }

    // Test inserts block on waiting for an insert count increment.
    #[test]
    fn test_insertion_blocked_on_insert_count_feedback() {
        let mut encoder = connect(false);

        encoder.encoder.set_max_capacity(60).unwrap();

        // test the change capacity instruction.
        send_instructions(&mut encoder, CAP_INSTRUCTION_60);

        // insert "content-length: 1234
        let res =
            encoder
                .encoder
                .send_and_insert(&mut encoder.conn, HEADER_CONTENT_LENGTH, VALUE_1);
        assert!(res.is_ok());
        send_instructions(&mut encoder, HEADER_CONTENT_LENGTH_VALUE_1_NAME_LITERAL);

        // insert "content-length: 12345 which will fail because the ntry in the table cannot be evicted.
        let res =
            encoder
                .encoder
                .send_and_insert(&mut encoder.conn, HEADER_CONTENT_LENGTH, VALUE_2);
        assert!(res.is_err());
        send_instructions(&mut encoder, &[]);

        // receive an insert count increment.
        recv_instruction(&mut encoder, &[0x01]);

        // insert "content-length: 12345 again it will succeed.
        let res =
            encoder
                .encoder
                .send_and_insert(&mut encoder.conn, HEADER_CONTENT_LENGTH, VALUE_2);
        assert!(res.is_ok());
        send_instructions(&mut encoder, HEADER_CONTENT_LENGTH_VALUE_2_NAME_LITERAL);
    }

    // Test inserts block on waiting for acks
    // test the table insertion is blocked:
    // 0 - waiting for a header ack
    // 2 - waiting for a stream cancel.
    fn test_insertion_blocked_on_waiting_for_header_ack_or_stream_cancel(wait: u8) {
        let mut encoder = connect(false);

        assert!(encoder.encoder.set_max_capacity(60).is_ok());
        // test the change capacity instruction.
        send_instructions(&mut encoder, CAP_INSTRUCTION_60);

        // insert "content-length: 1234
        let res =
            encoder
                .encoder
                .send_and_insert(&mut encoder.conn, HEADER_CONTENT_LENGTH, VALUE_1);
        assert!(res.is_ok());
        send_instructions(&mut encoder, HEADER_CONTENT_LENGTH_VALUE_1_NAME_LITERAL);

        // receive an insert count increment.
        recv_instruction(&mut encoder, &[0x01]);

        // send a header block
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("content-length"), String::from("1234"))],
                1,
            )
            .unwrap();
        assert_eq!(&buf[..], ENCODE_INDEXED_REF_DYNAMIC);
        send_instructions(&mut encoder, &[]);

        // insert "content-length: 12345 which will fail because the entry in the table cannot be evicted
        let res =
            encoder
                .encoder
                .send_and_insert(&mut encoder.conn, HEADER_CONTENT_LENGTH, VALUE_2);
        assert!(res.is_err());
        send_instructions(&mut encoder, &[]);

        if wait == 0 {
            // receive a header_ack.
            recv_instruction(&mut encoder, HEADER_ACK_STREAM_ID_1);
        } else {
            // receive a stream canceled
            recv_instruction(&mut encoder, STREAM_CANCELED_ID_1);
        }

        // insert "content-length: 12345 again it will succeed.
        let res =
            encoder
                .encoder
                .send_and_insert(&mut encoder.conn, HEADER_CONTENT_LENGTH, VALUE_2);
        assert!(res.is_ok());
        send_instructions(&mut encoder, HEADER_CONTENT_LENGTH_VALUE_2_NAME_LITERAL);
    }

    #[test]
    fn test_header_ack() {
        test_insertion_blocked_on_waiting_for_header_ack_or_stream_cancel(0);
    }

    #[test]
    fn test_stream_canceled() {
        test_insertion_blocked_on_waiting_for_header_ack_or_stream_cancel(1);
    }

    fn assert_is_index_to_dynamic(buf: &[u8]) {
        assert_eq!(buf[2] & 0xc0, 0x80);
    }

    fn assert_is_index_to_dynamic_post(buf: &[u8]) {
        assert_eq!(buf[2] & 0xf0, 0x10);
    }

    fn assert_is_index_to_static_name_only(buf: &[u8]) {
        assert_eq!(buf[2] & 0xf0, 0x50);
    }

    fn assert_is_literal_value_literal_name(buf: &[u8]) {
        assert_eq!(buf[2] & 0xf0, 0x20);
    }

    #[test]
    fn max_block_streams1() {
        let mut encoder = connect(false);

        assert!(encoder.encoder.set_max_capacity(60).is_ok());

        // change capacity to 60.
        send_instructions(&mut encoder, CAP_INSTRUCTION_60);

        // insert "content-length: 1234
        let res =
            encoder
                .encoder
                .send_and_insert(&mut encoder.conn, HEADER_CONTENT_LENGTH, VALUE_1);

        assert!(res.is_ok());
        send_instructions(&mut encoder, HEADER_CONTENT_LENGTH_VALUE_1_NAME_LITERAL);

        encoder.encoder.set_max_blocked_streams(1).unwrap();

        // send a header block, it refers to unacked entry.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("content-length"), String::from("1234"))],
                1,
            )
            .unwrap();
        assert_is_index_to_dynamic(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);

        send_instructions(&mut encoder, &[]);

        // The next one will not use the dynamic entry because it is exceeding the max_blocked_streams
        // limit.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("content-length"), String::from("1234"))],
                2,
            )
            .unwrap();
        assert_is_index_to_static_name_only(&buf);

        send_instructions(&mut encoder, &[]);
        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);

        // another header block to already blocked stream can still use the entry.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("content-length"), String::from("1234"))],
                1,
            )
            .unwrap();
        assert_is_index_to_dynamic(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);
    }

    #[test]
    fn max_block_streams2() {
        let mut encoder = connect(false);

        assert!(encoder.encoder.set_max_capacity(200).is_ok());

        // change capacity to 200.
        send_instructions(&mut encoder, CAP_INSTRUCTION_200);

        // insert "content-length: 1234
        let res =
            encoder
                .encoder
                .send_and_insert(&mut encoder.conn, HEADER_CONTENT_LENGTH, VALUE_1);

        assert!(res.is_ok());
        send_instructions(&mut encoder, HEADER_CONTENT_LENGTH_VALUE_1_NAME_LITERAL);

        // insert "content-length: 12345
        let res =
            encoder
                .encoder
                .send_and_insert(&mut encoder.conn, HEADER_CONTENT_LENGTH, VALUE_2);

        assert!(res.is_ok());
        send_instructions(&mut encoder, HEADER_CONTENT_LENGTH_VALUE_2_NAME_LITERAL);

        encoder.encoder.set_max_blocked_streams(1).unwrap();

        let stream_id = 1;
        // send a header block, it refers to unacked entry.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("content-length"), String::from("1234"))],
                stream_id,
            )
            .unwrap();
        assert_is_index_to_dynamic(&buf);

        // encode another header block for the same stream that will refer to the second entry
        // in the dynamic table.
        // This should work because the stream is already a blocked stream
        // send a header block, it refers to unacked entry.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("content-length"), String::from("12345"))],
                stream_id,
            )
            .unwrap();
        assert_is_index_to_dynamic(&buf);
    }

    #[test]
    fn max_block_streams3() {
        let mut encoder = connect(false);

        assert!(encoder.encoder.set_max_capacity(200).is_ok());

        // change capacity to 200.
        send_instructions(&mut encoder, CAP_INSTRUCTION_200);

        encoder.encoder.set_max_blocked_streams(1).unwrap();

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 0);

        // send a header block, that creates an new entry and refers to it.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("name1"), String::from("value1"))],
                1,
            )
            .unwrap();
        assert_is_index_to_dynamic_post(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);

        // The next one will not create a new entry because the encoder is on max_blocked_streams limit.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("name2"), String::from("value2"))],
                2,
            )
            .unwrap();
        assert_is_literal_value_literal_name(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);

        // another header block to already blocked stream can still create a new entry.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("name2"), String::from("value2"))],
                1,
            )
            .unwrap();
        assert_is_index_to_dynamic_post(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);
    }

    #[test]
    fn max_block_streams4() {
        let mut encoder = connect(false);

        assert!(encoder.encoder.set_max_capacity(200).is_ok());

        // change capacity to 200.
        send_instructions(&mut encoder, CAP_INSTRUCTION_200);

        encoder.encoder.set_max_blocked_streams(1).unwrap();

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 0);

        // send a header block, that creates an new entry and refers to it.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("name1"), String::from("value1"))],
                1,
            )
            .unwrap();
        assert_is_index_to_dynamic_post(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);

        // another header block to already blocked stream can still create a new entry.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("name2"), String::from("value2"))],
                1,
            )
            .unwrap();
        assert_is_index_to_dynamic_post(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);

        // receive a header_ack for the first header block.
        recv_instruction(&mut encoder, HEADER_ACK_STREAM_ID_1);

        // The stream is still blocking because the second header block is not acked.
        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);
    }

    #[test]
    fn max_block_streams5() {
        let mut encoder = connect(false);

        assert!(encoder.encoder.set_max_capacity(200).is_ok());

        // change capacity to 200.
        send_instructions(&mut encoder, CAP_INSTRUCTION_200);

        encoder.encoder.set_max_blocked_streams(1).unwrap();

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 0);

        // send a header block, that creates an new entry and refers to it.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("name1"), String::from("value1"))],
                1,
            )
            .unwrap();
        assert_is_index_to_dynamic_post(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);

        // another header block to already blocked stream can still create a new entry.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("name1"), String::from("value1"))],
                1,
            )
            .unwrap();
        assert_is_index_to_dynamic(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);

        // receive a header_ack for the first header block.
        recv_instruction(&mut encoder, HEADER_ACK_STREAM_ID_1);

        // The stream is not blocking anymore because header ack also acks the instruction.
        assert_eq!(encoder.encoder.blocked_stream_cnt(), 0);
    }

    #[test]
    fn max_block_streams6() {
        let mut encoder = connect(false);

        assert!(encoder.encoder.set_max_capacity(200).is_ok());

        // change capacity to 200.
        send_instructions(&mut encoder, CAP_INSTRUCTION_200);

        encoder.encoder.set_max_blocked_streams(2).unwrap();

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 0);

        // send a header block, that creates an new entry and refers to it.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("name1"), String::from("value1"))],
                1,
            )
            .unwrap();
        assert_is_index_to_dynamic_post(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);

        // header block for the next stream will create an new entry as well.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("name2"), String::from("value2"))],
                2,
            )
            .unwrap();
        assert_is_index_to_dynamic_post(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 2);

        // receive a header_ack for the second header block. This will ack the first as well
        recv_instruction(&mut encoder, HEADER_ACK_STREAM_ID_2);

        // The stream is not blocking anymore because header ack also acks the instruction.
        assert_eq!(encoder.encoder.blocked_stream_cnt(), 0);
    }

    #[test]
    fn max_block_streams7() {
        let mut encoder = connect(false);

        assert!(encoder.encoder.set_max_capacity(200).is_ok());

        // change capacity to 200.
        send_instructions(&mut encoder, CAP_INSTRUCTION_200);

        encoder.encoder.set_max_blocked_streams(2).unwrap();

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 0);

        // send a header block, that creates an new entry and refers to it.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("name1"), String::from("value1"))],
                1,
            )
            .unwrap();
        assert_is_index_to_dynamic_post(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);

        // header block for the next stream will create an new entry as well.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("name1"), String::from("value1"))],
                2,
            )
            .unwrap();
        assert_is_index_to_dynamic(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 2);

        // receive a stream cancel for the first stream.
        // This will remove the first stream as blocking but it will not mark the instruction as acked.
        // and the second steam will still be blocking.
        recv_instruction(&mut encoder, STREAM_CANCELED_ID_1);

        // The stream is not blocking anymore because header ack also acks the instruction.
        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);
    }

    #[test]
    fn max_block_stream8() {
        let mut encoder = connect(false);

        assert!(encoder.encoder.set_max_capacity(200).is_ok());

        // change capacity to 200.
        send_instructions(&mut encoder, CAP_INSTRUCTION_200);

        encoder.encoder.set_max_blocked_streams(2).unwrap();

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 0);

        // send a header block, that creates an new entry and refers to it.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("name1"), String::from("value1"))],
                1,
            )
            .unwrap();
        assert_is_index_to_dynamic_post(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);

        // header block for the next stream will refer to the same entry.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("name1"), String::from("value1"))],
                2,
            )
            .unwrap();
        assert_is_index_to_dynamic(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 2);

        // send another header block on stream 1.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("name2"), String::from("value2"))],
                1,
            )
            .unwrap();
        assert_is_index_to_dynamic_post(&buf);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 2);

        // stream 1 is block on entries 1 and 2; stream 2 is block only on 1.
        // receive an Insert Count Increment for the first entry.
        // After that only stream 1 will be blocking.
        recv_instruction(&mut encoder, &[0x01]);

        assert_eq!(encoder.encoder.blocked_stream_cnt(), 1);
    }

    #[test]
    fn dynamic_table_can_evict1() {
        let mut encoder = connect(false);

        assert!(encoder.encoder.set_max_capacity(60).is_ok());

        // change capacity to 60.
        send_instructions(&mut encoder, CAP_INSTRUCTION_60);

        encoder.encoder.set_max_blocked_streams(2).unwrap();

        // insert "content-length: 1234
        let res =
            encoder
                .encoder
                .send_and_insert(&mut encoder.conn, HEADER_CONTENT_LENGTH, VALUE_1);

        assert!(res.is_ok());
        send_instructions(&mut encoder, HEADER_CONTENT_LENGTH_VALUE_1_NAME_LITERAL);

        // send a header block, it refers to unacked entry.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("content-length"), String::from("1234"))],
                1,
            )
            .unwrap();
        assert_is_index_to_dynamic(&buf);

        // trying to evict the entry will failed.
        assert!(encoder.encoder.set_max_capacity(10).is_err());

        // receive an Insert Count Increment for the entry.
        recv_instruction(&mut encoder, &[0x01]);

        // trying to evict the entry will failed. The stream is still referring to it.
        assert!(encoder.encoder.set_max_capacity(10).is_err());

        // receive a header_ack for the header block.
        recv_instruction(&mut encoder, HEADER_ACK_STREAM_ID_1);

        // now entry can be evicted.
        assert!(encoder.encoder.set_max_capacity(10).is_ok());
    }

    #[test]
    fn dynamic_table_can_evict2() {
        let mut encoder = connect(false);

        assert!(encoder.encoder.set_max_capacity(60).is_ok());

        // change capacity to 60.
        send_instructions(&mut encoder, CAP_INSTRUCTION_60);

        encoder.encoder.set_max_blocked_streams(2).unwrap();

        // insert "content-length: 1234
        let res =
            encoder
                .encoder
                .send_and_insert(&mut encoder.conn, HEADER_CONTENT_LENGTH, VALUE_1);

        assert!(res.is_ok());
        send_instructions(&mut encoder, HEADER_CONTENT_LENGTH_VALUE_1_NAME_LITERAL);

        // send a header block, it refers to unacked entry.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("content-length"), String::from("1234"))],
                1,
            )
            .unwrap();
        assert_is_index_to_dynamic(&buf);

        // trying to evict the entry will failed.
        assert!(encoder.encoder.set_max_capacity(10).is_err());

        // receive an Insert Count Increment for the entry.
        recv_instruction(&mut encoder, &[0x01]);

        // trying to evict the entry will failed. The stream is still referring to it.
        assert!(encoder.encoder.set_max_capacity(10).is_err());

        // receive a stream cancelled.
        recv_instruction(&mut encoder, STREAM_CANCELED_ID_1);

        // now entry can be evicted.
        assert!(encoder.encoder.set_max_capacity(10).is_ok());
    }

    #[test]
    fn dynamic_table_can_evict3() {
        let mut encoder = connect(false);

        assert!(encoder.encoder.set_max_capacity(60).is_ok());

        // change capacity to 60.
        send_instructions(&mut encoder, CAP_INSTRUCTION_60);

        encoder.encoder.set_max_blocked_streams(2).unwrap();

        // insert "content-length: 1234
        let res =
            encoder
                .encoder
                .send_and_insert(&mut encoder.conn, HEADER_CONTENT_LENGTH, VALUE_1);

        assert!(res.is_ok());
        send_instructions(&mut encoder, HEADER_CONTENT_LENGTH_VALUE_1_NAME_LITERAL);

        // trying to evict the entry will failed, because the entry is not acked.
        assert!(encoder.encoder.set_max_capacity(10).is_err());

        // receive an Insert Count Increment for the entry.
        recv_instruction(&mut encoder, &[0x01]);

        // now entry can be evicted.
        assert!(encoder.encoder.set_max_capacity(10).is_ok());
    }

    #[test]
    fn dynamic_table_can_evict4() {
        let mut encoder = connect(false);

        assert!(encoder.encoder.set_max_capacity(60).is_ok());

        // change capacity to 60.
        send_instructions(&mut encoder, CAP_INSTRUCTION_60);

        encoder.encoder.set_max_blocked_streams(2).unwrap();

        // insert "content-length: 1234
        let res =
            encoder
                .encoder
                .send_and_insert(&mut encoder.conn, HEADER_CONTENT_LENGTH, VALUE_1);

        assert!(res.is_ok());
        send_instructions(&mut encoder, HEADER_CONTENT_LENGTH_VALUE_1_NAME_LITERAL);

        // send a header block, it refers to unacked entry.
        let buf = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[(String::from("content-length"), String::from("1234"))],
                1,
            )
            .unwrap();
        assert_is_index_to_dynamic(&buf);

        // trying to evict the entry will failed. The stream is still referring to it and
        // entry is not acked.
        assert!(encoder.encoder.set_max_capacity(10).is_err());

        // receive a header_ack for the header block. This will also ack the instruction.
        recv_instruction(&mut encoder, HEADER_ACK_STREAM_ID_1);

        // now entry can be evicted.
        assert!(encoder.encoder.set_max_capacity(10).is_ok());
    }

    #[test]
    fn encoder_flow_controlled_blocked() {
        const SMALL_MAX_DATA: u64 = 900;
        const STREAM_DATA_LEN: usize = 900 - 20;
        const STREAM_DATA: &[u8] = &[0; STREAM_DATA_LEN];
        const ONE_INSTRUCTION: &[u8] = &[
            0x67, 0x41, 0xe9, 0x2a, 0x67, 0x35, 0x53, 0x7f, 0x83, 0x8, 0x99, 0x6b,
        ];
        const TWO_INSTRUCTION: &[u8] = &[
            0x67, 0x41, 0xe9, 0x2a, 0x67, 0x35, 0x53, 0x37, 0x83, 0x8, 0x99, 0x6b, 0x67, 0x41,
            0xe9, 0x2a, 0x67, 0x35, 0x53, 0x39, 0x88, 0x8, 0x99, 0x69, 0xb7, 0x1d, 0x79, 0xf0,
            0x83,
        ];
        let mut encoder = connect_flow_control(SMALL_MAX_DATA);

        // change capacity to 1000 and max_block streams to 20.
        encoder.encoder.set_max_blocked_streams(20).unwrap();
        assert!(encoder.encoder.set_max_capacity(1000).is_ok());
        send_instructions(&mut encoder, CAP_INSTRUCTION_1000);

        // Write some data to fill the flow control allowance.
        let stream_id = encoder.conn.stream_create(StreamType::UniDi).unwrap();
        assert_eq!(
            encoder.conn.stream_send(stream_id, STREAM_DATA).unwrap(),
            STREAM_DATA_LEN
        );

        // Encode a header block with 2 headers. The first header will be added to the dynamic table.
        // The second will not be added to the dynamic table, because the corresponding instruction
        // cannot be written immediately due to the flow control limit.
        let buf1 = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[
                    (String::from("something"), String::from("1234")),
                    (String::from("something2"), String::from("12345678910")),
                ],
                1,
            )
            .unwrap();

        // Assert that the first header is encoded as an index to the dynamic table (a post form).
        assert_eq!(buf1[2], 0x10);
        // Assert that the second header is encoded as a literal with a name literal
        assert_eq!(buf1[3] & 0xf0, 0x20);

        // Try to encode another header block. Here both headers will be encoded as a literal with a name literal
        let buf2 = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[
                    (String::from("something3"), String::from("1234")),
                    (String::from("something4"), String::from("12345678910")),
                ],
                2,
            )
            .unwrap();
        assert_eq!(buf2[2] & 0xf0, 0x20);

        // Ensure that we have sent only one instruction for (String::from("something"), String::from("1234"))
        send_instructions(&mut encoder, ONE_INSTRUCTION);

        // Try writing a new header block. Now, headers will be added to the dynamic table again, because
        // instructions can be sent.
        let buf3 = encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[
                    (String::from("something5"), String::from("1234")),
                    (String::from("something6"), String::from("12345678910")),
                ],
                3,
            )
            .unwrap();
        // Assert that both headers are encoded as an index to the dynamic table (a post form).
        assert_eq!(buf3[2], 0x10);
        assert_eq!(buf3[3], 0x11);

        // Asset that 2 instruction has been sent
        send_instructions(&mut encoder, TWO_INSTRUCTION);
    }

    #[test]
    fn encoder_max_capacity_limit() {
        let mut encoder = connect(false);

        // change capacity to 2000.
        assert!(encoder.encoder.set_max_capacity(2000).is_ok());
        send_instructions(&mut encoder, CAP_INSTRUCTION_1500);
    }

    #[test]
    fn test_do_not_evict_entry_that_are_referd_only_by_the_same_header_blocked_encoding() {
        let mut encoder = connect(false);

        encoder.encoder.set_max_blocked_streams(20).unwrap();
        assert!(encoder.encoder.set_max_capacity(50).is_ok());

        encoder
            .encoder
            .send_and_insert(&mut encoder.conn, b"something5", b"1234")
            .unwrap();

        encoder.encoder.send(&mut encoder.conn).unwrap();
        let out = encoder.conn.process(None, now());
        let _ = encoder.peer_conn.process(out.dgram(), now());
        // receive an insert count increment.
        recv_instruction(&mut encoder, &[0x01]);

        assert!(encoder
            .encoder
            .encode_header_block(
                &mut encoder.conn,
                &[
                    (String::from("something5"), String::from("1234")),
                    (String::from("something6"), String::from("1234")),
                ],
                3,
            )
            .is_ok());
    }
}
