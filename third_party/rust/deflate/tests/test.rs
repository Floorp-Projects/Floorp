extern crate deflate;
extern crate flate2;

use std::io::{Write, Read};
use deflate::CompressionOptions;

fn get_test_file_data(name: &str) -> Vec<u8> {
    use std::fs::File;
    let mut input = Vec::new();
    let mut f = File::open(name).unwrap();

    f.read_to_end(&mut input).unwrap();
    input
}

fn get_test_data() -> Vec<u8> {
    use std::env;
    let path = env::var("TEST_FILE").unwrap_or_else(|_| "tests/pg11.txt".to_string());
    get_test_file_data(&path)
}

fn roundtrip(data: &[u8]) {
    roundtrip_conf(data, CompressionOptions::default())
}

fn roundtrip_conf(data: &[u8], level: CompressionOptions) {
    let compressed = deflate::deflate_bytes_zlib_conf(data, level);
    println!("Compressed len: {}, level: {:?}", compressed.len(), level);
    let decompressed = {
        let mut d = flate2::read::ZlibDecoder::new(compressed.as_slice());
        let mut out = Vec::new();
        d.read_to_end(&mut out).unwrap();
        out
    };
    assert!(decompressed.as_slice() == data);
}

// A test comparing the compression ratio of the library with flate2
#[test]
fn file_zlib_compare_output() {
    use flate2::Compression;
    let test_data = get_test_data();
    let flate2_compressed = {
        let mut e = flate2::write::ZlibEncoder::new(Vec::new(), Compression::best());
        e.write_all(&test_data).unwrap();
        e.finish().unwrap()
    };

    // {
    //     use std::fs::File;
    //     use std::io::Write;
    //     {
    //         let mut f = File::create("out.deflate").unwrap();
    //         f.write_all(&deflate_compressed).unwrap();
    //     }
    //     {
    //         let mut f = File::create("out.flate2").unwrap();
    //         f.write_all(&flate2_compressed).unwrap();
    //     }
    // }

    println!("flate2 len: {}", flate2_compressed.len(),);

    roundtrip_conf(&test_data, CompressionOptions::high());
}

#[test]
fn block_type() {
    let test_file = "tests/short.bin";
    let test_data = get_test_file_data(test_file);
    let compressed = deflate::deflate_bytes_zlib(&test_data);
    assert_eq!(compressed.len(), 30);

    roundtrip(b"test");
}

#[test]
fn issue_17() {
    // This is window size + 1 bytes long which made the hash table
    // slide when there was only the two end-bytes that don't need to be hashed left
    // and triggered an assertion.
    let data = vec![0; 65537];

    roundtrip(&data);
}

#[test]
fn fast() {
    let test_data = get_test_data();
    roundtrip_conf(&test_data, CompressionOptions::fast());
}

#[test]
fn rle() {
    use deflate::{deflate_bytes_conf, CompressionOptions};
    let test_data = get_test_data();
    let compressed = deflate_bytes_conf(&test_data, CompressionOptions::rle());
    let decompressed = {
        let mut d = flate2::read::DeflateDecoder::new(compressed.as_slice());
        let mut out = Vec::new();
        d.read_to_end(&mut out).unwrap();
        out
    };

    println!("Input size: {}", test_data.len());
    println!("Rle compressed len: {}", compressed.len());

    assert!(test_data == decompressed);
}

#[test]
fn issue_26() {
    use deflate::write::ZlibEncoder;
    let fp = Vec::new();
    let mut fp = ZlibEncoder::new( fp, CompressionOptions::default() );

    fp.write( &[0] ).unwrap();
    fp.flush().unwrap();
    fp.write( &[0] ).unwrap();
    fp.write( &[0, 0] ).unwrap();
}

#[cfg(feature = "gzip")]
#[test]
fn issue_26_gzip() {
    use deflate::write::DeflateEncoder;
    let fp = Vec::new();
    let mut fp = DeflateEncoder::new( fp, CompressionOptions::default() );

    fp.write( &[0] ).unwrap();
    fp.flush().unwrap();
    fp.write( &[0] ).unwrap();
    fp.write( &[0, 0] ).unwrap();

}
