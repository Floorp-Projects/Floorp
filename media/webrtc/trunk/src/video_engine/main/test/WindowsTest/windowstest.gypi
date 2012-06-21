# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'conditions': [
    ['OS=="win"', {
      'targets': [
        # WinTest - GUI test for Windows
        {
          'target_name': 'vie_win_test',
          'type': 'executable',
          'dependencies': [
            '<(webrtc_root)/modules/modules.gyp:video_render_module',
            '<(webrtc_root)/modules/modules.gyp:video_capture_module',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            ## VoiceEngine
            '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine_core',
            ## VideoEngine
            'video_engine_core',            
          ],
          'include_dirs': [
            './interface',            
            '../../../../', # common_types.h and typedefs.h
            '../commonTestClasses/'
          ],
          'sources': [
            'Capture.rc',
            'captureDeviceImage.jpg',
            'ChannelDlg.cc',
            'ChannelDlg.h',
            'ChannelPool.cc',
            'ChannelPool.h',            
            'renderStartImage.jpg',
            'renderTimeoutImage.jpg',
            'res\Capture.rc2',
            'resource.h',
            'StdAfx.h',
            'videosize.cc',
            'VideoSize.h',
            'WindowsTest.cc',
            'WindowsTest.h',
            'WindowsTestMainDlg.cc',
            'WindowsTestMainDlg.h',
            'WindowsTestResouce.rc',
            'WindowsTestResource.h',
            'tbExternalTransport.cc',
            'CaptureDevicePool.cc',
            'tbExternalTransport.h',
            'CaptureDevicePool.h',
            
          ],
           'configurations': {
            'Common_Base': {
              'msvs_configuration_attributes': {
                'conditions': [
                  ['component=="shared_library"', {
                    'UseOfMFC': '2',  # Shared DLL
                  },{
                    'UseOfMFC': '1',  # Static
                  }],
                ],
              },
            },
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',   # Windows
            },
          },
        },
      ],
    }],
  ],
}
# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
