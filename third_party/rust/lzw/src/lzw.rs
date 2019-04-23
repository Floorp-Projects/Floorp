//! This modules provides an implementation of the Lempel–Ziv–Welch Compression Algorithm

// Note: This implementation borrows heavily from the work of Julius Pettersson
// See http://www.cplusplus.com/articles/iL18T05o/ for his extensive explanations
// and a C++ implementatation

use std::io;
use std::io::{Read, Write};

use bitstream::{Bits, BitReader, BitWriter};

const MAX_CODESIZE: u8 = 12;
const MAX_ENTRIES: usize = 1 << MAX_CODESIZE as usize;

/// Alias for a LZW code point
type Code = u16;

/// Decoding dictionary.
///
/// It is not generic due to current limitations of Rust
/// Inspired by http://www.cplusplus.com/articles/iL18T05o/
#[derive(Debug)]
struct DecodingDict {
    min_size: u8,
    table: Vec<(Option<Code>, u8)>,
    buffer: Vec<u8>,
}

impl DecodingDict {
    /// Creates a new dict
    fn new(min_size: u8) -> DecodingDict {
        DecodingDict {
            min_size: min_size,
            table: Vec::with_capacity(512),
            buffer: Vec::with_capacity((1 << MAX_CODESIZE as usize) - 1)
        }
    }

    /// Resets the dictionary
    fn reset(&mut self) {
        self.table.clear();
        for i in 0..(1u16 << self.min_size as usize) {
            self.table.push((None, i as u8));
        }
    }

    /// Inserts a value into the dict
    #[inline(always)]
    fn push(&mut self, key: Option<Code>, value: u8) {
        self.table.push((key, value))
    }

    /// Reconstructs the data for the corresponding code
    fn reconstruct(&mut self, code: Option<Code>) -> io::Result<&[u8]> {
        self.buffer.clear();
        let mut code = code;
        let mut cha;
        // Check the first access more thoroughly since a bad code
        // could occur if the data is malformed
        if let Some(k) = code {
            match self.table.get(k as usize) {
                Some(&(code_, cha_)) => {
                    code = code_;
                    cha = cha_;
                }
                None => return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    &*format!("Invalid code {:X}, expected code <= {:X}", k, self.table.len())
                ))
            }
            self.buffer.push(cha);
        }
        while let Some(k) = code {
            if self.buffer.len() >= MAX_ENTRIES { 
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "Invalid code sequence. Cycle in decoding table."
                ))
            }
            //(code, cha) = self.table[k as usize];
            // Note: This could possibly be replaced with an unchecked array access if
            //  - value is asserted to be < self.next_code() in push
            //  - min_size is asserted to be < MAX_CODESIZE 
            let entry = self.table[k as usize]; code = entry.0; cha = entry.1;
            self.buffer.push(cha);
        }
        self.buffer.reverse();
        Ok(&self.buffer)
    }

    /// Returns the buffer constructed by the last reconstruction
    #[inline(always)]
    fn buffer(&self) -> &[u8] {
        &self.buffer
    }

    /// Number of entries in the dictionary
    #[inline(always)]
    fn next_code(&self) -> u16 {
        self.table.len() as u16
    }
}

