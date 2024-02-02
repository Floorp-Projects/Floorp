// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::convert::TryFrom;

use lazy_static::lazy_static;

use crate::huffman_table::HUFFMAN_TABLE;

pub struct HuffmanDecoderNode {
    pub next: [Option<Box<HuffmanDecoderNode>>; 2],
    pub value: Option<u16>,
}

lazy_static! {
    pub static ref HUFFMAN_DECODE_ROOT: HuffmanDecoderNode = make_huffman_tree(0, 0);
}

fn make_huffman_tree(prefix: u32, len: u8) -> HuffmanDecoderNode {
    let mut found = false;
    let mut next = [None, None];
    for (i, iter) in HUFFMAN_TABLE.iter().enumerate() {
        if iter.len <= len {
            continue;
        }
        if (iter.val >> (iter.len - len)) != prefix {
            continue;
        }

        found = true;
        if iter.len == len + 1 {
            // This is a leaf
            let bit = usize::try_from(iter.val & 1).unwrap();
            next[bit] = Some(Box::new(HuffmanDecoderNode {
                next: [None, None],
                value: Some(u16::try_from(i).unwrap()),
            }));
            if next[bit ^ 1].is_some() {
                return HuffmanDecoderNode { next, value: None };
            }
        }
    }

    if found {
        if next[0].is_none() {
            next[0] = Some(Box::new(make_huffman_tree(prefix << 1, len + 1)));
        }
        if next[1].is_none() {
            next[1] = Some(Box::new(make_huffman_tree((prefix << 1) + 1, len + 1)));
        }
    }

    HuffmanDecoderNode { next, value: None }
}
