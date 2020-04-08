// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::prefix::{
    BASE_PREFIX_NEGATIVE, BASE_PREFIX_POSITIVE, HEADER_FIELD_INDEX_DYNAMIC,
    HEADER_FIELD_INDEX_DYNAMIC_POST, HEADER_FIELD_INDEX_STATIC, HEADER_FIELD_LITERAL_NAME_LITERAL,
    HEADER_FIELD_LITERAL_NAME_REF_DYNAMIC, HEADER_FIELD_LITERAL_NAME_REF_DYNAMIC_POST,
    HEADER_FIELD_LITERAL_NAME_REF_STATIC, NO_PREFIX,
};
use crate::qpack_send_buf::QPData;
use crate::reader::{to_string, ReceiverBufferWrapper};
use crate::table::HeaderTable;
use crate::Header;
use crate::{Error, Res};
use neqo_common::qtrace;
use std::ops::{Deref, Div};

#[derive(Default, Debug, PartialEq)]
pub struct HeaderEncoder {
    buf: QPData,
    base: u64,
    use_huffman: bool,
    max_entries: u64,
    max_dynamic_index_ref: Option<u64>,
}

impl ::std::fmt::Display for HeaderEncoder {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "HeaderEncoder")
    }
}

impl HeaderEncoder {
    pub fn new(base: u64, use_huffman: bool, max_entries: u64) -> Self {
        let mut encoder = Self {
            buf: QPData::default(),
            base,
            use_huffman,
            max_entries,
            max_dynamic_index_ref: None,
        };
        encoder.encode_header_block_prefix(false, 0, base, true);
        encoder
    }

    pub fn len(&self) -> usize {
        self.buf.len()
    }

    pub fn read(&mut self, r: usize) {
        self.buf.read(r);
    }

    fn encode_header_block_prefix(
        &mut self,
        fix: bool,
        req_insert_cnt: u64,
        delta: u64,
        positive: bool,
    ) {
        qtrace!(
            [self],
            "encode header block prefix req_insert_cnt={} delta={} (fix={}).",
            req_insert_cnt,
            delta,
            fix
        );
        let enc_insert_cnt = if req_insert_cnt == 0 {
            0
        } else {
            (req_insert_cnt % (2 * self.max_entries)) + 1
        };

        let prefix = if positive {
            BASE_PREFIX_POSITIVE
        } else {
            BASE_PREFIX_NEGATIVE
        };

        if fix {
            // TODO fix for case when there is no enough space!!!
            let offset =
                self.buf
                    .encode_prefixed_encoded_int_with_offset(0, NO_PREFIX, enc_insert_cnt);
            let _ = self
                .buf
                .encode_prefixed_encoded_int_with_offset(offset, prefix, delta);
        } else {
            self.buf
                .encode_prefixed_encoded_int(NO_PREFIX, enc_insert_cnt);
            self.buf.encode_prefixed_encoded_int(prefix, delta);
        }
    }

    pub fn encode_indexed_static(&mut self, index: u64) {
        qtrace!([self], "encode static index {}.", index);
        self.buf
            .encode_prefixed_encoded_int(HEADER_FIELD_INDEX_STATIC, index);
    }

    fn new_ref(&mut self, index: u64) {
        if let Some(r) = self.max_dynamic_index_ref {
            if r < index {
                self.max_dynamic_index_ref = Some(index);
            }
        } else {
            self.max_dynamic_index_ref = Some(index);
        }
    }

    pub fn encode_indexed_dynamic(&mut self, index: u64) {
        qtrace!([self], "encode dynamic index {}.", index);
        if index < self.base {
            self.buf
                .encode_prefixed_encoded_int(HEADER_FIELD_INDEX_DYNAMIC, self.base - index - 1);
        } else {
            self.buf
                .encode_prefixed_encoded_int(HEADER_FIELD_INDEX_DYNAMIC_POST, index - self.base);
        }
        self.new_ref(index);
    }

