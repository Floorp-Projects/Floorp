extern crate pkg_config;

use std::fs::{self,File};
use std::path::Path;
use std::process::Command;

use std::io::prelude::*;

fn check_func(function_name: &str) -> bool {
    let test_file_path = format!("target/build/check_{}.rs", function_name);
    let test_file_name = Path::new(&test_file_path[..]);

    fs::create_dir_all("target/build").unwrap();

    {
        let mut test_file = File::create(test_file_name).unwrap();

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
        arg(test_file_name).
        arg("--out-dir").arg("target/build/").
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
