#![cfg(test)]

#[cfg(feature = "gzip")]
use flate2::read::GzDecoder;

fn get_test_file_data(name: &str) -> Vec<u8> {
    use std::fs::File;
    use std::io::Read;
    let mut input = Vec::new();
    let mut f = File::open(name).unwrap();

    f.read_to_end(&mut input).unwrap();
    input
}

pub fn get_test_data() -> Vec<u8> {
    use std::env;
    let path = env::var("TEST_FILE").unwrap_or("tests/pg11.txt".to_string());
    get_test_file_data(&path)
}

/// Helper function to decompress into a `Vec<u8>`
pub fn decompress_to_end(input: &[u8]) -> Vec<u8> {
    // use std::str;
    // let mut inflater = super::inflate::InflateStream::new();
    // let mut out = Vec::new();
    // let mut n = 0;
    // println!("input len {}", input.len());
    // while n < input.len() {
    // let res = inflater.update(&input[n..]) ;
    // if let Ok((num_bytes_read, result)) = res {
    // println!("result len {}, bytes_read {}", result.len(), num_bytes_read);
    // n += num_bytes_read;
    // out.extend(result);
    // } else {
    // println!("Output: `{}`", str::from_utf8(&out).unwrap());
    // println!("Output decompressed: {}", out.len());
    // res.unwrap();
    // }
    //
    // }
    // out

    use std::io::Read;
    use flate2::read::DeflateDecoder;

    let mut result = Vec::new();
    let i = &input[..];
    let mut e = DeflateDecoder::new(i);

    let res = e.read_to_end(&mut result);
    if let Ok(_) = res {
        //        println!("{} bytes decompressed successfully", n);
    } else {
        println!("result size: {}", result.len());
        res.unwrap();
    }
    result
}

#[cfg(feature = "gzip")]
pub fn decompress_gzip(compressed: &[u8]) -> (GzDecoder<&[u8]>, Vec<u8>) {
    use std::io::Read;
    let mut e = GzDecoder::new(&compressed[..]).unwrap();

    let mut result = Vec::new();
    e.read_to_end(&mut result).unwrap();
    (e, result)
}

pub fn decompress_zlib(compressed: &[u8]) -> Vec<u8> {
    use std::io::Read;
    use flate2::read::ZlibDecoder;
    let mut e = ZlibDecoder::new(&compressed[..]);

    let mut result = Vec::new();
    e.read_to_end(&mut result).unwrap();
    result
}
