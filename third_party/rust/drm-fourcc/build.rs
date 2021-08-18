#[cfg(not(feature = "build_bindings"))]
fn main() {
    println!("cargo:rerun-if-changed=build.rs"); // never rerun
}

#[cfg(feature = "build_bindings")]
fn main() {
    println!("cargo:rerun-if-changed=build.rs"); // avoids double-build when we output into src
    generate::generate().unwrap();
}

#[cfg(feature = "build_bindings")]
mod generate {
    use std::error::Error;
    use std::io::Write;
    use std::process::{Command, Stdio};

    use regex::Regex;
    use std::env;
    use std::fs::File;
    use std::path::Path;

    static CONST_PREFIX: &'static str = "DRM_FOURCC_";

    pub fn generate() -> Result<(), Box<dyn Error + Sync + Send>> {
        let out_dir = env::var("OUT_DIR").unwrap();
        let wrapper_path = Path::new(&out_dir).join("wrapper.h");

        // First get all the macros in drm_fourcc.h

        let mut cmd = Command::new("clang")
            .arg("-E") // run pre-processor only
            .arg("-dM") // output all macros defined
            .arg("-") // take input from stdin
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .spawn()?;

        {
            let stdin = cmd.stdin.as_mut().expect("failed to open stdin");
            stdin.write_all(b"#include <drm/drm_fourcc.h>\n")?;
        }

        let result = cmd.wait_with_output()?;
        let stdout = String::from_utf8(result.stdout)?;
        if !result.status.success() {
            panic!("Clang failed with output: {}", stdout)
        }

        // Then get the names of the format macros

        let fmt_re = Regex::new(r"^\s*#define (?P<full>DRM_FORMAT_(?P<short>[A-Z0-9_]+)) ")?;
        let format_names: Vec<(&str, &str)> = stdout
            .lines()
            .filter_map(|line| {
                if line.contains("DRM_FORMAT_RESERVED") || line.contains("INVALID") || line.contains("_MOD_") {
                    return None;
                }

                fmt_re.captures(line).map(|caps| {
                    let full = caps.name("full").unwrap().as_str();
                    let short = caps.name("short").unwrap().as_str();

                    (full, short)
                })
            })
            .collect();

        let vendor_re = Regex::new(r"^\s*#define (?P<full>DRM_FORMAT_MOD_VENDOR_(?P<short>[A-Z0-9_]+)) ")?;
        let vendor_names: Vec<(&str, &str)> = stdout
            .lines()
            .filter_map(|line| {
                if line.contains("DRM_FORMAT_MOD_VENDOR_NONE") {
                    return None;
                }

                vendor_re.captures(line).map(|caps| {
                    let full = caps.name("full").unwrap().as_str();
                    let short = caps.name("short").unwrap().as_str();

                    (full, short)
                })
            })
            .collect();

        let mod_re = Regex::new(r"^\s*#define (?P<full>(DRM|I915)_FORMAT_MOD_(?P<short>[A-Z0-9_]+)) ")?;
        let modifier_names: Vec<(&str, String)> = stdout
            .lines()
            .filter_map(|line| {
                if line.contains("DRM_FORMAT_MOD_NONE")
                    || line.contains("DRM_FORMAT_MOD_RESERVED")
                    || line.contains("VENDOR")
                    || line.contains("ARM_TYPE") // grrr..
                {
                    return None;
                }

                mod_re.captures(line).map(|caps| {
                    let full = caps.name("full").unwrap().as_str();
                    let short = caps.name("short").unwrap().as_str();

                    (full, if full.contains("I915") {
                        format!("I915_{}", short)
                    } else {
                        String::from(short)
                    })
                })
            })
            .collect();

        // Then create a file with a variable defined for every format macro

        let mut wrapper = File::create(&wrapper_path)?;

        wrapper.write_all(b"#include <stdint.h>\n")?;
        wrapper.write_all(b"#include <drm/drm_fourcc.h>\n")?;

        for (full, short) in &format_names {
            writeln!(wrapper, "uint32_t {}{} = {};\n", CONST_PREFIX, short, full)?;
        }
        for (full, short) in &vendor_names {
            writeln!(wrapper, "uint8_t {}{} = {};\n", CONST_PREFIX, short, full)?;
        }
        for (full, short) in &modifier_names {
            writeln!(wrapper, "uint64_t {}{} = {};\n", CONST_PREFIX, short, full)?;
        }

        wrapper.flush()?;

        // Then generate bindings from that file
        bindgen::builder()
            .header(wrapper_path.as_os_str().to_str().unwrap())
            .whitelist_var("DRM_FOURCC_.*")
            .generate()
            .unwrap()
            .write_to_file("src/consts.rs")?;

        // Then generate our enums
        fn write_enum(as_enum: &mut File, name: &str, repr: &str, names: Vec<(&str, &str)>) -> Result<(), std::io::Error> {
            as_enum.write_all(b"#[derive(Copy, Clone, Eq, PartialEq, Hash)]")?;
            as_enum.write_all(
                b"#[cfg_attr(feature = \"serde\", derive(serde::Serialize, serde::Deserialize))]",
            )?;
            writeln!(as_enum, "#[repr({})]", repr)?;
            writeln!(as_enum, "pub enum {} {{", name)?;

            let members: Vec<(String, String)> = names
                .iter()
                .map(|(_, short)| {
                    (
                        enum_member_case(short),
                        format!("consts::{}{}", CONST_PREFIX, short),
                    )
                })
                .collect();

            for (member, value) in &members {
                writeln!(as_enum, "{} = {},", member, value)?;
            }

            as_enum.write_all(b"}\n")?;

            writeln!(as_enum, "impl {} {{", name)?;
            writeln!(as_enum, "pub(crate) fn from_{}(n: {}) -> Option<Self> {{\n", repr, repr)?;
            as_enum.write_all(b"match n {\n")?;

            for (member, value) in &members {
                writeln!(as_enum, "{} => Some(Self::{}),", value, member)?;
            }

            writeln!(as_enum, "_ => None")?;
            as_enum.write_all(b"}}}\n")?;

            Ok(())
        }

        let as_enum_path = "src/as_enum.rs";
        {
            let mut as_enum = File::create(as_enum_path)?;

            as_enum.write_all(b"// Automatically generated by build.rs\n")?;
            as_enum.write_all(b"use crate::consts;")?;

            write_enum(&mut as_enum, "DrmFourcc", "u32", format_names)?;

            as_enum.write_all(b"#[derive(Debug)]")?;
            write_enum(&mut as_enum, "DrmVendor", "u8", vendor_names)?;

            // modifiers can overlap

            as_enum.write_all(b"#[derive(Debug, Copy, Clone)]")?;
            as_enum.write_all(
                b"#[cfg_attr(feature = \"serde\", derive(serde::Serialize, serde::Deserialize))]",
            )?;
            as_enum.write_all(b"pub enum DrmModifier {\n")?;

            let modifier_members: Vec<(String, String)> = modifier_names
                .iter()
                .map(|(_, short)| {
                    (
                        enum_member_case(short),
                        format!("consts::{}{}", CONST_PREFIX, short),
                    )
                })
                .collect();
            for (member, _) in &modifier_members {
                writeln!(as_enum, "{},", member)?;
            }
            as_enum.write_all(b"Unrecognized(u64)")?;

            as_enum.write_all(b"}\n")?;

            as_enum.write_all(b"impl DrmModifier {\n")?;
            as_enum.write_all(b"pub(crate) fn from_u64(n: u64) -> Self {\n")?;
            as_enum.write_all(b"#[allow(unreachable_patterns)]\n")?;
            as_enum.write_all(b"match n {\n")?;

            for (member, value) in &modifier_members {
                writeln!(as_enum, "{} => Self::{},", value, member)?;
            }
            as_enum.write_all(b"x => Self::Unrecognized(x)\n")?;
            
            as_enum.write_all(b"}}\n")?;
            as_enum.write_all(b"pub(crate) fn into_u64(&self) -> u64 {\n")?;
            as_enum.write_all(b"match self {\n")?;

            for (member, value) in &modifier_members {
                writeln!(as_enum, "Self::{} => {},", member, value)?;
            }
            as_enum.write_all(b"Self::Unrecognized(x) => *x,\n")?;

            as_enum.write_all(b"}}}\n")?;
        }

        Command::new("rustfmt").arg(as_enum_path).spawn()?.wait()?;

        Ok(())
    }

    fn enum_member_case(s: &str) -> String {
        let (first, rest) = s.split_at(1);
        format!("{}{}", first, rest.to_ascii_lowercase())
    }
}
