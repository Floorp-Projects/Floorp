# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
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
      'target_name': 'rtc_p2p_unittest',
      'type': 'none',
      'direct_dependent_settings': {
        'sources': [
          'base/dtlstransportchannel_unittest.cc',
          'base/fakesession.h',
          'base/p2ptransportchannel_unittest.cc',
          'base/port_unittest.cc',
          'base/portallocatorsessionproxy_unittest.cc',
          'base/pseudotcp_unittest.cc',
          'base/relayport_unittest.cc',
          'base/relayserver_unittest.cc',
          'base/stun_unittest.cc',
          'base/stunport_unittest.cc',
          'base/stunrequest_unittest.cc',
          'base/stunserver_unittest.cc',
          'base/testrelayserver.h',
          'base/teststunserver.h',
          'base/testturnserver.h',
          'base/transport_unittest.cc',
          'base/transportdescriptionfactory_unittest.cc',
          'base/turnport_unittest.cc',
          'client/connectivitychecker_unittest.cc',
          'client/fakeportallocator.h',
          'client/portallocator_unittest.cc',
        ],
      },
    },
  ],
}

