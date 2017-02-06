//! metadeps lets you write `pkg-config` dependencies in `Cargo.toml` metadata,
//! rather than programmatically in `build.rs`.  This makes those dependencies
//! declarative, so other tools can read them as well.
//!
//! metadeps parses metadata like this in `Cargo.toml`:
//!
//! ```toml
//! [package.metadata.pkg-config]
//! testlib = "1.2"
//! testdata = { version = "4.5", feature = "some-feature" }
//! ```

#![deny(missing_docs, warnings)]

#[macro_use]
extern crate error_chain;
extern crate pkg_config;
extern crate toml;

use std::collections::HashMap;
use std::env;
use std::fs;
use std::io::Read;
use std::path::PathBuf;
use pkg_config::{Config, Library};

error_chain! {
    foreign_links {
        PkgConfig(pkg_config::Error) #[doc="pkg-config error"];
    }
}

/// Probe all libraries configured in the Cargo.toml
/// `[package.metadata.pkg-config]` section.
pub fn probe() -> Result<HashMap<String, Library>> {
    let dir = try!(env::var_os("CARGO_MANIFEST_DIR").ok_or("$CARGO_MANIFEST_DIR not set"));
    let mut path = PathBuf::from(dir);
    path.push("Cargo.toml");
    let mut manifest = try!(fs::File::open(&path).chain_err(||
        format!("Error opening {}", path.display())
    ));
    let mut manifest_str = String::new();
    try!(manifest.read_to_string(&mut manifest_str).chain_err(||
        format!("Error reading {}", path.display())
    ));
    let toml = try!(manifest_str.parse::<toml::Value>().map_err(|e|
        format!("Error parsing TOML from {}: {:?}", path.display(), e)
    ));
    let key = "package.metadata.pkg-config";
    let meta = try!(toml.lookup(key).ok_or(
        format!("No {} in {}", key, path.display())
    ));
    let table = try!(meta.as_table().ok_or(
        format!("{} not a table in {}", key, path.display())
    ));
    let mut libraries = HashMap::new();
    for (name, value) in table {
        let ref version = match value {
            &toml::Value::String(ref s) => s,
            &toml::Value::Table(ref t) => {
                let mut feature = None;
                let mut version = None;
                for (tname, tvalue) in t {
                    match (tname.as_str(), tvalue) {
                        ("feature", &toml::Value::String(ref s)) => { feature = Some(s); }
                        ("version", &toml::Value::String(ref s)) => { version = Some(s); }
                        _ => bail!("Unexpected key {}.{}.{} type {}", key, name, tname, tvalue.type_str()),
                    }
                }
                if let Some(feature) = feature {
                    let var = format!("CARGO_FEATURE_{}", feature.to_uppercase().replace('-', "_"));
                    if env::var_os(var).is_none() {
                        continue;
                    }
                }
                try!(version.ok_or(format!("No version in {}.{}", key, name)))
            }
            _ => bail!("{}.{} not a string or table", key, name),
        };
        let library = try!(Config::new().atleast_version(&version).probe(name));
        libraries.insert(name.clone(), library);
    }
    Ok(libraries)
}