    pub fn encode_literal_with_name_ref(&mut self, is_static: bool, index: u64, value: &[u8]) {
        qtrace!(
            [self],
            "encode literal with name ref - index={}, static={}, value={:x?}",
            index,
            is_static,
            value
        );
        if is_static {
            self.buf
                .encode_prefixed_encoded_int(HEADER_FIELD_LITERAL_NAME_REF_STATIC, index);
        } else if index < self.base {
            self.buf.encode_prefixed_encoded_int(
                HEADER_FIELD_LITERAL_NAME_REF_DYNAMIC,
                self.base - index - 1,
            );
            self.new_ref(index);
        } else {
            self.buf.encode_prefixed_encoded_int(
                HEADER_FIELD_LITERAL_NAME_REF_DYNAMIC_POST,
                index - self.base,
            );
            self.new_ref(index);
        }

        self.buf.encode_literal(self.use_huffman, NO_PREFIX, value);
    }

    pub fn encode_literal_with_name_literal(&mut self, name: &[u8], value: &[u8]) {
        qtrace!(
            [self],
            "encode literal with name literal - name={:x?}, value={:x?}.",
            name,
            value
        );
        self.buf
            .encode_literal(self.use_huffman, HEADER_FIELD_LITERAL_NAME_LITERAL, name);
        self.buf.encode_literal(self.use_huffman, NO_PREFIX, value);
    }

    pub fn fix_header_block_prefix(&mut self) {
        if let Some(r) = self.max_dynamic_index_ref {
            let req_insert_cnt = r + 1;
            if req_insert_cnt <= self.base {
                self.encode_header_block_prefix(
                    true,
                    req_insert_cnt,
                    self.base - req_insert_cnt,
                    true,
                );
            } else {
                self.encode_header_block_prefix(
                    true,
                    req_insert_cnt,
                    req_insert_cnt - self.base - 1,
                    false,
                );
            }
        }
    }
}

impl Deref for HeaderEncoder {
    type Target = [u8];
    fn deref(&self) -> &Self::Target {
        self.buf.deref()
    }
}

pub struct HeaderDecoder<'a> {
    buf: ReceiverBufferWrapper<'a>,
    base: u64,
    req_insert_cnt: u64,
}

impl<'a> ::std::fmt::Display for HeaderDecoder<'a> {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "HeaderDecoder")
    }
}

#[derive(Debug, PartialEq)]
pub enum HeaderDecoderResult {
    Blocked(u64),
    Headers(Vec<Header>),
}

impl<'a> HeaderDecoder<'a> {
    pub fn new(buf: &'a [u8]) -> Self {
        Self {
            buf: ReceiverBufferWrapper::new(buf),
            base: 0,
            req_insert_cnt: 0,
        }
    }

    pub fn decode_header_block(
        &mut self,
        table: &HeaderTable,
        max_entries: u64,
        total_num_of_inserts: u64,
    ) -> Res<HeaderDecoderResult> {
        self.read_base(max_entries, total_num_of_inserts)?;

        if table.base() < self.req_insert_cnt {
            qtrace!(
                [self],
                "decoding is blocked, requested inserts count={}",
                self.req_insert_cnt
            );
            return Ok(HeaderDecoderResult::Blocked(self.req_insert_cnt));
        }
        let mut h: Vec<Header> = Vec::new();

        loop {
            if self.buf.done() {
                qtrace!([self], "done decoding header block.");
                break Ok(HeaderDecoderResult::Headers(h));
            }

            let b = self.buf.peek()?;
            if HEADER_FIELD_INDEX_STATIC.cmp_prefix(b) {
                h.push(self.read_indexed_static()?);
            } else if HEADER_FIELD_INDEX_DYNAMIC.cmp_prefix(b) {
                h.push(self.read_indexed_dynamic(table)?);
            } else if HEADER_FIELD_INDEX_DYNAMIC_POST.cmp_prefix(b) {
                h.push(self.read_indexed_dynamic_post(table)?);
            } else if HEADER_FIELD_LITERAL_NAME_REF_STATIC.cmp_prefix(b) {
                h.push(self.read_literal_with_name_ref_static()?);
            } else if HEADER_FIELD_LITERAL_NAME_REF_DYNAMIC.cmp_prefix(b) {
                h.push(self.read_literal_with_name_ref_dynamic(table)?);
            } else if HEADER_FIELD_LITERAL_NAME_LITERAL.cmp_prefix(b) {
                h.push(self.read_literal_with_name_literal()?);
            } else if HEADER_FIELD_LITERAL_NAME_REF_DYNAMIC_POST.cmp_prefix(b) {
                h.push(self.read_literal_with_name_ref_dynamic_post(table)?);
            } else {
                unreachable!("All prefixes are covered");
            }
        }
    }

