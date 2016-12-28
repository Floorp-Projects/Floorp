{
  'variables': {
    'variables': {
      'webrtc_root%': '<(DEPTH)/webrtc',
      # Override the default (10.6) in Chromium's build/common.gypi.
      # Needed for ARC and libc++.
      'mac_deployment_target%': '10.7',
      # Disable use of sysroot for Linux. It's enabled by default in Chromium,
      # but it currently lacks the libudev-dev package.
      # TODO(kjellander): Remove when crbug.com/561584 is fixed.
      'use_sysroot': 0,
    },
    'webrtc_root%': '<(webrtc_root)',
    'mac_deployment_target%': '<(mac_deployment_target)',
    'use_sysroot%': '<(use_sysroot)',
    'build_with_chromium': 0,
    'conditions': [
      ['OS=="ios"', {
        # Default to using BoringSSL on iOS.
        'use_openssl%': 1,

        # Set target_subarch for if not already set. This is needed because the
        # Chromium iOS toolchain relies on target_subarch being set.
        'conditions': [
          ['target_arch=="arm" or target_arch=="ia32"', {
            'target_subarch%': 'arm32',
          }],
          ['target_arch=="arm64" or target_arch=="x64"', {
            'target_subarch%': 'arm64',
          }],
        ],
      }],
    ],
  },
  'target_defaults': {
    'target_conditions': [
      ['_target_name=="sanitizer_options"', {
        'conditions': [
          ['lsan==1', {
            # Replace Chromium's LSan suppressions with our own for WebRTC.
            'sources/': [
              ['exclude', 'lsan_suppressions.cc'],
            ],
            'sources': [
              '<(webrtc_root)/build/sanitizers/lsan_suppressions_webrtc.cc',
            ],
          }],
          ['tsan==1', {
            # Replace Chromium's TSan v2 suppressions with our own for WebRTC.
            'sources/': [
              ['exclude', 'tsan_suppressions.cc'],
            ],
            'sources': [
              '<(webrtc_root)/build/sanitizers/tsan_suppressions_webrtc.cc',
            ],
          }],
        ],
      }],
    ],
  },
}
