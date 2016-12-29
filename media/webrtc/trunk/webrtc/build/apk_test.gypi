# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This is almost an identical copy of src/build/apk_test.gypi with minor
# modifications to allow test executables starting with "lib".
# See http://crbug.com/543820 for more details.

{
  'dependencies': [
    '<(DEPTH)/base/base.gyp:base_java',
    '<(DEPTH)/build/android/pylib/device/commands/commands.gyp:chromium_commands',
    '<(DEPTH)/build/android/pylib/remote/device/dummy/dummy.gyp:remote_device_dummy_apk',
    '<(DEPTH)/testing/android/appurify_support.gyp:appurify_support_java',
    '<(DEPTH)/testing/android/on_device_instrumentation.gyp:reporter_java',
    '<(DEPTH)/tools/android/android_tools.gyp:android_tools',
  ],
  'conditions': [
     ['OS == "android"', {
       'variables': {
         # These are used to configure java_apk.gypi included below.
         'test_type': 'gtest',
         'apk_name': '<(test_suite_name)',
         'intermediate_dir': '<(PRODUCT_DIR)/<(test_suite_name)_apk',
         'final_apk_path': '<(intermediate_dir)/<(test_suite_name)-debug.apk',
         'java_in_dir': '<(DEPTH)/testing/android/native_test/java',
         'native_lib_target': '<(test_suite_name)',
         'gyp_managed_install': 0,
       },
       'includes': [
         '../../build/java_apk.gypi',
         '../../build/android/test_runner.gypi',
       ],
     }],  # 'OS == "android"
  ],  # conditions
}
