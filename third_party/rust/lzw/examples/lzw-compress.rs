//! Compresses the input from stdin and writes the result to stdout.

extern crate lzw;

use std::io::{self, Write, BufRead};

fn main() {
    match (|| -> io::Result<()> {
        let mut encoder = try!(
            lzw::Encoder::new(lzw::LsbWriter::new(io::stdout()), 8)
        );
        let stdin = io::stdin();
        let mut stdin = stdin.lock();
        loop {
            let len = {
                let buf = try!(stdin.fill_buf());
                try!(encoder.encode_bytes(buf));
                buf.len()
            };
            if len == 0 {
                break
            }
            stdin.consume(len);
        }
        Ok(())
    })() {
        Ok(()) => (),
        Err(err) => { let _ = write!(io::stderr(), "{}", err); }
    }
    
}