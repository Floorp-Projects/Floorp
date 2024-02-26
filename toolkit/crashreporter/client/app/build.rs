/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{env, path::Path};

fn main() {
    windows_manifest();
    crash_ping_annotations();
    set_mock_cfg();
}

fn windows_manifest() {
    let target = env::var("TARGET").unwrap();
    if !(target.ends_with("windows-msvc") || target.ends_with("windows-gnu")) {
        return;
    }

    let manifest = Path::new("crashreporter.exe.manifest")
        .canonicalize()
        .unwrap();
    println!("cargo:rustc-link-arg-bins=/MANIFEST:EMBED");
    println!(
        "cargo:rustc-link-arg-bins=/MANIFESTINPUT:{}",
        manifest.display()
    );
    println!("cargo:rerun-if-changed={}", manifest.display());
}

/// Generate crash ping annotation information from the yaml definition file.
fn crash_ping_annotations() {
    use std::fs::File;
    use std::io::{BufWriter, Write};
    use yaml_rust::{Yaml, YamlLoader};

    let crash_annotations = Path::new("../../CrashAnnotations.yaml")
        .canonicalize()
        .unwrap();
    println!("cargo:rerun-if-changed={}", crash_annotations.display());

    let crash_ping_file = Path::new(&env::var("OUT_DIR").unwrap()).join("ping_annotations.rs");

    let yaml = std::fs::read_to_string(crash_annotations).unwrap();
    let Yaml::Hash(entries) = YamlLoader::load_from_str(&yaml)
        .unwrap()
        .into_iter()
        .next()
        .unwrap()
    else {
        panic!("unexpected crash annotations root type");
    };

    let ping_annotations = entries.into_iter().filter_map(|(k, v)| {
        v["ping"]
            .as_bool()
            .unwrap_or(false)
            .then(|| k.into_string().unwrap())
    });

    let mut phf_set = phf_codegen::Set::new();
    for annotation in ping_annotations {
        phf_set.entry(annotation);
    }

    let mut file = BufWriter::new(File::create(&crash_ping_file).unwrap());
    writeln!(
        &mut file,
        "static PING_ANNOTATIONS: phf::Set<&'static str> = {};",
        phf_set.build()
    )
    .unwrap();
}

/// Set the mock configuration option when tests are enabled or when the mock feature is enabled.
fn set_mock_cfg() {
    // Very inconveniently, there's no way to detect `cfg(test)` from build scripts. See
    // https://github.com/rust-lang/cargo/issues/4789. This seems like an arbitrary and pointless
    // limitation, and only complicates the evaluation of mock behavior. Because of this, we have a
    // `mock` feature which is activated by `toolkit/library/rust/moz.build`.
    if env::var_os("CARGO_FEATURE_MOCK").is_some() || mozbuild::config::MOZ_CRASHREPORTER_MOCK {
        println!("cargo:rustc-cfg=mock");
    }
}
