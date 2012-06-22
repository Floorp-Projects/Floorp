# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'includes': ['release_defaults.gypi'],
  'defines': ['OFFICIAL_BUILD'],
  'msvs_settings': {
    'VCCLCompilerTool': {
      'InlineFunctionExpansion': '2',
      'EnableIntrinsicFunctions': 'true',
      'OmitFramePointers': 'true',
      'EnableFiberSafeOptimizations': 'true',
    },
    'VCLibrarianTool': {
      'AdditionalOptions': [
        '/ltcg',
        '/expectedoutputsize:120000000'
      ],
    },
    'VCLinkerTool': {
      'AdditionalOptions': [
        '/time',
        # This may reduce memory fragmentation during linking.
        # The expected size is 40*1024*1024, which gives us about 10M of
        # headroom as of Dec 16, 2011.
        '/expectedoutputsize:41943040',
      ],
      'LinkTimeCodeGeneration': '1',
      # The /PROFILE flag causes the linker to add a "FIXUP" debug stream to
      # the generated PDB. According to MSDN documentation, this flag is only
      # available (or perhaps supported) in the Enterprise (team development)
      # version of Visual Studio. If this blocks your official build, simply
      # comment out this line, then  re-run "gclient runhooks".
      'Profile': 'true',
    },
  },
}
