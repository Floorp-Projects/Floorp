# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # When including this gypi, the following variables must be set:
    #   json_schema_files: an array of json files that comprise the api model.
    #   cc_dir: path to generated files
    #   root_namespace: the C++ namespace that all generated files go under
    'api_gen_dir': '<(DEPTH)/tools/json_schema_compiler',
    'api_gen': '<(api_gen_dir)/compiler.py',
  },
  'rules': [
    {
      'rule_name': 'genapi',
      'extension': 'json',
      'inputs': [
        '<(api_gen_dir)/code.py',
        '<(api_gen_dir)/compiler.py',
        '<(api_gen_dir)/model.py',
        '<(api_gen_dir)/cc_generator.py',
        '<(api_gen_dir)/h_generator.py',
        '<(api_gen_dir)/cpp_type_generator.py',
        '<@(json_schema_files)',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(cc_dir)/<(RULE_INPUT_ROOT).cc',
        '<(SHARED_INTERMEDIATE_DIR)/<(cc_dir)/<(RULE_INPUT_ROOT).h',
      ],
      'action': [
        'python',
        '<(api_gen)',
        '<(RULE_INPUT_PATH)',
        '--root=<(DEPTH)',
        '--destdir=<(SHARED_INTERMEDIATE_DIR)',
        '--namespace=<(root_namespace)',
        '<@(json_schema_files)',
      ],
      'message': 'Generating C++ code from <(RULE_INPUT_PATH) jsons',
      'process_outputs_as_sources': 1,
    },
  ],
  'include_dirs': [
    '<(SHARED_INTERMEDIATE_DIR)',
    '<(DEPTH)',
  ],
  'dependencies':[
    '<(DEPTH)/tools/json_schema_compiler/api_gen_util.gyp:api_gen_util',
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
