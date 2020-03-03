extern crate pkg_config;

use std::env;
use std::fs::File;
use std::path::Path;
use std::process::Command;

use std::io::prelude::*;

fn check_func(function_name: &str) -> bool {
    let out_dir = env::var_os("OUT_DIR").unwrap();
    let test_file_name = Path::new(&out_dir).join(format!("check_{}.rs", function_name));

    {
        let mut test_file = File::create(&test_file_name).unwrap();

        writeln!(&mut test_file, "extern \"C\" {{").unwrap();
        writeln!(&mut test_file, "    fn {}();", function_name).unwrap();
        writeln!(&mut test_file, "}}").unwrap();
        writeln!(&mut test_file, "").unwrap();
        writeln!(&mut test_file, "fn main() {{").unwrap();
        writeln!(&mut test_file, "    unsafe {{").unwrap();
        writeln!(&mut test_file, "        {}();", function_name).unwrap();
        writeln!(&mut test_file, "    }}").unwrap();
        writeln!(&mut test_file, "}}").unwrap();
    }

    let output = Command::new("rustc").
        arg(&test_file_name).
        arg("--out-dir").arg(&out_dir).
        arg("-l").arg("udev").
        output().unwrap();

    output.status.success()
}

fn main() {
    pkg_config::find_library("libudev").unwrap();

    if check_func("udev_hwdb_new") {
        println!("cargo:rustc-cfg=hwdb");
        println!("cargo:hwdb=true");
    }
    else {
        println!("cargo:hwdb=false");
    }
}