    pub fn get_req_insert_cnt(&self) -> u64 {
        self.req_insert_cnt
    }

    fn read_base(&mut self, max_entries: u64, total_num_of_inserts: u64) -> Res<()> {
        let insert_cnt = self.buf.read_prefixed_int(0)?;
        self.req_insert_cnt =
            HeaderDecoder::calc_req_insert_cnt(insert_cnt, max_entries, total_num_of_inserts)?;

        let s = self.buf.peek()? & 0x80 != 0;
        let base_delta = self.buf.read_prefixed_int(1)?;
        self.base = if s {
            if self.req_insert_cnt <= base_delta {
                return Err(Error::DecompressionFailed);
            }
            self.req_insert_cnt - base_delta - 1
        } else {
            self.req_insert_cnt + base_delta
        };
        qtrace!(
            [self],
            "requested inserts count is {} and base is {}",
            self.req_insert_cnt,
            self.base
        );
        Ok(())
    }

    fn calc_req_insert_cnt(encoded: u64, max_entries: u64, total_num_of_inserts: u64) -> Res<u64> {
        if encoded == 0 {
            Ok(0)
        } else if max_entries == 0 {
            Err(Error::DecompressionFailed)
        } else {
            let full_range = 2 * max_entries;
            if encoded > full_range {
                return Err(Error::DecompressionFailed);
            }
            let max_value = total_num_of_inserts + max_entries;
            let max_wrapped = max_value.div(full_range) * full_range;
            let mut req_insert_cnt = max_wrapped + encoded - 1;
            if req_insert_cnt > max_value {
                if req_insert_cnt < full_range {
                    return Err(Error::DecompressionFailed);
                } else {
                    req_insert_cnt -= full_range;
                }
            }
            Ok(req_insert_cnt)
        }
    }

    fn read_indexed_static(&mut self) -> Res<Header> {
        let index = self
            .buf
            .read_prefixed_int(HEADER_FIELD_INDEX_STATIC.len())?;
        qtrace!([self], "decoder static indexed {}.", index);

        match HeaderTable::get_static(index) {
            Ok(entry) => Ok((to_string(entry.name())?, to_string(entry.value())?)),
            Err(_) => Err(Error::DecompressionFailed),
        }
    }

    fn read_indexed_dynamic(&mut self, table: &HeaderTable) -> Res<Header> {
        let index = self
            .buf
            .read_prefixed_int(HEADER_FIELD_INDEX_DYNAMIC.len())?;
        qtrace!([self], "decoder dynamic indexed {}.", index);
        match table.get_dynamic(index, self.base, false) {
            Ok(entry) => Ok((to_string(entry.name())?, to_string(entry.value())?)),
            Err(_) => Err(Error::DecompressionFailed),
        }
    }

    fn read_indexed_dynamic_post(&mut self, table: &HeaderTable) -> Res<Header> {
        let index = self
            .buf
            .read_prefixed_int(HEADER_FIELD_INDEX_DYNAMIC_POST.len())?;
        qtrace!([self], "decode post-based {}.", index);
        match table.get_dynamic(index, self.base, true) {
            Ok(entry) => Ok((to_string(entry.name())?, to_string(entry.value())?)),
            Err(_) => Err(Error::DecompressionFailed),
        }
    }

    fn read_literal_with_name_ref_static(&mut self) -> Res<Header> {
        qtrace!(
            [self],
            "read literal with name reference to the static table."
        );

        let index = self
            .buf
            .read_prefixed_int(HEADER_FIELD_LITERAL_NAME_REF_STATIC.len())?;

        match HeaderTable::get_static(index) {
            Ok(entry) => Ok((
                to_string(entry.name())?,
                self.buf.read_literal_from_buffer(0)?,
            )),
            Err(_) => Err(Error::DecompressionFailed),
        }
    }

