{
  'variables': {
    'variables': {
      'webrtc_root%': '<(DEPTH)/webrtc',
    },
    'webrtc_root%': '<(webrtc_root)',
    'build_with_chromium': 0,
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
