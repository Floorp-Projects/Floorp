//! Decompresses the input from stdin and writes the result to stdout.

extern crate lzw;

use std::io::{self, Write, BufWriter, BufRead};

fn main() {
    match (|| -> io::Result<()> {
        let mut decoder = 
            lzw::Decoder::new(lzw::LsbReader::new(), 8)
        ;
        let stdout = io::stdout();
        let mut stdout = BufWriter::new(stdout.lock());
        let stdin = io::stdin();
        let mut stdin = stdin.lock();
        loop {
            let len = {
                let buf = try!(stdin.fill_buf());
                if buf.len() == 0 {
                    break
                }
                let (len, bytes) = try!(decoder.decode_bytes(buf));
                try!(stdout.write_all(bytes));
                len
            };
            stdin.consume(len);
        }
        Ok(())
    })() {
        Ok(()) => (),
        Err(err) => { let _ = write!(io::stderr(), "{}", err); }
    }
    
}