macro_rules! define_decoder_struct {
    {$(
        $name:ident, $offset:expr, #[$doc:meta];
    )*} => {

$( // START struct definition

#[$doc]
/// 
/// The maximum supported code size is 16 bits. The decoder assumes two
/// special code word to be present in the stream:
///
///  * `CLEAR_CODE == 1 << min_code_size`
///  * `END_CODE   == CLEAR_CODE + 1`
///
///Furthermore the decoder expects the stream to start with a `CLEAR_CODE`. This
/// corresponds to the implementation needed for en- and decoding GIF and TIFF files.
#[derive(Debug)]
pub struct $name<R: BitReader> {
    r: R,
    prev: Option<Code>,
    table: DecodingDict,
    buf: [u8; 1],
    code_size: u8,
    min_code_size: u8,
    clear_code: Code,
    end_code: Code,
}

impl<R> $name<R> where R: BitReader {
    /// Creates a new LZW decoder. 
    pub fn new(reader: R, min_code_size: u8) -> $name<R> {
        $name {
            r: reader,
            prev: None,
            table: DecodingDict::new(min_code_size),
            buf: [0; 1],
            code_size: min_code_size + 1,
            min_code_size: min_code_size,
            clear_code: 1 << min_code_size,
            end_code: (1 << min_code_size) + 1,
        }
    }
    
    /// Tries to obtain and decode a code word from `bytes`.
    ///
    /// Returns the number of bytes that have been consumed from `bytes`. An empty
    /// slice does not indicate `EOF`.
    pub fn decode_bytes(&mut self, bytes: &[u8]) -> io::Result<(usize, &[u8])> {
        Ok(match self.r.read_bits(bytes, self.code_size) {
            Bits::Some(consumed, code) => {
                (consumed, if code == self.clear_code {
                    self.table.reset();
                    self.table.push(None, 0); // clear code
                    self.table.push(None, 0); // end code
                    self.code_size = self.min_code_size + 1;
                    self.prev = None;
                    &[]
                } else if code == self.end_code {
                    &[]
                } else {
                    let next_code = self.table.next_code();
                    if code > next_code {
                        return Err(io::Error::new(
                            io::ErrorKind::InvalidInput,
                            &*format!("Invalid code {:X}, expected code <= {:X}",
                                      code,
                                      next_code
                            )
                        ))
                    }
                    let prev = self.prev;
                    let result = if prev.is_none() {
                        self.buf = [code as u8];
                        &self.buf[..]
                    } else {
                        let data = if code == next_code {
                            let cha = try!(self.table.reconstruct(prev))[0];
                            self.table.push(prev, cha);
                            try!(self.table.reconstruct(Some(code)))
                        } else if code < next_code {
                            let cha = try!(self.table.reconstruct(Some(code)))[0];
                            self.table.push(prev, cha);
                            self.table.buffer()
                        } else {
                            // code > next_code is already tested a few lines earlier
                            unreachable!()
                        };
                        data
                    };
                    if next_code == (1 << self.code_size as usize) - 1 - $offset
                       && self.code_size < MAX_CODESIZE {
                        self.code_size += 1;
                    }
                    self.prev = Some(code);
                    result
                })
            },
            Bits::None(consumed) => {
                (consumed, &[])
            }
        })
    }
}

)* // END struct definition

    }
}

define_decoder_struct!{
    Decoder, 0, #[doc = "Decoder for a LZW compressed stream (this algorithm is used for GIF files)."];
    DecoderEarlyChange, 1, #[doc = "Decoder for a LZW compressed stream using an “early change” algorithm (used in TIFF files)."];
}

struct Node {
    prefix: Option<Code>,
    c: u8,
    left: Option<Code>,
    right: Option<Code>,
}

impl Node {
    #[inline(always)]
    fn new(c: u8) -> Node {
        Node {
            prefix: None,
            c: c,
            left: None,
            right: None
        }
    }
}

struct EncodingDict {
    table: Vec<Node>,
    min_size: u8,

}

/// Encoding dictionary based on a binary tree
impl EncodingDict {
    fn new(min_size: u8) -> EncodingDict {
        let mut this = EncodingDict {
            table: Vec::with_capacity(MAX_ENTRIES),
            min_size: min_size
        };
        this.reset();
        this
    }

    fn reset(&mut self) {
        self.table.clear();
        for i in 0 .. (1u16 << self.min_size as usize) {
            self.push_node(Node::new(i as u8));
        }
    }

    #[inline(always)]
    fn push_node(&mut self, node: Node) {
        self.table.push(node)
    }

    #[inline(always)]
    fn clear_code(&self) -> Code {
        1u16 << self.min_size
    }

    #[inline(always)]
    fn end_code(&self) -> Code {
        self.clear_code() + 1
    }

    // Searches for a new prefix
    fn search_and_insert(&mut self, i: Option<Code>, c: u8) -> Option<Code> {
        if let Some(i) = i.map(|v| v as usize) {
            let table_size = self.table.len() as Code;
            if let Some(mut j) = self.table[i].prefix {
                loop {
                    let entry = &mut self.table[j as usize];
                    if c < entry.c {
                        if let Some(k) = entry.left {
                            j = k
                        } else {
                            entry.left = Some(table_size);
                            break
                        }
                    } else if c > entry.c {
                        if let Some(k) = entry.right {
                            j = k
                        } else {
                            entry.right = Some(table_size);
                            break
                        }
                    } else {
                        return Some(j)
                    }
                }
            } else {
                self.table[i].prefix = Some(table_size);
            }
            self.table.push(Node::new(c));
            None
        } else {
            Some(self.search_initials(c as Code))
        }
    }

    fn next_code(&self) -> usize {
        self.table.len()
    }

    fn search_initials(&self, i: Code) -> Code {
        self.table[i as usize].c as Code
    }
}

