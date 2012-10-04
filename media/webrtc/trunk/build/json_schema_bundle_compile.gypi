# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # When including this gypi, the following variables must be set:
    #   idl_schema_files: an array of idl files that comprise the api model.
    #   cc_dir: path to generated files
    #   root_namespace: the C++ namespace that all generated files go under
    # Functions and namespaces can be excluded by setting "nocompile" to true.
    'api_gen_dir': '<(DEPTH)/tools/json_schema_compiler',
    'api_gen': '<(api_gen_dir)/compiler.py',
  },
  'actions': [
    {
      'action_name': 'genapi_bundle',
      'inputs': [
        '<(api_gen_dir)/cc_generator.py',
        '<(api_gen_dir)/code.py',
        '<(api_gen_dir)/compiler.py',
        '<(api_gen_dir)/cpp_type_generator.py',
        '<(api_gen_dir)/cpp_util.py',
        '<(api_gen_dir)/h_generator.py',
        '<(api_gen_dir)/idl_schema.py',
        '<(api_gen_dir)/json_schema.py',
        '<(api_gen_dir)/model.py',
        '<(api_gen_dir)/schema_bundle_generator.py',
        '<(api_gen_dir)/util_cc_helper.py',
        '<@(idl_schema_files)',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(cc_dir)/generated_api.h',
        '<(SHARED_INTERMEDIATE_DIR)/<(cc_dir)/generated_schemas.h',
        '<(SHARED_INTERMEDIATE_DIR)/<(cc_dir)/generated_schemas.cc',
      ],
      'action': [
        'python',
        '<(api_gen)',
        '--root=<(DEPTH)',
        '--destdir=<(SHARED_INTERMEDIATE_DIR)',
        '--namespace=<(root_namespace)',
        '--bundle',
        '<@(idl_schema_files)',
      ],
      'message': 'Generating C++ API bundle code',
      'process_outputs_as_sources': 1,
    }
  ],
  'include_dirs': [
    '<(SHARED_INTERMEDIATE_DIR)',
    '<(DEPTH)',
  ],
  'direct_dependent_settings': {
    'include_dirs': [
      '<(SHARED_INTERMEDIATE_DIR)',
    ]
  },
  # This target exports a hard dependency because it generates header
  # files.
  'hard_dependency': 1,
}
