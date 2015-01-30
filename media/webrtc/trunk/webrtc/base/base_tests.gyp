# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
{
  'includes': [ '../build/common.gypi', ],
  'targets': [
    {
      'target_name': 'rtc_base_tests_utils',
      'type': 'static_library',
      'sources': [
        'unittest_main.cc',
        # Also use this as a convenient dumping ground for misc files that are
        # included by multiple targets below.
        'fakecpumonitor.h',
        'fakenetwork.h',
        'fakesslidentity.h',
        'faketaskrunner.h',
        'gunit.h',
        'testbase64.h',
        'testechoserver.h',
        'testutils.h',
        'win32toolhelp.h',
      ],
      'defines': [
        'GTEST_RELATIVE_PATH',
      ],
      'dependencies': [
        'base.gyp:rtc_base',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'direct_dependent_settings': {
        'defines': [
          'GTEST_RELATIVE_PATH',
        ],
      },
      'export_dependent_settings': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'rtc_base_tests',
      'type': 'none',
      'direct_dependent_settings': {
        'sources': [
          'asynchttprequest_unittest.cc',
          'atomicops_unittest.cc',
          'autodetectproxy_unittest.cc',
          'bandwidthsmoother_unittest.cc',
          'base64_unittest.cc',
          'basictypes_unittest.cc',
          'bind_unittest.cc',
          'buffer_unittest.cc',
          'bytebuffer_unittest.cc',
          'byteorder_unittest.cc',
          'callback_unittest.cc',
          'cpumonitor_unittest.cc',
          'crc32_unittest.cc',
          'criticalsection_unittest.cc',
          'event_unittest.cc',
          'exp_filter_unittest.cc',
          'filelock_unittest.cc',
          'fileutils_unittest.cc',
          'helpers_unittest.cc',
          'httpbase_unittest.cc',
          'httpcommon_unittest.cc',
          'httpserver_unittest.cc',
          'ipaddress_unittest.cc',
          'logging_unittest.cc',
          'md5digest_unittest.cc',
          'messagedigest_unittest.cc',
          #'messagequeue_unittest.cc',
          'multipart_unittest.cc',
          'nat_unittest.cc',
          'network_unittest.cc',
          'nullsocketserver_unittest.cc',
          'optionsfile_unittest.cc',
          'pathutils_unittest.cc',
          'physicalsocketserver_unittest.cc',
          'profiler_unittest.cc',
          'proxy_unittest.cc',
          'proxydetect_unittest.cc',
          'ratelimiter_unittest.cc',
          'ratetracker_unittest.cc',
          'referencecountedsingletonfactory_unittest.cc',
          'rollingaccumulator_unittest.cc',
          'scopedptrcollection_unittest.cc',
          'sha1digest_unittest.cc',
          'sharedexclusivelock_unittest.cc',
          'signalthread_unittest.cc',
          'sigslot_unittest.cc',
          'sigslottester.h',
          'sigslottester.h.pump',
          'socket_unittest.cc',
          'socket_unittest.h',
          'socketaddress_unittest.cc',
          'stream_unittest.cc',
          'stringencode_unittest.cc',
          'stringutils_unittest.cc',
          # TODO(ronghuawu): Reenable this test.
          # 'systeminfo_unittest.cc',
          'task_unittest.cc',
          'testclient_unittest.cc',
          'thread_checker_unittest.cc',
          'thread_unittest.cc',
          'timeutils_unittest.cc',
          'urlencode_unittest.cc',
          'versionparsing_unittest.cc',
          'virtualsocket_unittest.cc',
          # TODO(ronghuawu): Reenable this test.
          # 'windowpicker_unittest.cc',
        ],
        'conditions': [
          ['OS=="linux"', {
            'sources': [
              'latebindingsymboltable_unittest.cc',
              # TODO(ronghuawu): Reenable this test.
              # 'linux_unittest.cc',
              'linuxfdwalk_unittest.cc',
            ],
          }],
          ['OS=="win"', {
            'sources': [
              'win32_unittest.cc',
              'win32regkey_unittest.cc',
              'win32socketserver_unittest.cc',
              'win32toolhelp_unittest.cc',
              'win32window_unittest.cc',
              'win32windowpicker_unittest.cc',
              'winfirewall_unittest.cc',
            ],
            'sources!': [
              # TODO(ronghuawu): Fix TestUdpReadyToSendIPv6 on windows bot
              # then reenable these tests.
              'physicalsocketserver_unittest.cc',
              'socket_unittest.cc',
              'win32socketserver_unittest.cc',
              'win32windowpicker_unittest.cc',
            ],
          }],
          ['OS=="mac"', {
            'sources': [
              'macsocketserver_unittest.cc',
              'macutils_unittest.cc',
            ],
          }],
          ['os_posix==1', {
            'sources': [
              'ssladapter_unittest.cc',
              'sslidentity_unittest.cc',
              'sslstreamadapter_unittest.cc',
            ],
          }],
          ['OS=="ios" or (OS=="mac" and target_arch!="ia32")', {
            'defines': [
              'CARBON_DEPRECATED=YES',
            ],
          }],
        ],  # conditions
      },
    },
  ],
}
