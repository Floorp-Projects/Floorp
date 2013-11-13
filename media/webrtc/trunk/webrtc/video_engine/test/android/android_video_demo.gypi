# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
{
  'targets': [
    {
      'target_name': 'libwebrtc-video-demo-jni',
      'type': 'loadable_module',
      'dependencies': [
        '<(webrtc_root)/modules/modules.gyp:*',
        '<(webrtc_root)/test/test.gyp:channel_transport',
        '<(webrtc_root)/video_engine/video_engine.gyp:video_engine_core',
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',
      ],
      'sources': [
        'jni/android_media_codec_decoder.cc',
        'jni/vie_android_java_api.cc',
      ],
      'link_settings': {
        'libraries': [
          '-llog',
          '-lGLESv2',
          '-lOpenSLES',
        ],
      }
    },
    {
      'target_name': 'WebRTCDemo',
      'type': 'none',
      'dependencies': [
        'libwebrtc-video-demo-jni',
        '<(modules_java_gyp_path):*',
      ],
      'actions': [
        {
          # TODO(yujie.mao): Convert building of the demo to a proper GYP target
          # so this action is not needed once chromium's apk-building machinery
          # can be used. (crbug.com/225101)
          'action_name': 'build_webrtcdemo_apk',
          'variables': {
            'android_webrtc_demo_root': '<(webrtc_root)/video_engine/test/android',
          },
          'inputs' : [
            '<(PRODUCT_DIR)/lib.java/audio_device_module_java.jar',
            '<(PRODUCT_DIR)/lib.java/video_capture_module_java.jar',
            '<(PRODUCT_DIR)/lib.java/video_render_module_java.jar',
            '<(PRODUCT_DIR)/libwebrtc-video-demo-jni.so',
            '<!@(find <(android_webrtc_demo_root)/src -name "*.java")',
          ],
          'outputs': ['<(PRODUCT_DIR)/WebRTCDemo-debug.apk'],
          'action': ['bash', '-ec',
                     'rm -f <(_outputs) && '
                     'mkdir -p <(android_webrtc_demo_root)/libs/<(android_app_abi) && '
                     '<(android_strip) -o <(android_webrtc_demo_root)/libs/<(android_app_abi)/libwebrtc-video-demo-jni.so <(PRODUCT_DIR)/libwebrtc-video-demo-jni.so && '
                     'cd <(android_webrtc_demo_root) && '
                     'ant debug && '
                     'cd - && '
                     'cp <(android_webrtc_demo_root)/bin/WebRTCDemo-debug.apk <(_outputs)'
          ],
        },
      ],
    },
  ],
}
