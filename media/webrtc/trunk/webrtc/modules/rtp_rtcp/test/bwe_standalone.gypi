# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'bwe_standalone',
      'type': 'executable',
      'dependencies': [
        'matlab_plotting',
        'rtp_rtcp',
        'udp_transport',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
        '../../interface',
      ],
      'sources': [
        'BWEStandAlone/BWEStandAlone.cc',
        'BWEStandAlone/TestLoadGenerator.cc',
        'BWEStandAlone/TestLoadGenerator.h',
        'BWEStandAlone/TestSenderReceiver.cc',
        'BWEStandAlone/TestSenderReceiver.h',
      ], # source
      'conditions': [
          ['OS=="linux"', {
              'cflags': [
                  '-fexceptions', # enable exceptions
                  ],
              },
           ],
          ],

      'include_dirs': [
          ],
      'link_settings': {
          },
    },

    {
      'target_name': 'matlab_plotting',
      'type': 'static_library',
      'dependencies': [
        'matlab_plotting_include',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
          '/opt/matlab2010a/extern/include',
          ],
      # 'direct_dependent_settings': {
      #     'defines': [
      #         'MATLAB',
      #         ],
      #     'include_dirs': [
      #         'BWEStandAlone',
      #         ],
      #     },
      'export_dependent_settings': [
          'matlab_plotting_include',
          ],
      'sources': [
          'BWEStandAlone/MatlabPlot.cc',
          'BWEStandAlone/MatlabPlot.h',
          ],
      'link_settings': {
          'ldflags' : [
              '-L/opt/matlab2010a/bin/glnxa64',
              '-leng',
              '-lmx',
              '-Wl,-rpath,/opt/matlab2010a/bin/glnxa64',
              ],
          },
      'defines': [
          'MATLAB',
          ],
      'conditions': [
          ['OS=="linux"', {
              'cflags': [
                  '-fexceptions', # enable exceptions
                  ],
              },
           ],
          ],
      },

    {
      'target_name': 'matlab_plotting_include',
      'type': 'none',
      'direct_dependent_settings': {
          'defines': [
#              'MATLAB',
              ],
          'include_dirs': [
              'BWEStandAlone',
              ],
          },
      },
  ],
}
