# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to generate jni bindings for system Java-files in a consistent manner.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'android_jar_jni_headers',
#   'type': 'none',
#   'variables': {
#     'jni_gen_dir': 'chrome',
#     'input_java_class': 'java/io/InputStream.class',
#     'input_jar_file': '<(android_sdk)/android.jar',
#   },
#   'includes': [ '../build/jar_file_jni_generator.gypi' ],
# },

{
  'variables': {
    'jni_generator': '<(DEPTH)/base/android/jni_generator/jni_generator.py',
  },
  'actions': [
    {
      'action_name': 'generate_jni_headers_from_jar_file',
      'inputs': [
        '<(jni_generator)',
        '<(input_jar_file)',
      ],
      'variables': {
        'java_class_name': '<!(basename <(input_java_class)|sed "s/\.class//")'
      },
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(jni_gen_dir)/jni/<(java_class_name)_jni.h',
      ],
      'action': [
        '<(jni_generator)',
        '-j',
        '<(input_jar_file)',
        '--input_file',
        '<(input_java_class)',
        '--output_dir',
        '<(SHARED_INTERMEDIATE_DIR)/<(jni_gen_dir)/jni',
      ],
      'message': 'Generating JNI bindings from  <(input_jar_file)/<(input_java_class)',
      'process_outputs_as_sources': 1,
    },
  ],
  # This target exports a hard dependency because it generates header
  # files.
  'hard_dependency': 1,
}
