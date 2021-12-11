// This file is heavily based on https://bitbucket.org/marshallpierce/rust-auxv
// Thus I'm keeping the original MIT-license copyright here:
// Copyright 2017 Marshall Pierce
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be in…substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
use crate::errors::AuxvReaderError;
use byteorder::{NativeEndian, ReadBytesExt};
use std::fs::File;
use std::io::{BufReader, Read};

pub type Result<T> = std::result::Result<T, AuxvReaderError>;

/// The type used in auxv keys and values.
#[cfg(target_pointer_width = "32")]
pub type AuxvType = u32;
/// The type used in auxv keys and values.
#[cfg(target_pointer_width = "64")]
pub type AuxvType = u64;

/// An auxv key-value pair.
#[derive(Debug, PartialEq)]
pub struct AuxvPair {
    pub key: AuxvType,
    pub value: AuxvType,
}

/// An iterator across auxv pairs froom procfs.
pub struct ProcfsAuxvIter {
    pair_size: usize,
    buf: Vec<u8>,
    input: BufReader<File>,
    keep_going: bool,
}

impl ProcfsAuxvIter {
    pub fn new(input: BufReader<File>) -> Self {
        let pair_size = 2 * std::mem::size_of::<AuxvType>();
        let buf: Vec<u8> = Vec::with_capacity(pair_size);

        ProcfsAuxvIter {
            pair_size,
            buf,
            input,
            keep_going: true,
        }
    }
}

impl Iterator for ProcfsAuxvIter {
    type Item = Result<AuxvPair>;
    fn next(&mut self) -> Option<Self::Item> {
        if !self.keep_going {
            return None;
        }
        // assume something will fail
        self.keep_going = false;

        self.buf = vec![0; self.pair_size];

        let mut read_bytes: usize = 0;
        while read_bytes < self.pair_size {
            // read exactly buf's len of bytes.
            match self.input.read(&mut self.buf[read_bytes..]) {
                Ok(n) => {
                    if n == 0 {
                        // should not hit EOF before AT_NULL
                        return Some(Err(AuxvReaderError::InvalidFormat));
                    }

                    read_bytes += n;
                }
                Err(x) => return Some(Err(x.into())),
            }
        }

        let mut reader = &self.buf[..];
        let aux_key = match read_long(&mut reader) {
            Ok(x) => x,
            Err(x) => return Some(Err(x.into())),
        };
        let aux_val = match read_long(&mut reader) {
            Ok(x) => x,
            Err(x) => return Some(Err(x.into())),
        };

        let at_null;
        #[cfg(target_arch = "arm")]
        {
            at_null = 0;
        }
        #[cfg(not(target_arch = "arm"))]
        {
            at_null = libc::AT_NULL;
        }

        if aux_key == at_null {
            return None;
        }

        self.keep_going = true;
        Some(Ok(AuxvPair {
            key: aux_key,
            value: aux_val,
        }))
    }
}

fn read_long(reader: &mut dyn Read) -> std::io::Result<AuxvType> {
    match std::mem::size_of::<AuxvType>() {
        4 => reader.read_u32::<NativeEndian>().map(|u| u as AuxvType),
        8 => reader.read_u64::<NativeEndian>().map(|u| u as AuxvType),
        x => panic!("Unexpected type width: {}", x),
    }
}
