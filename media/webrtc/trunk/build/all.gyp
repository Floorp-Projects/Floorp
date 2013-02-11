# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'xcode_create_dependents_test_runner': 1,
      'dependencies': [
        'some.gyp:*',
        '../base/base.gyp:*',
        '../content/content.gyp:*',
        '../crypto/crypto.gyp:*',
        '../media/media.gyp:*',
        '../net/net.gyp:*',
        '../sdch/sdch.gyp:*',
        '../sql/sql.gyp:*',
        '../testing/gmock.gyp:*',
        '../testing/gtest.gyp:*',
        '../third_party/bzip2/bzip2.gyp:*',
        '../third_party/icu/icu.gyp:*',
        '../third_party/libxml/libxml.gyp:*',
        '../third_party/sqlite/sqlite.gyp:*',
        '../third_party/zlib/zlib.gyp:*',
        '../ui/ui.gyp:*',
        '../webkit/support/webkit_support.gyp:*',
        'temp_gyp/googleurl.gyp:*',
      ],
      'conditions': [
        ['OS!="ios"', {
          'dependencies': [
            '../cc/cc_tests.gyp:*',
            '../chrome/chrome.gyp:*',
            '../gpu/gpu.gyp:*',
            '../gpu/tools/tools.gyp:*',
            '../ipc/ipc.gyp:*',
            '../jingle/jingle.gyp:*',
            '../ppapi/ppapi.gyp:*',
            '../ppapi/ppapi_internal.gyp:*',
            '../printing/printing.gyp:*',
            '../skia/skia.gyp:*',
            '../sync/sync.gyp:*',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:*',
            '../third_party/cld/cld.gyp:*',
            '../third_party/codesighs/codesighs.gyp:*',
            '../third_party/ffmpeg/ffmpeg.gyp:*',
            '../third_party/iccjpeg/iccjpeg.gyp:*',
            '../third_party/libpng/libpng.gyp:*',
            '../third_party/libusb/libusb.gyp:*',
            '../third_party/libwebp/libwebp.gyp:*',
            '../third_party/libxslt/libxslt.gyp:*',
            '../third_party/lzma_sdk/lzma_sdk.gyp:*',
            '../third_party/mesa/mesa.gyp:*',
            '../third_party/modp_b64/modp_b64.gyp:*',
            '../third_party/npapi/npapi.gyp:*',
            '../third_party/ots/ots.gyp:*',
            '../third_party/qcms/qcms.gyp:*',
            '../third_party/re2/re2.gyp:re2',
            '../third_party/WebKit/Source/WebKit/chromium/All.gyp:*',
            '../v8/tools/gyp/v8.gyp:*',
            '../webkit/compositor_bindings/compositor_bindings_tests.gyp:*',
            '../webkit/webkit.gyp:*',
            '<(libjpeg_gyp_path):*',
          ],
        }],
        ['os_posix==1 and OS!="android" and OS!="ios"', {
          'dependencies': [
            '../third_party/yasm/yasm.gyp:*#host',
          ],
        }],
        ['OS=="mac" or OS=="ios" or OS=="win"', {
          'dependencies': [
            '../third_party/nss/nss.gyp:*',
           ],
        }],
        ['OS=="win" or OS=="ios" or OS=="linux"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:*',
           ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            '../third_party/ocmock/ocmock.gyp:*',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../courgette/courgette.gyp:*',
            '../dbus/dbus.gyp:*',
            '../sandbox/sandbox.gyp:*',
          ],
          'conditions': [
            ['branding=="Chrome"', {
              'dependencies': [
                '../chrome/chrome.gyp:linux_packages_<(channel)',
              ],
            }],
            ['chromeos==0', {
              'dependencies': [
                '../third_party/cros_dbus_cplusplus/cros_dbus_cplusplus.gyp:*',
              ],
            }],
          ],
        }],
        ['toolkit_uses_gtk==1', {
          'dependencies': [
            '../tools/gtk_clipboard_dump/gtk_clipboard_dump.gyp:*',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:*',
          ],
        }],
        ['OS=="win"', {
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:*',
              ],
            }],
          ],
          'dependencies': [
            '../chrome_frame/chrome_frame.gyp:*',
            '../cloud_print/cloud_print.gyp:*',
            '../courgette/courgette.gyp:*',
            '../rlz/rlz.gyp:*',
            '../sandbox/sandbox.gyp:*',
            '../third_party/angle/src/build_angle.gyp:*',
            '../third_party/bsdiff/bsdiff.gyp:*',
            '../third_party/bspatch/bspatch.gyp:*',
            '../third_party/gles2_book/gles2_book.gyp:*',
            '../tools/memory_watcher/memory_watcher.gyp:*',
          ],
        }, {
          'dependencies': [
            '../third_party/libevent/libevent.gyp:*',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/controls/webview/webview.gyp:*',
            '../ui/views/views.gyp:*',
          ],
        }],
        ['use_aura==1', {
          'dependencies': [
            '../ash/ash.gyp:*',
            '../ui/aura/aura.gyp:*',
            '../ui/oak/oak.gyp:*',
          ],
        }],
        ['remoting==1', {
          'dependencies': [
            '../remoting/remoting.gyp:*',
          ],
        }],
        ['use_openssl==0', {
          'dependencies': [
            '../net/third_party/nss/ssl.gyp:*',
          ],
        }],
      ],
    }, # target_name: All
    {
      'target_name': 'All_syzygy',
      'type': 'none',
      'conditions': [
        ['OS=="win" and fastbuild==0', {
            'dependencies': [
              '../chrome/installer/mini_installer_syzygy.gyp:*',
            ],
          },
        ],
      ],
    }, # target_name: All_syzygy
    {
      'target_name': 'chromium_builder_tests',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base_unittests',
        '../crypto/crypto.gyp:crypto_unittests',
        '../media/media.gyp:media_unittests',
        '../net/net.gyp:net_unittests',
        '../sql/sql.gyp:sql_unittests',
        '../ui/ui.gyp:ui_unittests',
        'temp_gyp/googleurl.gyp:googleurl_unittests',
      ],
      'conditions': [
        ['OS!="ios"', {
          'dependencies': [
            '../cc/cc_tests.gyp:cc_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:safe_browsing_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../content/content.gyp:content_browsertests',
            '../content/content.gyp:content_unittests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../gpu/gles2_conform_support/gles2_conform_support.gyp:gles2_conform_support',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../webkit/compositor_bindings/compositor_bindings_tests.gyp:webkit_compositor_bindings_unittests',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '../chrome/chrome.gyp:installer_util_unittests',
            '../chrome/chrome.gyp:mini_installer_test',
            # mini_installer_tests depends on mini_installer. This should be
            # defined in installer.gyp.
            '../chrome/installer/mini_installer.gyp:mini_installer',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_net_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_perftests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_reliability_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_unittests',
            '../chrome_frame/chrome_frame.gyp:npchrome_frame',
            '../courgette/courgette.gyp:courgette_unittests',
            '../sandbox/sandbox.gyp:sbox_integration_tests',
            '../sandbox/sandbox.gyp:sbox_unittests',
            '../sandbox/sandbox.gyp:sbox_validation_tests',
            '../webkit/webkit.gyp:pull_in_copy_TestNetscapePlugIn',
            '../ui/views/views.gyp:views_unittests',
            '../webkit/webkit.gyp:test_shell_common',
           ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../sandbox/sandbox.gyp:sandbox_linux_unittests',
          ],
        }],
      ],
    }, # target_name: chromium_builder_tests
    {
      'target_name': 'chromium_2010_builder_tests',
      'type': 'none',
      'dependencies': [
        'chromium_builder_tests',
      ],
    }, # target_name: chromium_2010_builder_tests
  ],
  'conditions': [
    ['OS!="ios"', {
      'targets': [
        {
          'target_name': 'chromium_builder_nacl_win_integration',
          'type': 'none',
          'dependencies': [
            'chromium_builder_qa', # needed for pyauto
            'chromium_builder_tests',
          ],
        }, # target_name: chromium_builder_nacl_win_integration
        {
          'target_name': 'chromium_builder_perf',
          'type': 'none',
          'dependencies': [
            'chromium_builder_qa', # needed for pyauto
            '../chrome/chrome.gyp:performance_browser_tests',
            '../chrome/chrome.gyp:performance_ui_tests',
            '../chrome/chrome.gyp:sync_performance_tests',
          ],
        }, # target_name: chromium_builder_perf
        {
          'target_name': 'chromium_gpu_builder',
          'type': 'none',
          'dependencies': [
            '../chrome/chrome.gyp:gpu_tests',
            '../chrome/chrome.gyp:performance_browser_tests',
            '../chrome/chrome.gyp:performance_ui_tests',
            '../gpu/gpu.gyp:gl_tests',
          ],
          'conditions': [
            ['internal_gles2_conform_tests', {
              'dependencies': [
                '../gpu/gles2_conform_test/gles2_conform_test.gyp:gles2_conform_test',
              ],
            }], # internal_gles2_conform
          ],
        }, # target_name: chromium_gpu_builder
        {
          'target_name': 'chromium_gpu_debug_builder',
          'type': 'none',
          'dependencies': [
            '../chrome/chrome.gyp:gpu_tests',
            '../gpu/gpu.gyp:gl_tests',
          ],
          'conditions': [
            ['internal_gles2_conform_tests', {
              'dependencies': [
                '../gpu/gles2_conform_test/gles2_conform_test.gyp:gles2_conform_test',
              ],
            }], # internal_gles2_conform
          ],
        }, # target_name: chromium_gpu_debug_builder
        {
          'target_name': 'chromium_builder_qa',
          'type': 'none',
          'dependencies': [
            '../chrome/chrome.gyp:chromedriver',
            # Dependencies of pyauto_functional tests.
            '../remoting/remoting.gyp:remoting_webapp',
          ],
# not needed for Mozilla
#          'conditions': [
#            # If you change this condition, make sure you also change it
#            # in chrome_tests.gypi
#            ['enable_automation==1 and (OS=="mac" or OS=="win" or (os_posix==1 and target_arch==python_arch))', {
#              'dependencies': [
#                '../chrome/chrome.gyp:pyautolib',
#              ],
#            }],
            ['OS=="mac"', {
              'dependencies': [
                '../remoting/remoting.gyp:remoting_me2me_host_archive',
              ],
            }],
            ['OS=="win" and component != "shared_library" and wix_exists == "True" and sas_dll_exists == "True"', {
              'dependencies': [
                '../remoting/remoting.gyp:remoting_host_installation',
              ],
            }],
          ],
        }, # target_name: chromium_builder_qa
        {
          'target_name': 'chromium_builder_perf_av',
          'type': 'none',
          'dependencies': [
            'chromium_builder_qa',  # needed for perf pyauto tests
            '../webkit/webkit.gyp:pull_in_DumpRenderTree',  # to run layout tests
          ],
        },  # target_name: chromium_builder_perf_av
      ],  # targets
    }],
    ['OS=="mac"', {
      'targets': [
        {
          # Target to build everything plus the dmg.  We don't put the dmg
          # in the All target because developers really don't need it.
          'target_name': 'all_and_dmg',
          'type': 'none',
          'dependencies': [
            'All',
            '../chrome/chrome.gyp:build_app_dmg',
          ],
        },
        # These targets are here so the build bots can use them to build
        # subsets of a full tree for faster cycle times.
        {
          'target_name': 'chromium_builder_dbg',
          'type': 'none',
          'dependencies': [
            '../cc/cc_tests.gyp:cc_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:safe_browsing_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../content/content.gyp:content_browsertests',
            '../content/content.gyp:content_unittests',
            '../ui/ui.gyp:ui_unittests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/media.gyp:media_unittests',
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../rlz/rlz.gyp:*',
            '../sql/sql.gyp:sql_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../webkit/compositor_bindings/compositor_bindings_tests.gyp:webkit_compositor_bindings_unittests',
            'temp_gyp/googleurl.gyp:googleurl_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_rel',
          'type': 'none',
          'dependencies': [
            '../cc/cc_tests.gyp:cc_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:performance_browser_tests',
            '../chrome/chrome.gyp:performance_ui_tests',
            '../chrome/chrome.gyp:safe_browsing_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../content/content.gyp:content_browsertests',
            '../content/content.gyp:content_unittests',
            '../ui/ui.gyp:ui_unittests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/media.gyp:media_unittests',
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../webkit/compositor_bindings/compositor_bindings_tests.gyp:webkit_compositor_bindings_unittests',
            'temp_gyp/googleurl.gyp:googleurl_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_dbg_tsan_mac',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../crypto/crypto.gyp:crypto_unittests',
            'temp_gyp/googleurl.gyp:googleurl_unittests',
            '../net/net.gyp:net_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/media.gyp:media_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_asan_mac',
          'type': 'none',
          'dependencies': [
            '../chrome/chrome.gyp:chrome',
            '../net/net.gyp:dns_fuzz_stub',
            '../webkit/webkit.gyp:pull_in_DumpRenderTree',
          ],
        },
        {
          'target_name': 'chromium_builder_dbg_valgrind_mac',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../crypto/crypto.gyp:crypto_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../media/media.gyp:media_unittests',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../chrome/chrome.gyp:safe_browsing_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../content/content.gyp:content_unittests',
            '../ui/ui.gyp:ui_unittests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            'temp_gyp/googleurl.gyp:googleurl_unittests',
          ],
        },
      ],  # targets
    }], # OS="mac"
    ['OS=="win"', {
      'targets': [
        # These targets are here so the build bots can use them to build
        # subsets of a full tree for faster cycle times.
        {
          'target_name': 'chromium_builder',
          'type': 'none',
          'dependencies': [
            '../cc/cc_tests.gyp:cc_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:installer_util_unittests',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:mini_installer_test',
            '../chrome/chrome.gyp:performance_browser_tests',
            '../chrome/chrome.gyp:performance_ui_tests',
            '../chrome/chrome.gyp:safe_browsing_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../content/content.gyp:content_browsertests',
            '../content/content.gyp:content_unittests',
            # mini_installer_tests depends on mini_installer. This should be
            # defined in installer.gyp.
            '../chrome/installer/mini_installer.gyp:mini_installer',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_net_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_perftests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_reliability_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_unittests',
            '../chrome_frame/chrome_frame.gyp:npchrome_frame',
            '../courgette/courgette.gyp:courgette_unittests',
            '../ui/ui.gyp:ui_unittests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/media.gyp:media_unittests',
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../ui/views/views.gyp:views_unittests',
            '../webkit/compositor_bindings/compositor_bindings_tests.gyp:webkit_compositor_bindings_unittests',
            '../webkit/webkit.gyp:pull_in_copy_TestNetscapePlugIn',
            'temp_gyp/googleurl.gyp:googleurl_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_win_cf',
          'type': 'none',
          'dependencies': [
            '../chrome_frame/chrome_frame.gyp:chrome_frame_net_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_perftests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_reliability_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_unittests',
            '../chrome_frame/chrome_frame.gyp:npchrome_frame',
          ],
        },
        {
          'target_name': 'chromium_builder_dbg_tsan_win',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../content/content.gyp:content_unittests',
            '../crypto/crypto.gyp:crypto_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/media.gyp:media_unittests',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            'temp_gyp/googleurl.gyp:googleurl_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_dbg_drmemory_win',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests',
            '../chrome/chrome.gyp:unit_tests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../content/content.gyp:content_unittests',
            '../crypto/crypto.gyp:crypto_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/media.gyp:media_unittests',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            'temp_gyp/googleurl.gyp:googleurl_unittests',
          ],
        },
        {
          'target_name': 'webkit_builder_win',
          'type': 'none',
          'dependencies': [
            '../webkit/webkit.gyp:test_shell',
            '../webkit/webkit.gyp:test_shell_tests',
            '../webkit/webkit.gyp:pull_in_webkit_unit_tests',
            '../webkit/webkit.gyp:pull_in_DumpRenderTree',
          ],
        },
        {
          'target_name': 'chromium_builder_qa_nacl_win64',
          'type': 'none',
          'dependencies': [
            'chromium_builder_qa', # needed for pyauto
            '../chrome/chrome.gyp:chrome_nacl_win64',
          ],
        }, # target_name: chromium_builder_qa_nacl_win64
      ],  # targets
      'conditions': [
        ['branding=="Chrome"', {
          'targets': [
            {
              'target_name': 'chrome_official_builder',
              'type': 'none',
              'dependencies': [
                '../chrome/chrome.gyp:chromedriver',
                '../chrome/chrome.gyp:crash_service',
                '../chrome/chrome.gyp:crash_service_win64',
                '../chrome/chrome.gyp:performance_ui_tests',
                '../chrome/chrome.gyp:policy_templates',
                '../chrome/chrome.gyp:pyautolib',
                '../chrome/chrome.gyp:reliability_tests',
                '../chrome/chrome.gyp:automated_ui_tests',
                '../chrome/installer/mini_installer.gyp:mini_installer',
                '../chrome_frame/chrome_frame.gyp:npchrome_frame',
                '../courgette/courgette.gyp:courgette',
                '../courgette/courgette.gyp:courgette64',
                '../cloud_print/cloud_print.gyp:cloud_print',
                '../remoting/remoting.gyp:remoting_webapp',
                '../third_party/adobe/flash/flash_player.gyp:flash_player',
                '../third_party/widevine/cdm/widevine_cdm.gyp:widevinecdmplugin',
              ],
              'conditions': [
                ['internal_pdf', {
                  'dependencies': [
                    '../pdf/pdf.gyp:pdf',
                  ],
                }], # internal_pdf
                ['component != "shared_library" and wix_exists == "True" and \
                    sas_dll_exists == "True"', {
                  'dependencies': [
                    '../remoting/remoting.gyp:remoting_host_installation',
                  ],
                }], # component != "shared_library"
              ]
            },
          ], # targets
        }], # branding=="Chrome"
       ], # conditions
    }], # OS="win"
    ['use_aura==1', {
      'targets': [
        {
          'target_name': 'aura_builder',
          'type': 'none',
          'dependencies': [
            '../cc/cc_tests.gyp:cc_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:chrome',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../content/content.gyp:content_browsertests',
            '../content/content.gyp:content_unittests',
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../ui/aura/aura.gyp:*',
            '../ui/compositor/compositor.gyp:*',
            '../ui/ui.gyp:ui_unittests',
            '../ui/views/views.gyp:views',
            '../ui/views/views.gyp:views_unittests',
            '../webkit/compositor_bindings/compositor_bindings_tests.gyp:webkit_compositor_bindings_unittests',
            '../webkit/webkit.gyp:pull_in_webkit_unit_tests',
          ],
          'conditions': [
            ['OS=="win"', {
              # Remove this when we have the real compositor.
              'copies': [
                {
                  'destination': '<(PRODUCT_DIR)',
                  'files': ['../third_party/directxsdk/files/dlls/D3DX10d_43.dll']
                },
              ],
              'dependencies': [
                '../content/content.gyp:content_unittests',
                '../chrome/chrome.gyp:crash_service',
                '../chrome/chrome.gyp:crash_service_win64',
              ],
            }],
            ['use_ash==1', {
              'dependencies': [
                '../ash/ash.gyp:ash_shell',
                '../ash/ash.gyp:ash_unittests',
              ],
            }],
            ['OS=="linux"', {
              # Tests that currently only work on Linux.
              'dependencies': [
                '../base/base.gyp:base_unittests',
                '../content/content.gyp:content_unittests',
                '../ipc/ipc.gyp:ipc_tests',
                '../sql/sql.gyp:sql_unittests',
                '../sync/sync.gyp:sync_unit_tests',
              ],
            }],
            ['OS=="mac"', {
              # Exclude dependencies that are not currently implemented.
              'dependencies!': [
                '../chrome/chrome.gyp:chrome',
                '../chrome/chrome.gyp:unit_tests',
                '../ui/views/views.gyp:views_unittests',
              ],
            }],
            ['chromeos==1', {
              'dependencies': [
                '../chromeos/chromeos.gyp:chromeos_unittests',
              ],
            }],
          ],
        },
      ],  # targets
    }], # "use_aura==1"
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'chromium_swarm_tests',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests_run',
            '../chrome/chrome.gyp:browser_tests_run',
            '../chrome/chrome.gyp:unit_tests_run',
            '../net/net.gyp:net_unittests_run',
          ],
        }, # target_name: chromium_swarm_tests
      ],
    }],
  ], # conditions
}