    fn read_literal_with_name_ref_dynamic(&mut self, table: &HeaderTable) -> Res<Header> {
        qtrace!(
            [self],
            "read literal with name reference ot the dynamic table."
        );

        let index = self
            .buf
            .read_prefixed_int(HEADER_FIELD_LITERAL_NAME_REF_DYNAMIC.len())?;

        match table.get_dynamic(index, self.base, false) {
            Ok(entry) => Ok((
                to_string(entry.name())?,
                self.buf.read_literal_from_buffer(0)?,
            )),
            Err(_) => Err(Error::DecompressionFailed),
        }
    }

    fn read_literal_with_name_ref_dynamic_post(&mut self, table: &HeaderTable) -> Res<Header> {
        qtrace!([self], "decoder literal with post-based index.");

        let index = self
            .buf
            .read_prefixed_int(HEADER_FIELD_LITERAL_NAME_REF_DYNAMIC_POST.len())?;

        match table.get_dynamic(index, self.base, true) {
            Ok(entry) => Ok((
                to_string(entry.name())?,
                self.buf.read_literal_from_buffer(0)?,
            )),
            Err(_) => Err(Error::DecompressionFailed),
        }
    }

    fn read_literal_with_name_literal(&mut self) -> Res<Header> {
        qtrace!([self], "decode literal with name literal.");

        let name = self
            .buf
            .read_literal_from_buffer(HEADER_FIELD_LITERAL_NAME_LITERAL.len())?;

        Ok((name, self.buf.read_literal_from_buffer(0)?))
    }
}

#[cfg(test)]
mod tests {

    use super::{Deref, HeaderDecoder, HeaderDecoderResult, HeaderEncoder, HeaderTable};

    const INDEX_STATIC_TEST: [(u64, &[u8], &str, &str); 4] = [
        (0, &[0x0, 0x0, 0xc0], ":authority", ""),
        (10, &[0x0, 0x0, 0xca], "last-modified", ""),
        (15, &[0x0, 0x0, 0xcf], ":method", "CONNECT"),
        (65, &[0x0, 0x0, 0xff, 0x02], ":status", "206"),
    ];

    const INDEX_DYNAMIC_TEST: [(u64, &[u8], &str, &str); 4] = [
        (0, &[0x02, 0x41, 0xbf, 0x2], "header0", "0"),
        (10, &[0x0c, 0x37, 0xb7], "header10", "10"),
        (15, &[0x11, 0x32, 0xb2], "header15", "15"),
        (65, &[0x43, 0x0, 0x80], "header65", "65"),
    ];

    const INDEX_DYNAMIC_POST_TEST: [(u64, &[u8], &str, &str); 4] = [
        (0, &[0x02, 0x80, 0x10], "header0", "0"),
        (10, &[0x0c, 0x8a, 0x1a], "header10", "10"),
        (15, &[0x11, 0x8f, 0x1f, 0x00], "header15", "15"),
        (65, &[0x43, 0xc1, 0x1f, 0x32], "header65", "65"),
    ];

    const NAME_REF_STATIC: [(u64, &[u8], &str, &str); 4] = [
        (
            0,
            &[
                0x00, 0x00, 0x50, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65, 0x79,
            ],
            ":authority",
            "custom-key",
        ),
        (
            10,
            &[
                0x00, 0x00, 0x5a, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65, 0x79,
            ],
            "last-modified",
            "custom-key",
        ),
        (
            15,
            &[
                0x00, 0x00, 0x5f, 0x00, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65,
                0x79,
            ],
            ":method",
            "custom-key",
        ),
        (
            65,
            &[
                0x00, 0x00, 0x5f, 0x32, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65,
                0x79,
            ],
            ":status",
            "custom-key",
        ),
    ];

    const NAME_REF_DYNAMIC: [(u64, &[u8], &str, &str); 4] = [
        (
            0,
            &[
                0x02, 0x41, 0x4f, 0x32, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65,
                0x79,
            ],
            "header0",
            "custom-key",
        ),
        (
            10,
            &[
                0x0c, 0x37, 0x4f, 0x28, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65,
                0x79,
            ],
            "header10",
            "custom-key",
        ),
        (
            15,
            &[
                0x11, 0x32, 0x4f, 0x23, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65,
                0x79,
            ],
            "header15",
            "custom-key",
        ),
        (
            65,
            &[
                0x43, 0x00, 0x40, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65, 0x79,
            ],
            "header65",
            "custom-key",
        ),
    ];

