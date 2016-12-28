# Copyright (c) 2012 The WebRTC Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
{
  'includes': [
    '../talk/build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'relayserver',
      'type': 'executable',
      'dependencies': [
        '../talk/libjingle.gyp:libjingle',
        '../talk/libjingle.gyp:libjingle_p2p',
      ],
      'sources': [
        'examples/relayserver/relayserver_main.cc',
      ],
    },  # target relayserver
    {
      'target_name': 'stunserver',
      'type': 'executable',
      'dependencies': [
        '../talk/libjingle.gyp:libjingle',
        '../talk/libjingle.gyp:libjingle_p2p',
      ],
      'sources': [
        'examples/stunserver/stunserver_main.cc',
      ],
    },  # target stunserver
    {
      'target_name': 'turnserver',
      'type': 'executable',
      'dependencies': [
        '../talk/libjingle.gyp:libjingle',
        '../talk/libjingle.gyp:libjingle_p2p',
      ],
      'sources': [
        'examples/turnserver/turnserver_main.cc',
      ],
    },  # target turnserver
    {
      'target_name': 'peerconnection_server',
      'type': 'executable',
      'sources': [
        'examples/peerconnection/server/data_socket.cc',
        'examples/peerconnection/server/data_socket.h',
        'examples/peerconnection/server/main.cc',
        'examples/peerconnection/server/peer_channel.cc',
        'examples/peerconnection/server/peer_channel.h',
        'examples/peerconnection/server/utils.cc',
        'examples/peerconnection/server/utils.h',
      ],
      'dependencies': [
        '<(webrtc_root)/common.gyp:webrtc_common',
        '../talk/libjingle.gyp:libjingle',
      ],
      # TODO(ronghuawu): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4309, ],
    }, # target peerconnection_server
  ],
  'conditions': [
    ['OS=="linux" or OS=="win"', {
      'targets': [
        {
          'target_name': 'peerconnection_client',
          'type': 'executable',
          'sources': [
            'examples/peerconnection/client/conductor.cc',
            'examples/peerconnection/client/conductor.h',
            'examples/peerconnection/client/defaults.cc',
            'examples/peerconnection/client/defaults.h',
            'examples/peerconnection/client/peer_connection_client.cc',
            'examples/peerconnection/client/peer_connection_client.h',
          ],
          'dependencies': [
            '../talk/libjingle.gyp:libjingle_peerconnection',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:field_trial_default',
            '<@(libjingle_tests_additional_deps)',
          ],
          'conditions': [
            ['build_json==1', {
              'dependencies': [
                '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
              ],
            }],
            # TODO(ronghuawu): Move these files to a win/ directory then they
            # can be excluded automatically.
            ['OS=="win"', {
              'sources': [
                'examples/peerconnection/client/flagdefs.h',
                'examples/peerconnection/client/main.cc',
                'examples/peerconnection/client/main_wnd.cc',
                'examples/peerconnection/client/main_wnd.h',
              ],
              'msvs_settings': {
                'VCLinkerTool': {
                 'SubSystem': '2',  # Windows
                },
              },
            }],  # OS=="win"
            ['OS=="win" and clang==1', {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'AdditionalOptions': [
                    # Disable warnings failing when compiling with Clang on Windows.
                    # https://bugs.chromium.org/p/webrtc/issues/detail?id=5366
                    '-Wno-reorder',
                    '-Wno-unused-function',
                  ],
                },
              },
            }], # OS=="win" and clang==1
            ['OS=="linux"', {
              'sources': [
                'examples/peerconnection/client/linux/main.cc',
                'examples/peerconnection/client/linux/main_wnd.cc',
                'examples/peerconnection/client/linux/main_wnd.h',
              ],
              'cflags': [
                '<!@(pkg-config --cflags glib-2.0 gobject-2.0 gtk+-2.0)',
              ],
              'link_settings': {
                'ldflags': [
                  '<!@(pkg-config --libs-only-L --libs-only-other glib-2.0'
                      ' gobject-2.0 gthread-2.0 gtk+-2.0)',
                ],
                'libraries': [
                  '<!@(pkg-config --libs-only-l glib-2.0 gobject-2.0'
                      ' gthread-2.0 gtk+-2.0)',
                  '-lX11',
                  '-lXcomposite',
                  '-lXext',
                  '-lXrender',
                ],
              },
            }],  # OS=="linux"
          ],  # conditions
        },  # target peerconnection_client
      ], # targets
    }],  # OS=="linux" or OS=="win"

    ['OS=="ios" or (OS=="mac" and target_arch!="ia32")', {
      'targets': [
        {
          'target_name': 'apprtc_common',
          'type': 'static_library',
          'dependencies': [
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:field_trial_default',
            '../talk/libjingle.gyp:libjingle_peerconnection_objc',
          ],
          'sources': [
            'examples/objc/AppRTCDemo/common/ARDUtilities.h',
            'examples/objc/AppRTCDemo/common/ARDUtilities.m',
          ],
          'include_dirs': [
            'examples/objc/AppRTCDemo/common',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'examples/objc/AppRTCDemo/common',
            ],
          },
          'conditions': [
            ['OS=="mac"', {
              'xcode_settings': {
                'MACOSX_DEPLOYMENT_TARGET' : '10.8',
              },
            }],
          ],
          'link_settings': {
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                '-framework QuartzCore',
              ],
            },
          },
        },
        {
          'target_name': 'apprtc_signaling',
          'type': 'static_library',
          'dependencies': [
            'apprtc_common',
            '../talk/libjingle.gyp:libjingle_peerconnection_objc',
            'socketrocket',
          ],
          'sources': [
            'examples/objc/AppRTCDemo/ARDAppClient.h',
            'examples/objc/AppRTCDemo/ARDAppClient.m',
            'examples/objc/AppRTCDemo/ARDAppClient+Internal.h',
            'examples/objc/AppRTCDemo/ARDAppEngineClient.h',
            'examples/objc/AppRTCDemo/ARDAppEngineClient.m',
            'examples/objc/AppRTCDemo/ARDBitrateTracker.h',
            'examples/objc/AppRTCDemo/ARDBitrateTracker.m',
            'examples/objc/AppRTCDemo/ARDCEODTURNClient.h',
            'examples/objc/AppRTCDemo/ARDCEODTURNClient.m',
            'examples/objc/AppRTCDemo/ARDJoinResponse.h',
            'examples/objc/AppRTCDemo/ARDJoinResponse.m',
            'examples/objc/AppRTCDemo/ARDJoinResponse+Internal.h',
            'examples/objc/AppRTCDemo/ARDMessageResponse.h',
            'examples/objc/AppRTCDemo/ARDMessageResponse.m',
            'examples/objc/AppRTCDemo/ARDMessageResponse+Internal.h',
            'examples/objc/AppRTCDemo/ARDRoomServerClient.h',
            'examples/objc/AppRTCDemo/ARDSDPUtils.h',
            'examples/objc/AppRTCDemo/ARDSDPUtils.m',
            'examples/objc/AppRTCDemo/ARDSignalingChannel.h',
            'examples/objc/AppRTCDemo/ARDSignalingMessage.h',
            'examples/objc/AppRTCDemo/ARDSignalingMessage.m',
            'examples/objc/AppRTCDemo/ARDStatsBuilder.h',
            'examples/objc/AppRTCDemo/ARDStatsBuilder.m',
            'examples/objc/AppRTCDemo/ARDTURNClient.h',
            'examples/objc/AppRTCDemo/ARDWebSocketChannel.h',
            'examples/objc/AppRTCDemo/ARDWebSocketChannel.m',
            'examples/objc/AppRTCDemo/RTCICECandidate+JSON.h',
            'examples/objc/AppRTCDemo/RTCICECandidate+JSON.m',
            'examples/objc/AppRTCDemo/RTCICEServer+JSON.h',
            'examples/objc/AppRTCDemo/RTCICEServer+JSON.m',
            'examples/objc/AppRTCDemo/RTCMediaConstraints+JSON.h',
            'examples/objc/AppRTCDemo/RTCMediaConstraints+JSON.m',
            'examples/objc/AppRTCDemo/RTCSessionDescription+JSON.h',
            'examples/objc/AppRTCDemo/RTCSessionDescription+JSON.m',
          ],
          'include_dirs': [
            'examples/objc/AppRTCDemo',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'examples/objc/AppRTCDemo',
            ],
          },
          'export_dependent_settings': [
            '../talk/libjingle.gyp:libjingle_peerconnection_objc',
          ],
          'conditions': [
            ['OS=="mac"', {
              'xcode_settings': {
                'MACOSX_DEPLOYMENT_TARGET' : '10.8',
              },
            }],
          ],
        },
        {
          'target_name': 'AppRTCDemo',
          'type': 'executable',
          'product_name': 'AppRTCDemo',
          'mac_bundle': 1,
          'dependencies': [
            'apprtc_common',
            'apprtc_signaling',
          ],
          'conditions': [
            ['OS=="ios"', {
              'mac_bundle_resources': [
                'examples/objc/AppRTCDemo/ios/resources/iPhone5@2x.png',
                'examples/objc/AppRTCDemo/ios/resources/iPhone6@2x.png',
                'examples/objc/AppRTCDemo/ios/resources/iPhone6p@3x.png',
                'examples/objc/AppRTCDemo/ios/resources/Roboto-Regular.ttf',
                'examples/objc/AppRTCDemo/ios/resources/ic_call_end_black_24dp.png',
                'examples/objc/AppRTCDemo/ios/resources/ic_call_end_black_24dp@2x.png',
                'examples/objc/AppRTCDemo/ios/resources/ic_clear_black_24dp.png',
                'examples/objc/AppRTCDemo/ios/resources/ic_clear_black_24dp@2x.png',
                'examples/objc/AppRTCDemo/ios/resources/ic_switch_video_black_24dp.png',
                'examples/objc/AppRTCDemo/ios/resources/ic_switch_video_black_24dp@2x.png',
                'examples/objc/Icon.png',
              ],
              'sources': [
                'examples/objc/AppRTCDemo/ios/ARDAppDelegate.h',
                'examples/objc/AppRTCDemo/ios/ARDAppDelegate.m',
                'examples/objc/AppRTCDemo/ios/ARDMainView.h',
                'examples/objc/AppRTCDemo/ios/ARDMainView.m',
                'examples/objc/AppRTCDemo/ios/ARDMainViewController.h',
                'examples/objc/AppRTCDemo/ios/ARDMainViewController.m',
                'examples/objc/AppRTCDemo/ios/ARDStatsView.h',
                'examples/objc/AppRTCDemo/ios/ARDStatsView.m',
                'examples/objc/AppRTCDemo/ios/ARDVideoCallView.h',
                'examples/objc/AppRTCDemo/ios/ARDVideoCallView.m',
                'examples/objc/AppRTCDemo/ios/ARDVideoCallViewController.h',
                'examples/objc/AppRTCDemo/ios/ARDVideoCallViewController.m',
                'examples/objc/AppRTCDemo/ios/AppRTCDemo-Prefix.pch',
                'examples/objc/AppRTCDemo/ios/UIImage+ARDUtilities.h',
                'examples/objc/AppRTCDemo/ios/UIImage+ARDUtilities.m',
                'examples/objc/AppRTCDemo/ios/main.m',
              ],
              'xcode_settings': {
                'INFOPLIST_FILE': 'examples/objc/AppRTCDemo/ios/Info.plist',
              },
            }],
            ['OS=="mac"', {
              'sources': [
                'examples/objc/AppRTCDemo/mac/APPRTCAppDelegate.h',
                'examples/objc/AppRTCDemo/mac/APPRTCAppDelegate.m',
                'examples/objc/AppRTCDemo/mac/APPRTCViewController.h',
                'examples/objc/AppRTCDemo/mac/APPRTCViewController.m',
                'examples/objc/AppRTCDemo/mac/main.m',
              ],
              'xcode_settings': {
                'CLANG_WARN_OBJC_MISSING_PROPERTY_SYNTHESIS': 'NO',
                'INFOPLIST_FILE': 'examples/objc/AppRTCDemo/mac/Info.plist',
                'MACOSX_DEPLOYMENT_TARGET' : '10.8',
                'OTHER_LDFLAGS': [
                  '-framework AVFoundation',
                ],
              },
            }],
            ['target_arch=="ia32"', {
              'dependencies' : [
                '<(DEPTH)/testing/iossim/iossim.gyp:iossim#host',
              ],
            }],
          ],
        },  # target AppRTCDemo
        {
          # TODO(tkchin): move this into the real third party location and
          # have it mirrored on chrome infra.
          'target_name': 'socketrocket',
          'type': 'static_library',
          'sources': [
            'examples/objc/AppRTCDemo/third_party/SocketRocket/SRWebSocket.h',
            'examples/objc/AppRTCDemo/third_party/SocketRocket/SRWebSocket.m',
          ],
          'conditions': [
            ['OS=="mac"', {
              'xcode_settings': {
                # SocketRocket autosynthesizes some properties. Disable the
                # warning so we can compile successfully.
                'CLANG_WARN_OBJC_MISSING_PROPERTY_SYNTHESIS': 'NO',
                'MACOSX_DEPLOYMENT_TARGET' : '10.8',
                # SRWebSocket.m uses code with partial availability.
                # https://code.google.com/p/webrtc/issues/detail?id=4695
                'WARNING_CFLAGS!': ['-Wpartial-availability'],
              },
            }],
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'examples/objc/AppRTCDemo/third_party/SocketRocket',
            ],
          },
          'xcode_settings': {
            'CLANG_ENABLE_OBJC_ARC': 'YES',
            'WARNING_CFLAGS': [
              '-Wno-deprecated-declarations',
              '-Wno-nonnull',
            ],
          },
          'link_settings': {
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                '-framework CFNetwork',
                '-licucore',
              ],
            },
          }
        },  # target socketrocket
      ],  # targets
    }],  # OS=="ios" or (OS=="mac" and target_arch!="ia32")

    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'AppRTCDemo',
          'type': 'none',
          'dependencies': [
            '../talk/libjingle.gyp:libjingle_peerconnection_java',
          ],
          'variables': {
            'apk_name': 'AppRTCDemo',
            'java_in_dir': 'examples/androidapp',
            'has_java_resources': 1,
            'resource_dir': 'examples/androidapp/res',
            'R_package': 'org.appspot.apprtc',
            'R_package_relpath': 'org/appspot/apprtc',
            'input_jars_paths': [
              'examples/androidapp/third_party/autobanh/autobanh.jar',
             ],
            'library_dexed_jars_paths': [
              'examples/androidapp/third_party/autobanh/autobanh.jar',
             ],
            'native_lib_target': 'libjingle_peerconnection_so',
            'add_to_dependents_classpaths':1,
          },
          'includes': [ '../build/java_apk.gypi' ],
        },  # target AppRTCDemo

        {
          # AppRTCDemo creates a .jar as a side effect. Any java targets
          # that need that .jar in their classpath should depend on this target,
          # AppRTCDemo_apk. Dependents of AppRTCDemo_apk receive its
          # jar path in the variable 'apk_output_jar_path'.
          # This target should only be used by targets which instrument
          # AppRTCDemo_apk.
          'target_name': 'AppRTCDemo_apk',
          'type': 'none',
          'dependencies': [
             'AppRTCDemo',
           ],
           'includes': [ '../build/apk_fake_jar.gypi' ],
        },  # target AppRTCDemo_apk

        {
          'target_name': 'AppRTCDemoTest',
          'type': 'none',
          'dependencies': [
            'AppRTCDemo_apk',
           ],
          'variables': {
            'apk_name': 'AppRTCDemoTest',
            'java_in_dir': 'examples/androidtests',
            'is_test_apk': 1,
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
      ],  # targets
    }],  # OS=="android"
  ],
}
