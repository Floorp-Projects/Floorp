extern crate phf_codegen;

use std::fs::File;
use std::io::{BufWriter, Write};
use std::path::Path;
use std::process::{Command, Stdio};
use std::fmt::Write as FmtWrite;

fn main(){
    let mut cmd = std::env::var_os("RUSTC")
                  .map(|c| Command::new(c))
                  .unwrap_or(Command::new("rustc"));
    let c = cmd
            .args(&["--print=target-list"])
            .stdout(Stdio::piped())
            .spawn()
            .expect("Could not spawn rustc!");
    let targets = c.wait_with_output().expect("could not wait for rustc to exit");
    if !targets.status.success() {
        println!("rustc --print=target-list did not exit successfully");
        std::process::exit(1);
    }
    let output = Path::new(&::std::env::var_os("OUT_DIR").expect("OUT_DIR")).join("builtins.rs");
    let mut file = BufWriter::new(File::create(&output).expect("builtins.rs file"));

    let stdout = String::from_utf8_lossy(&targets.stdout);
    write!(&mut file, "static BUILTINS: phf::Map<&'static str, TargetInfo> = ").unwrap();
    let mut map = phf_codegen::Map::new();
    for line in stdout.lines() {
        let cfg = cfg_for_target(line);
        if !cfg.is_empty() {
            map.entry(line, &cfg);
        }
    }
    map.build(&mut file).unwrap();
    write!(&mut file, ";").unwrap();
}


fn cfg_for_target(target: &str) -> String {
    let mut cmd = std::env::var_os("RUSTC")
                  .map(|c| Command::new(c))
                  .unwrap_or(Command::new("rustc"));
    let c = cmd
            .args(&["--target", target, "--print=cfg"])
            .stdout(Stdio::piped())
            .spawn().and_then(|c| c.wait_with_output());

    if let Ok(o) = c {
        if o.status.success() {
            let string = String::from_utf8_lossy(&o.stdout);
            let mut switches = Vec::new();
            let mut other_keys = Vec::new();
            let (mut arch, mut os, mut env, mut endian, mut ptrw) = ("", "", "", "", "");
            for (k, v) in parse(&string) {
                match (k, v) {
                    ("target_arch", Some(v)) => arch = v,
                    ("target_os", Some(v)) => os = v,
                    ("target_env", Some(v)) => env = v,
                    ("target_endian", Some(v)) => endian = v,
                    ("target_pointer_width", Some(v)) => ptrw = v,
                    (_, None) => match k {
                        "unix" |
                        "windows" |
                        "target_thread_local" => switches.push(k),
                        _ => println!("Switch `{}` blacklisted", k),
                    },
                    (k, Some(v)) => other_keys.push((k, v)),
                }
            }
            let mut switches_fmt = String::with_capacity(1024);
            let mut other_keys_fmt = String::with_capacity(4096);
            switches_fmt.push_str("[");
            for switch in switches {
                write!(switches_fmt, "B({:?}), ", switch).expect("writes to String do not fail");
            }
            switches_fmt.push_str("]");
            other_keys_fmt.push_str("[");
            for (k, v) in other_keys {
                write!(other_keys_fmt, "(B({:?}), B({:?})), ", k, v)
                    .expect("writes to String do not fail");
            }
            other_keys_fmt.push_str("]");

            format!("TargetInfo {{ \
                        arch: B({:?}), \
                        os: B({:?}), \
                        env: B({:?}), \
                        endian: B({:?}), \
                        pointer_width: B({:?}), \
                        switches: B(&{}), \
                        other_keys: B(&{}) \
                    }}", arch, os, env, endian, ptrw, switches_fmt, other_keys_fmt)
        } else {
            println!("rustc --print=cfg --target={} did not exit successfully", target);
            String::new()
        }
    } else {
        String::new()
    }
}

fn parse(i: &str) -> Vec<(&str, Option<&str>)> {
    i.lines().map(|line| {
        let mut split = line.split('=');
        let key = split.next().expect("probably bug");
        let val = split.next().map(|v| v.trim().trim_matches('"'));
        debug_assert!(split.next().is_none());
        (key, val)
    }).collect()
}
