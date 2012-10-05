# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # When including this gypi, the following variables must be set:
    #   json_schema_files: a list of json files that comprise the api model.
    #   idl_schema_files: a list of IDL files that comprise the api model.
    #   cc_dir: path to generated files
    #   root_namespace: the C++ namespace that all generated files go under
    # Functions and namespaces can be excluded by setting "nocompile" to true.
    'api_gen_dir': '<(DEPTH)/tools/json_schema_compiler',
    'api_gen': '<(api_gen_dir)/compiler.py',
  },
  'rules': [
    {
      'rule_name': 'genapi',
      'extension': 'json',
      'inputs': [
        '<(api_gen_dir)/any.cc',
        '<(api_gen_dir)/any.h',
        '<(api_gen_dir)/any_helper.py',
        '<(api_gen_dir)/cc_generator.py',
        '<(api_gen_dir)/code.py',
        '<(api_gen_dir)/compiler.py',
        '<(api_gen_dir)/cpp_type_generator.py',
        '<(api_gen_dir)/cpp_util.py',
        '<(api_gen_dir)/h_generator.py',
        '<(api_gen_dir)/json_schema.py',
        '<(api_gen_dir)/model.py',
        '<(api_gen_dir)/util.cc',
        '<(api_gen_dir)/util.h',
        '<(api_gen_dir)/util_cc_helper.py',
        # TODO(calamity): uncomment this when gyp on windows behaves like other
        # platforms. List expansions of filepaths in inputs expand to different
        # things.
        # '<@(json_schema_files)',
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
      ],
      'message': 'Generating C++ code from <(RULE_INPUT_PATH) json files',
      'process_outputs_as_sources': 1,
    },
    {
      'rule_name': 'genapi_idl',
      'msvs_external_rule': 1,
      'extension': 'idl',
      'inputs': [
        '<(api_gen_dir)/any.cc',
        '<(api_gen_dir)/any.h',
        '<(api_gen_dir)/any_helper.py',
        '<(api_gen_dir)/cc_generator.py',
        '<(api_gen_dir)/code.py',
        '<(api_gen_dir)/compiler.py',
        '<(api_gen_dir)/cpp_type_generator.py',
        '<(api_gen_dir)/cpp_util.py',
        '<(api_gen_dir)/h_generator.py',
        '<(api_gen_dir)/idl_schema.py',
        '<(api_gen_dir)/model.py',
        '<(api_gen_dir)/util.cc',
        '<(api_gen_dir)/util.h',
        '<(api_gen_dir)/util_cc_helper.py',
        # TODO(calamity): uncomment this when gyp on windows behaves like other
        # platforms. List expansions of filepaths in inputs expand to different
        # things.
        # '<@(idl_schema_files)',
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
      ],
      'message': 'Generating C++ code from <(RULE_INPUT_PATH) IDL files',
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