    const NAME_REF_DYNAMIC_POST: [(u64, &[u8], &str, &str); 4] = [
        (
            0,
            &[
                0x02, 0x80, 0x00, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65, 0x79,
            ],
            "header0",
            "custom-key",
        ),
        (
            10,
            &[
                0x0c, 0x8a, 0x07, 0x03, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65,
                0x79,
            ],
            "header10",
            "custom-key",
        ),
        (
            15,
            &[
                0x11, 0x8f, 0x07, 0x08, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65,
                0x79,
            ],
            "header15",
            "custom-key",
        ),
        (
            65,
            &[
                0x43, 0xc1, 0x07, 0x3a, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65,
                0x79,
            ],
            "header65",
            "custom-key",
        ),
    ];

    const NAME_REF_DYNAMIC_HUFFMAN: [(u64, &[u8], &str, &str); 4] = [
        (
            0,
            &[
                0x02, 0x41, 0x4f, 0x32, 0x88, 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f,
            ],
            "header0",
            "custom-key",
        ),
        (
            10,
            &[
                0x0c, 0x37, 0x4f, 0x28, 0x88, 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f,
            ],
            "header10",
            "custom-key",
        ),
        (
            15,
            &[
                0x11, 0x32, 0x4f, 0x23, 0x88, 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f,
            ],
            "header15",
            "custom-key",
        ),
        (
            65,
            &[
                0x43, 0x00, 0x40, 0x88, 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f,
            ],
            "header65",
            "custom-key",
        ),
    ];

    const VALUE: &[u8] = &[0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65, 0x79];

    const LITERAL_LITERAL: &[u8] = &[
        0x0, 0x42, 0x27, 0x03, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65, 0x79, 0x0a,
        0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65, 0x79,
    ];
    const LITERAL_LITERAL_HUFFMAN: &[u8] = &[
        0x0, 0x42, 0x2f, 0x01, 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f, 0x88, 0x25, 0xa8,
        0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f,
    ];

    const LITERAL_VALUE: &str = "custom-key";

    const TEST_N_BIT: [(&[u8], &str, &str); 4] = [
        (
            &[
                0x02, 0x41, 0x6f, 0x32, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65,
                0x79,
            ],
            "header0",
            "custom-key",
        ),
        (
            &[
                0x02, 0x80, 0x08, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65, 0x79,
            ],
            "header0",
            "custom-key",
        ),
        (
            &[
                0x0, 0x42, 0x37, 0x03, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65, 0x79,
                0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65, 0x79,
            ],
            "custom-key",
            "custom-key",
        ),
        (
            &[
                0x0, 0x42, 0x3f, 0x01, 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f, 0x88, 0x25,
                0xa8, 0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f,
            ],
            "custom-key",
            "custom-key",
        ),
    ];

    #[test]
    fn test_encode_indexed_static() {
        for (index, result, _, _) in &INDEX_STATIC_TEST {
            let mut encoded_h = HeaderEncoder::new(0, true, 1000);
            encoded_h.encode_indexed_static(*index);
            encoded_h.fix_header_block_prefix();
            assert_eq!(&&encoded_h[..], result);
        }
    }

    #[test]
    fn test_encode_indexed_dynamic() {
        for (index, result, _, _) in &INDEX_DYNAMIC_TEST {
            let mut encoded_h = HeaderEncoder::new(66, true, 1000);
            encoded_h.encode_indexed_dynamic(*index);
            encoded_h.fix_header_block_prefix();
            assert_eq!(&encoded_h.deref(), result);
        }
    }

    #[test]
    fn test_encode_indexed_dynamic_post() {
        for (index, result, _, _) in &INDEX_DYNAMIC_POST_TEST {
            let mut encoded_h = HeaderEncoder::new(0, true, 1000);
            encoded_h.encode_indexed_dynamic(*index);
            encoded_h.fix_header_block_prefix();
            assert_eq!(&encoded_h.deref(), result);
        }
    }

    #[test]
    fn test_encode_literal_with_name_ref_static() {
        for (index, result, _, _) in &NAME_REF_STATIC {
            let mut encoded_h = HeaderEncoder::new(0, false, 1000);
            encoded_h.encode_literal_with_name_ref(true, *index, VALUE);
            encoded_h.fix_header_block_prefix();
            assert_eq!(&encoded_h.deref(), result);
        }
    }

