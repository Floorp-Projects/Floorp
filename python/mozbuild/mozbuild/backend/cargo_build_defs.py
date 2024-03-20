# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cargo_extra_outputs = {
    "bindgen": ["tests.rs", "host-target.txt"],
    "cssparser": ["tokenizer.rs"],
    "gleam": ["gl_and_gles_bindings.rs", "gl_bindings.rs", "gles_bindings.rs"],
    "khronos_api": ["webgl_exts.rs"],
    "libloading": ["libglobal_static.a", "src/os/unix/global_static.o"],
    "lmdb-sys": ["liblmdb.a", "midl.o", "mdb.o"],
    "num-integer": ["rust_out.o"],
    "num-traits": ["rust_out.o"],
    "selectors": ["ascii_case_insensitive_html_attributes.rs"],
    "style": [
        "gecko/atom_macro.rs",
        "gecko/pseudo_element_definition.rs",
        "gecko/structs.rs",
        "properties.rs",
    ],
    "webrender": ["shaders.rs"],
    "geckodriver": ["build-info.rs"],
    "gecko-profiler": ["gecko/bindings.rs"],
    "crc": ["crc64_constants.rs", "crc32_constants.rs"],
    "bzip2-sys": [
        "bzip2-1.0.6/blocksort.o",
        "bzip2-1.0.6/bzlib.o",
        "bzip2-1.0.6/compress.o",
        "bzip2-1.0.6/crctable.o",
        "bzip2-1.0.6/decompress.o",
        "bzip2-1.0.6/huffman.o",
        "bzip2-1.0.6/randtable.o",
        "libbz2.a",
    ],
    "clang-sys": ["common.rs", "dynamic.rs"],
    "target-lexicon": ["host.rs"],
    "baldrdash": ["bindings.rs"],
    "typenum": ["op.rs", "consts.rs"],
}
