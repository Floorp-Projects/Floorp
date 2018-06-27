# This file will be picked up by gyp to initialize some global settings.
{
  'variables': {
# make sure we can override this with --include
    'build_with_chromium%': 1,
    'clang_use_chrome_plugins%': 0,
    'enable_protobuf%': 1,
    'enabled_libjingle_device_manager%': 1,
    'include_internal_audio_device%': 1,
    'include_internal_video_capture%': 1,
    'include_internal_video_render%': 1,
    'include_pulse_audio%': 1,
    'use_openssl%': 1,
  },
  'target_defaults': {
    'conditions': [
      ['OS=="linux" and clang==1', {
        'cflags': [
          # Suppress the warning caused by
          # LateBindingSymbolTable::TableInfo from
          # latebindingsymboltable.cc.def.
          '-Wno-address-of-array-temporary',
        ],
        'cflags_mozilla': [
          '-Wno-address-of-array-temporary',
        ],
      }],
    ],
  }, # target_defaults
}
