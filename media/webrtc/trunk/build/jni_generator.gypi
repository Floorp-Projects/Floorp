# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to generate jni bindings for Java-files in a consistent manner.
#
# To use this, create a gyp target with the following form:
#  {
#    'target_name': 'base_jni_headers',
#    'type': 'none',
#    'sources': [
#      'android/java/src/org/chromium/base/BuildInfo.java',
#      ...
#      ...
#      'android/java/src/org/chromium/base/SystemMessageHandler.java',
#    ],
#    'variables': {
#      'jni_gen_dir': 'base',
#    },
#    'includes': [ '../build/jni_generator.gypi' ],
#  },
#
# The generated file name pattern can be seen on the "outputs" section below.
# (note that RULE_INPUT_ROOT is the basename for the java file).
#
# See base/android/jni_generator/jni_generator.py for more info about the
# format of generating JNI bindings.

{
  'variables': {
    'jni_generator': '<(DEPTH)/base/android/jni_generator/jni_generator.py',
  },
  'rules': [
    {
      'rule_name': 'generate_jni_headers',
      'extension': 'java',
      'inputs': [
        '<(jni_generator)',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(jni_gen_dir)/jni/<(RULE_INPUT_ROOT)_jni.h',
      ],
      'action': [
        '<(jni_generator)',
        '--input_file',
        '<(RULE_INPUT_PATH)',
        '--output_dir',
        '<(SHARED_INTERMEDIATE_DIR)/<(jni_gen_dir)/jni',
      ],
      'message': 'Generating JNI bindings from <(RULE_INPUT_PATH)',
      'process_outputs_as_sources': 1,
    },
  ],
  # This target exports a hard dependency because it generates header
  # files.
  'hard_dependency': 1,
}
