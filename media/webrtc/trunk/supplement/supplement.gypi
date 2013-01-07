# This file will be picked up by gyp to initialize some global settings.
{
  'variables': {
    'build_with_chromium': 0,
    'enable_protobuf': 0,
    'enabled_libjingle_device_manager': 1,
    'include_internal_audio_device': 1,
    'include_internal_video_capture': 1,
    'include_internal_video_render': 0,
    'include_pulse_audio': 0,
    'conditions': [
      ['moz_widget_toolkit_gonk==1', {
        'include_internal_audio_device': 0,
      }],
    ],
  },
}
