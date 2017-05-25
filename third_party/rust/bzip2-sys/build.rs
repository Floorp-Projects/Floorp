extern crate gcc;

use std::env;

fn main() {
    let mut cfg = gcc::Config::new();

    if env::var("TARGET").unwrap().contains("windows") {
        cfg.define("_WIN32", None);
        cfg.define("BZ_EXPORT", None);
    }

    cfg.include("bzip2-1.0.6")
       .define("BZ_NO_STDIO", None)
       .file("bzip2-1.0.6/blocksort.c")
       .file("bzip2-1.0.6/huffman.c")
       .file("bzip2-1.0.6/crctable.c")
       .file("bzip2-1.0.6/randtable.c")
       .file("bzip2-1.0.6/compress.c")
       .file("bzip2-1.0.6/decompress.c")
       .file("bzip2-1.0.6/bzlib.c")
       .compile("libbz2.a");
}