    #[test]
    fn test_encode_literal_with_name_ref_dynamic() {
        for (index, result, _, _) in &NAME_REF_DYNAMIC {
            let mut encoded_h = HeaderEncoder::new(66, false, 1000);
            encoded_h.encode_literal_with_name_ref(false, *index, VALUE);
            encoded_h.fix_header_block_prefix();
            assert_eq!(&encoded_h.deref(), result);
        }
    }

    #[test]
    fn test_encode_literal_with_name_ref_dynamic_post() {
        for (index, result, _, _) in &NAME_REF_DYNAMIC_POST {
            let mut encoded_h = HeaderEncoder::new(0, false, 1000);
            encoded_h.encode_literal_with_name_ref(false, *index, VALUE);
            encoded_h.fix_header_block_prefix();
            assert_eq!(&encoded_h.deref(), result);
        }
    }

    #[test]
    fn test_encode_literal_with_name_ref_dynamic_huffman() {
        for (index, result, _, _) in &NAME_REF_DYNAMIC_HUFFMAN {
            let mut encoded_h = HeaderEncoder::new(66, true, 1000);
            encoded_h.encode_literal_with_name_ref(false, *index, VALUE);
            encoded_h.fix_header_block_prefix();
            assert_eq!(&encoded_h.deref(), result);
        }
    }
    #[test]
    fn test_encode_literal_with_literal() {
        let mut encoded_h = HeaderEncoder::new(66, false, 1000);
        encoded_h.encode_literal_with_name_literal(VALUE, VALUE);
        encoded_h.fix_header_block_prefix();
        assert_eq!(&encoded_h.deref(), &LITERAL_LITERAL);

        let mut encoded_h = HeaderEncoder::new(66, true, 1000);
        encoded_h.encode_literal_with_name_literal(VALUE, VALUE);
        encoded_h.fix_header_block_prefix();
        assert_eq!(&encoded_h.deref(), &LITERAL_LITERAL_HUFFMAN);
    }

    #[test]
    fn decode_indexed_static() {
        for (_, encoded, decoded1, decoded2) in &INDEX_STATIC_TEST {
            let table = HeaderTable::new(false);
            let mut decoder_h = HeaderDecoder::new(encoded);
            if let HeaderDecoderResult::Headers(result) =
                decoder_h.decode_header_block(&table, 1000, 0).unwrap()
            {
                assert_eq!(result.len(), 1);
                assert_eq!(result[0].0, *decoded1);
                assert_eq!(result[0].1, *decoded2);
            } else {
                panic!("No headers");
            }
        }
    }

    fn fill_table(table: &mut HeaderTable) {
        table.set_capacity(10000).unwrap();
        for i in 0..66 {
            let mut v = b"header".to_vec();
            let mut num = i.to_string().as_bytes().to_vec();
            v.append(&mut num);
            table.insert(&v[..], i.to_string().as_bytes()).unwrap();
        }
    }

    #[test]
    fn decode_indexed_dynamic() {
        for (_, encoded, decoded1, decoded2) in &INDEX_DYNAMIC_TEST {
            let mut table = HeaderTable::new(false);
            fill_table(&mut table);
            let mut decoder_h = HeaderDecoder::new(encoded);
            if let HeaderDecoderResult::Headers(result) =
                decoder_h.decode_header_block(&table, 1000, 0).unwrap()
            {
                assert_eq!(result.len(), 1);
                assert_eq!(result[0].0, *decoded1);
                assert_eq!(result[0].1, *decoded2);
            } else {
                panic!("No headers");
            }
        }
    }

    #[test]
    fn decode_indexed_dynamic_post() {
        for (_, encoded, decoded1, decoded2) in &INDEX_DYNAMIC_POST_TEST {
            let mut table = HeaderTable::new(false);
            fill_table(&mut table);
            let mut decoder_h = HeaderDecoder::new(encoded);
            if let HeaderDecoderResult::Headers(result) =
                decoder_h.decode_header_block(&table, 1000, 0).unwrap()
            {
                assert_eq!(result.len(), 1);
                assert_eq!(result[0].0, *decoded1);
                assert_eq!(result[0].1, *decoded2);
            } else {
                panic!("No headers");
            }
        }
    }