/// Convenience function that reads and compresses all bytes from `R`.
pub fn encode<R, W>(r: R, mut w: W, min_code_size: u8) -> io::Result<()>
where R: Read, W: BitWriter {
    let mut dict = EncodingDict::new(min_code_size);
    dict.push_node(Node::new(0)); // clear code
    dict.push_node(Node::new(0)); // end code
    let mut code_size = min_code_size + 1;
    let mut i = None;
    // gif spec: first clear code
    try!(w.write_bits(dict.clear_code(), code_size));
    let mut r = r.bytes();
    while let Some(Ok(c)) = r.next() {
        let prev = i;
        i = dict.search_and_insert(prev, c);
        if i.is_none() {
            if let Some(code) = prev {
                try!(w.write_bits(code, code_size));
            }
            i = Some(dict.search_initials(c as Code))
        }
        // There is a hit: do not write out code but continue
        let next_code = dict.next_code();
        if next_code > (1 << code_size as usize)
           && code_size < MAX_CODESIZE {
            code_size += 1;
        }
        if next_code > MAX_ENTRIES {
            dict.reset();
            dict.push_node(Node::new(0)); // clear code
            dict.push_node(Node::new(0)); // end code
            try!(w.write_bits(dict.clear_code(), code_size));
            code_size = min_code_size + 1;
        }

    }
    if let Some(code) = i {
        try!(w.write_bits(code, code_size));
    }
    try!(w.write_bits(dict.end_code(), code_size));
    try!(w.flush());
    Ok(())
}

/// LZW encoder using the algorithm of GIF files.
pub struct Encoder<W: BitWriter> {
    w: W,
    dict: EncodingDict,
    min_code_size: u8,
    code_size: u8,
    i: Option<Code>
}

impl<W: BitWriter> Encoder<W> {
    /// Creates a new LZW encoder.
    ///
    /// **Note**: If `min_code_size < 8` then `Self::encode_bytes` might panic when
    /// the supplied data containts values that exceed `1 << min_code_size`.
    pub fn new(mut w: W, min_code_size: u8) -> io::Result<Encoder<W>> {
        let mut dict = EncodingDict::new(min_code_size);
        dict.push_node(Node::new(0)); // clear code
        dict.push_node(Node::new(0)); // end code
        let code_size = min_code_size + 1;
        try!(w.write_bits(dict.clear_code(), code_size));
        Ok(Encoder {
            w: w,
            dict: dict,
            min_code_size: min_code_size,
            code_size: code_size,
            i: None
        })
    }
    
    /// Compresses `bytes` and writes the result into the writer.
    ///
    /// ## Panics
    ///
    /// This function might panic if any of the input bytes exceeds `1 << min_code_size`.
    /// This cannot happen if `min_code_size >= 8`.
    pub fn encode_bytes(&mut self, bytes: &[u8]) -> io::Result<()> {
        let w = &mut self.w;
        let dict = &mut self.dict;
        let code_size = &mut self.code_size;
        let i = &mut self.i;
        for &c in bytes {
            let prev = *i;
            *i = dict.search_and_insert(prev, c);
            if i.is_none() {
                if let Some(code) = prev {
                    try!(w.write_bits(code, *code_size));
                }
                *i = Some(dict.search_initials(c as Code))
            }
            // There is a hit: do not write out code but continue
            let next_code = dict.next_code();
            if next_code > (1 << *code_size as usize)
               && *code_size < MAX_CODESIZE {
                *code_size += 1;
            }
            if next_code > MAX_ENTRIES {
                dict.reset();
                dict.push_node(Node::new(0)); // clear code
                dict.push_node(Node::new(0)); // end code
                try!(w.write_bits(dict.clear_code(), *code_size));
                *code_size = self.min_code_size + 1;
            }

        }
        Ok(())
    }
}

impl<W: BitWriter> Drop for Encoder<W> {
    #[cfg(feature = "raii_no_panic")]
    fn drop(&mut self) {
        let w = &mut self.w;
        let code_size = &mut self.code_size;
        
        if let Some(code) = self.i {
            let _ = w.write_bits(code, *code_size);
        }
        let _ = w.write_bits(self.dict.end_code(), *code_size);
        let _ = w.flush();
    }
    #[cfg(not(feature = "raii_no_panic"))]
    fn drop(&mut self) {
        (|| {
            let w = &mut self.w;
            let code_size = &mut self.code_size;
            if let Some(code) = self.i {
                try!(w.write_bits(code, *code_size));
            }
            try!(w.write_bits(self.dict.end_code(), *code_size));
            w.flush()
         })().unwrap()
        
    }
}

#[cfg(test)]
#[test]
fn round_trip() {
    use {LsbWriter, LsbReader};
    
    let size = 8;
    let data = b"TOBEORNOTTOBEORTOBEORNOT";
    let mut compressed = vec![];
    {
        let mut enc = Encoder::new(LsbWriter::new(&mut compressed), size).unwrap();
        enc.encode_bytes(data).unwrap();
    }
    println!("{:?}", compressed);
    let mut dec = Decoder::new(LsbReader::new(), size);
    let mut compressed = &compressed[..];
    let mut data2 = vec![];
    while compressed.len() > 0 {
        let (start, bytes) = dec.decode_bytes(&compressed).unwrap();
        compressed = &compressed[start..];
        data2.extend(bytes.iter().map(|&i| i));
    }
    assert_eq!(data2, data)
}