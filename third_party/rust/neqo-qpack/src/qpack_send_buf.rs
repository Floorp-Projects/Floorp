// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(unused_variables, dead_code)]

use std::ops::Deref;

#[derive(Default, Debug, PartialEq)]
pub struct QPData {
    buf: Vec<u8>,
}

impl QPData {
    pub fn len(&self) -> usize {
        self.buf.len()
    }

    pub fn write_byte(&mut self, b: u8) {
        self.buf.push(b);
    }

    pub fn encode_prefixed_encoded_int(&mut self, prefix: u8, prefix_len: u8, mut val: u64) {
        if prefix_len > 7 {
            panic!("invalid QPACK integer prefix")
        }

        let first_byte_max: u8 = if prefix_len == 0 {
            0xff
        } else {
            (1 << (8 - prefix_len)) - 1
        };

        if val < u64::from(first_byte_max) {
            self.write_byte((prefix & !first_byte_max) | (val as u8));
            return;
        }
        self.write_byte(prefix | first_byte_max);
        val -= u64::from(first_byte_max);

        let mut done = false;
        while !done {
            let mut b = (val as u8) & 0x7f;
            val >>= 7;
            if val > 0 {
                b |= 0x80;
            } else {
                done = true;
            }
            self.write_byte(b);
        }
    }

    pub fn encode_prefixed_encoded_int_fix(
        &mut self,
        offset: usize,
        prefix: u8,
        prefix_len: u8,
        mut val: u64,
    ) -> usize {
        if prefix_len > 7 {
            panic!("invalid HPACK integer prefix")
        }

        let first_byte_max: u8 = if prefix_len == 0 {
            0xff
        } else {
            (1 << (8 - prefix_len)) - 1
        };

        if val < u64::from(first_byte_max) {
            self.buf[offset] = (prefix & !first_byte_max) | (val as u8);
            return 1;
        }
        self.write_byte(prefix | first_byte_max);
        val -= u64::from(first_byte_max);

        let mut written = 1;
        let mut done = false;
        while !done {
            let mut b = (val as u8) & 0x7f;
            val >>= 7;
            if val > 0 {
                b |= 0x80;
            } else {
                done = true;
            }
            self.buf[offset + written] = b;
            written += 1;
        }
        written
    }

    pub fn write_bytes(&mut self, buf: &[u8]) {
        self.buf.extend_from_slice(buf);
    }

    pub fn read(&mut self, r: usize) {
        if r > self.buf.len() {
            panic!("want to set more byte read than remaing in the buffer.");
        }

        self.buf = self.buf.split_off(r);
    }
}

impl Deref for QPData {
    type Target = [u8];
    fn deref(&self) -> &Self::Target {
        self.buf.deref()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_encode_prefixed_encoded_int_1() {
        let mut d = QPData::default();
        d.encode_prefixed_encoded_int(0xff, 2, 5);
        assert_eq!(d[..], [0xc5]);
    }

    #[test]
    fn test_encode_prefixed_encoded_int_2() {
        let mut d = QPData::default();
        d.encode_prefixed_encoded_int(0xff, 2, 65);
        assert_eq!(d[..], [0xff, 0x02]);
    }

    #[test]
    fn test_encode_prefixed_encoded_int_3() {
        let mut d = QPData::default();
        d.encode_prefixed_encoded_int(0xff, 2, 100_000);
        assert_eq!(d[..], [0xff, 0xe1, 0x8c, 0x06]);
    }
}
