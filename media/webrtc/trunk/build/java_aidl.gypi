# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to build Java aidl files in a consistent manner.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'aidl_aidl-file-name',
#   'type': 'none',
#   'variables': {
#     'package_name': <name-of-package>
#     'aidl_interface_file': '<interface-path>/<interface-file>.aidl',
#   },
#   'sources': {
#     '<input-path1>/<input-file1>.aidl',
#     '<input-path2>/<input-file2>.aidl',
#     ...
#   },
#   'includes': ['<path-to-this-file>/java_aidl.gypi'],
# }
#
#
# The generated java files will be:
#   <(PRODUCT_DIR)/lib.java/<input-file1>.java
#   <(PRODUCT_DIR)/lib.java/<input-file2>.java
#   ...
#
# TODO(cjhopman): dependents need to rebuild when this target's inputs have changed.

{
  'direct_dependent_settings': {
    'variables': {
      'generated_src_dirs': ['<(SHARED_INTERMEDIATE_DIR)/<(package_name)/aidl/'],
    },
  },
  'rules': [
    {
      'rule_name': 'compile_aidl',
      'extension': 'aidl',
      'inputs': [
        '<(android_sdk)/framework.aidl',
        '<(aidl_interface_file)',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(package_name)/aidl/<(RULE_INPUT_ROOT).java',
      ],
      'action': [
        '<(android_sdk_tools)/aidl',
        '-p<(android_sdk)/framework.aidl',
        '-p<(aidl_interface_file)',
        '<(RULE_INPUT_PATH)',
        '<(SHARED_INTERMEDIATE_DIR)/<(package_name)/aidl/<(RULE_INPUT_ROOT).java',
      ],
    },
  ],
}