    #[test]
    fn decode_literal_with_name_ref_static() {
        for (_, encoded, decoded1, decoded2) in &NAME_REF_STATIC {
            let table = HeaderTable::new(false);
            let mut decoder_h = HeaderDecoder::new(encoded);
            if let HeaderDecoderResult::Headers(result) =
                decoder_h.decode_header_block(&table, 1000, 0).unwrap()
            {
                assert_eq!(result.len(), 1);
                assert_eq!(result[0].0, *decoded1);
                assert_eq!(result[0].1, *decoded2);
            } else {
                panic!("No headers");
            }
        }
    }

    #[test]
    fn decode_literal_with_name_ref_dynamic() {
        for (_, encoded, decoded1, decoded2) in &NAME_REF_DYNAMIC {
            let mut table = HeaderTable::new(false);
            fill_table(&mut table);
            let mut decoder_h = HeaderDecoder::new(encoded);
            if let HeaderDecoderResult::Headers(result) =
                decoder_h.decode_header_block(&table, 1000, 0).unwrap()
            {
                assert_eq!(result.len(), 1);
                assert_eq!(result[0].0, *decoded1);
                assert_eq!(result[0].1, *decoded2);
            } else {
                panic!("No headers");
            }
        }
    }

    #[test]
    fn decode_literal_with_name_ref_dynamic_post() {
        for (_, encoded, decoded1, decoded2) in &NAME_REF_DYNAMIC_POST {
            let mut table = HeaderTable::new(false);
            fill_table(&mut table);
            let mut decoder_h = HeaderDecoder::new(encoded);
            if let HeaderDecoderResult::Headers(result) =
                decoder_h.decode_header_block(&table, 1000, 0).unwrap()
            {
                assert_eq!(result.len(), 1);
                assert_eq!(result[0].0, *decoded1);
                assert_eq!(result[0].1, *decoded2);
            } else {
                panic!("No headers");
            }
        }
    }

    #[test]
    fn decode_literal_with_name_ref_dynamic_huffman() {
        for (_, encoded, decoded1, decoded2) in &NAME_REF_DYNAMIC_HUFFMAN {
            let mut table = HeaderTable::new(false);
            fill_table(&mut table);
            let mut decoder_h = HeaderDecoder::new(encoded);
            if let HeaderDecoderResult::Headers(result) =
                decoder_h.decode_header_block(&table, 1000, 0).unwrap()
            {
                assert_eq!(result.len(), 1);
                assert_eq!(result[0].0, *decoded1);
                assert_eq!(result[0].1, *decoded2);
            } else {
                panic!("No headers");
            }
        }
    }

    #[test]
    fn decode_literal_literal() {
        let mut table = HeaderTable::new(false);
        fill_table(&mut table);
        let mut decoder_h = HeaderDecoder::new(LITERAL_LITERAL);
        if let HeaderDecoderResult::Headers(result) =
            decoder_h.decode_header_block(&table, 1000, 0).unwrap()
        {
            assert_eq!(result.len(), 1);
            assert_eq!(result[0].0, *LITERAL_VALUE);
            assert_eq!(result[0].1, *LITERAL_VALUE);
        } else {
            panic!("No headers");
        }

        let mut decoder_h = HeaderDecoder::new(LITERAL_LITERAL_HUFFMAN);
        if let HeaderDecoderResult::Headers(result) =
            decoder_h.decode_header_block(&table, 1000, 0).unwrap()
        {
            assert_eq!(result.len(), 1);
            assert_eq!(result[0].0, *LITERAL_VALUE);
            assert_eq!(result[0].1, *LITERAL_VALUE);
        } else {
            panic!("No headers");
        }
    }

    // Test that we are ignoring N-bit.
    #[test]
    fn decode_ignore_n_bit() {
        for (encoded, decoded1, decoded2) in &TEST_N_BIT {
            let mut table = HeaderTable::new(false);
            fill_table(&mut table);
            let mut decoder_h = HeaderDecoder::new(encoded);
            if let HeaderDecoderResult::Headers(result) =
                decoder_h.decode_header_block(&table, 1000, 0).unwrap()
            {
                assert_eq!(result.len(), 1);
                assert_eq!(result[0].0, *decoded1);
                assert_eq!(result[0].1, *decoded2);
            } else {
                panic!("No headers");
            }
        }
    }
}
