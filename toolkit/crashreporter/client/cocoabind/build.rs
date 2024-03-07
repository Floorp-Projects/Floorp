/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use mozbuild::config::CC_BASE_FLAGS as CFLAGS;

const TYPES: &[&str] = &[
    "ActionCell",
    "Application",
    "Array",
    "AttributedString",
    "Box",
    "Button",
    "ButtonCell",
    "Cell",
    "ClassDescription",
    "Control",
    "DefaultRunLoopMode",
    "Dictionary",
    "ForegroundColorAttributeName",
    "LayoutDimension",
    "LayoutGuide",
    "LayoutXAxisAnchor",
    "LayoutYAxisAnchor",
    "MutableAttributedString",
    "MutableParagraphStyle",
    "MutableString",
    "ModalPanelRunLoopMode",
    "Panel",
    "ProcessInfo",
    "ProgressIndicator",
    "Proxy",
    "RunLoop",
    "ScrollView",
    "StackView",
    "String",
    "TextField",
    "TextView",
    "Value",
    "View",
    "Window",
];

fn main() {
    let mut builder = bindgen::Builder::default()
        .header_contents(
            "cocoa_bindings.h",
            "#define self self_
            #import <Cocoa/Cocoa.h>
            ",
        )
        .generate_block(true)
        .prepend_enum_name(false)
        .clang_args(CFLAGS)
        .clang_args(["-x", "objective-c"])
        .clang_arg("-fblocks")
        .derive_default(true)
        .allowlist_item("TransformProcessType");
    for name in TYPES {
        // (I|P) covers generated traits (interfaces and protocols). `(_.*)?` covers categories
        // (which are generated as `CLASS_CATEGORY`).
        builder = builder.allowlist_item(format!("(I|P)?NS{name}(_.*)?"));
    }
    let bindings = builder
        .generate()
        .expect("unable to generate cocoa bindings");
    let out_path = std::path::PathBuf::from(std::env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("cocoa_bindings.rs"))
        .expect("failed to write cocoa bindings");
    println!("cargo:rustc-link-lib=framework=AppKit");
    println!("cargo:rustc-link-lib=framework=Cocoa");
    println!("cargo:rustc-link-lib=framework=Foundation");
}
