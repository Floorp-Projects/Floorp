// build.rs

use std::env;
use std::fs::File;
use std::io::Write;
use std::path::Path;

fn main() {
    let out_dir = env::var("OUT_DIR").unwrap();
    let dest_path = Path::new(&out_dir).join("generate_bytes_and_bits.rs");
    let mut f = File::create(&dest_path).unwrap();

    let up_to_bytes = 32;

    // bytes
    for i in 1..(up_to_bytes + 1) {

        let b = format!("bytes_type!(Bytes{}, {});\r\n", i, i);
        f.write_all(b.as_bytes()).unwrap();
    }

    // bits
    for i in 1..(up_to_bytes * 8) {
        let b = format!("bits_type!(Bits{}, {}, Bytes{}, {});\r\n", i, i, (i as f32 / 8.0).ceil() as usize, if (i % 8) == 0 {
            "BitsFullBytes"
        } else {
            "BitsPartialBytes"
        });
        f.write_all(b.as_bytes()).unwrap();
    }
}