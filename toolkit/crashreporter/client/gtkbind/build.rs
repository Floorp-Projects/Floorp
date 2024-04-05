/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use mozbuild::config::{
    CC_BASE_FLAGS as CFLAGS, MOZ_GTK3_CFLAGS as GTK_CFLAGS, MOZ_GTK3_LIBS as GTK_LIBS,
};

const HEADER: &str = r#"
#include "gtk/gtk.h"
#include "pango/pango.h"
#include "gdk-pixbuf/gdk-pixbuf.h"
"#;

fn main() {
    let bindings = bindgen::Builder::default()
        .header_contents("gtk_bindings.h", HEADER)
        .clang_args(CFLAGS)
        .clang_args(GTK_CFLAGS)
        .allowlist_function("gtk_.*")
        .allowlist_function(
            "g_(application|main_context|memory_input_stream|object|signal|timeout)_.*",
        )
        .allowlist_function("gdk_pixbuf_new_from_stream")
        .allowlist_function("pango_attr_.*")
        // The gtk/glib valist functions generate FFI-unsafe signatures on aarch64 which cause
        // compile errors. We don't use them anyway.
        .blocklist_function(".*_valist")
        .derive_default(true)
        .generate()
        .expect("unable to generate gtk bindings");
    for flag in GTK_LIBS {
        if let Some(lib) = flag.strip_prefix("-l") {
            println!("cargo:rustc-link-lib={lib}");
        } else if let Some(path) = flag.strip_prefix("-L") {
            println!("cargo:rustc-link-search={path}");
        }
    }
    let out_path = std::path::PathBuf::from(std::env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("gtk_bindings.rs"))
        .expect("failed to write gtk bindings");
}
