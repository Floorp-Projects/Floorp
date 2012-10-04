# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to build Java in a consistent manner.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my-package_java',
#   'type': 'none',
#   'variables': {
#     'package_name': 'my-package',
#     'java_in_dir': 'path/to/package/root',
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#
# Note that this assumes that there's ant buildfile with package_name in
# java_in_dir. So if if you have package_name="base" and
# java_in_dir="base/android/java" you should have a directory structure like:
#
# base/android/java/base.xml
# base/android/java/org/chromium/base/Foo.java
# base/android/java/org/chromium/base/Bar.java
#
# Finally, the generated jar-file will be:
#   <(PRODUCT_DIR)/lib.java/chromium_base.jar

{
  'direct_dependent_settings': {
    'variables': {
      'input_jars_paths': ['<(PRODUCT_DIR)/lib.java/chromium_<(package_name).jar'],
    },
  },
  'variables': {
    'input_jars_paths': [],
    'additional_src_dirs': [],
  },
  'actions': [
    {
      'action_name': 'ant_<(package_name)',
      'message': 'Building <(package_name) java sources.',
      'inputs': [
        'android/ant/common.xml',
        'android/ant/chromium-jars.xml',
        '<!@(find <(java_in_dir) -name "*.java")',
        '>@(input_jars_paths)',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/lib.java/chromium_<(package_name).jar',
      ],
      'action': [
        'ant',
        '-DPRODUCT_DIR=<(ant_build_out)',
        '-DPACKAGE_NAME=<(package_name)',
        '-DINPUT_JARS_PATHS=>(input_jars_paths)',
        '-DADDITIONAL_SRC_DIRS=>(additional_src_dirs)',
        '-DANDROID_SDK=<(android_sdk)',
        '-DANDROID_SDK_ROOT=<(android_sdk_root)',
        '-DANDROID_SDK_TOOLS=<(android_sdk_tools)',
        '-DANDROID_SDK_VERSION=<(android_sdk_version)',
        '-DANDROID_TOOLCHAIN=<(android_toolchain)',
        '-Dbasedir=<(java_in_dir)',
        '-buildfile',
        '<(DEPTH)/build/android/ant/chromium-jars.xml'
      ]
    },
  ],
}
