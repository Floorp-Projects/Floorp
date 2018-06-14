# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

cargo_extra_outputs = {
    'bindgen': [
        'tests.rs',
        'host-target.txt',
    ],
    'cssparser': [
        'tokenizer.rs',
    ],
    'gleam': [
        'gl_and_gles_bindings.rs',
        'gl_bindings.rs',
        'gles_bindings.rs',
    ],
    'khronos_api': [
        'webgl_exts.rs',
    ],
    'libloading': [
        'libglobal_static.a',
        'src/os/unix/global_static.o',
    ],
    'selectors': [
        'ascii_case_insensitive_html_attributes.rs',
    ],
    'style': [
        'gecko/atom_macro.rs',
        'gecko/pseudo_element_definition.rs',
        'gecko_properties.rs',
        'properties.rs',
        'gecko/bindings.rs',
        'gecko/structs.rs',
    ],
    'webrender': [
        'shaders.rs',
    ],
}

cargo_extra_flags = {
    'style': [
        '-l', 'static=global_static',
        '-L', 'native=%(libloading_outdir)s',
    ]
}
