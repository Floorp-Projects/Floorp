# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['sysroot!=""', {
      'variables': {
        'pkg-config': './pkg-config-wrapper "<(sysroot)"',
      },
    }, {
      'variables': {
        'pkg-config': 'pkg-config'
      },
    }],
    [ 'os_posix==1 and OS!="mac"', {
      'variables': {
        # We use our own copy of libssl3, although we still need to link against
        # the rest of NSS.
        'use_system_ssl%': 0,
      },
    }, {
      'variables': {
        'use_system_ssl%': 1,
      },
    }],
  ],


  'targets': [
    {
      'target_name': 'gtk',
      'type': 'none',
      'toolsets': ['host', 'target'],
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gtk+-2.0 gthread-2.0)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other gtk+-2.0 gthread-2.0)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l gtk+-2.0 gthread-2.0)',
            ],
          },
        }, {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags gtk+-2.0 gthread-2.0)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other gtk+-2.0 gthread-2.0)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l gtk+-2.0 gthread-2.0)',
            ],
          },
        }],
        ['chromeos==1', {
          'link_settings': {
            'libraries': [ '-lXtst' ]
          }
        }],
      ],
    },
    {
      'target_name': 'gtkprint',
      'type': 'none',
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gtk+-unix-print-2.0)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other gtk+-unix-print-2.0)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l gtk+-unix-print-2.0)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'ssl',
      'type': 'none',
      'conditions': [
        ['_toolset=="target"', {
          'conditions': [
            ['use_openssl==1', {
              'dependencies': [
                '../../third_party/openssl/openssl.gyp:openssl',
              ],
            }],
            ['use_openssl==0 and use_system_ssl==0', {
              'dependencies': [
                '../../net/third_party/nss/ssl.gyp:libssl',
                '../../third_party/zlib/zlib.gyp:zlib',
              ],
              'direct_dependent_settings': {
                'include_dirs+': [
                  # We need for our local copies of the libssl3 headers to come
                  # before other includes, as we are shadowing system headers.
                  '<(DEPTH)/net/third_party/nss/ssl',
                ],
                'cflags': [
                  '<!@(<(pkg-config) --cflags nss)',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(<(pkg-config) --libs-only-L --libs-only-other nss)',
                ],
                'libraries': [
                  '<!@(<(pkg-config) --libs-only-l nss | sed -e "s/-lssl3//")',
                ],
              },
            }],
            ['use_openssl==0 and use_system_ssl==1', {
              'direct_dependent_settings': {
                'cflags': [
                  '<!@(<(pkg-config) --cflags nss)',
                ],
                'defines': [
                  'USE_SYSTEM_SSL',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(<(pkg-config) --libs-only-L --libs-only-other nss)',
                ],
                'libraries': [
                  '<!@(<(pkg-config) --libs-only-l nss)',
                ],
              },
            }],
          ]
        }],
      ],
    },
    {
      'target_name': 'freetype2',
      'type': 'none',
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags freetype2)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other freetype2)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l freetype2)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'fontconfig',
      'type': 'none',
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags fontconfig)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other fontconfig)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l fontconfig)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'gdk',
      'type': 'none',
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gdk-2.0)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other gdk-2.0)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l gdk-2.0)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'gconf',
      'type': 'none',
      'conditions': [
        ['use_gconf==1 and _toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gconf-2.0)',
            ],
            'defines': [
              'USE_GCONF',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other gconf-2.0)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l gconf-2.0)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'gio',
      'type': 'none',
      'conditions': [
        ['use_gio==1 and _toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gio-2.0)',
            ],
            'defines': [
              'USE_GIO',
            ],
            'conditions': [
              ['linux_link_gsettings==0', {
                'defines': ['DLOPEN_GSETTINGS'],
              }],
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other gio-2.0)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l gio-2.0)',
            ],
            'conditions': [
              ['linux_link_gsettings==0 and OS=="linux"', {
                'libraries': [
                  '-ldl',
                ],
              }],
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'x11',
      'type': 'none',
      'toolsets': ['host', 'target'],
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags x11)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other x11 xi)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l x11 xi)',
            ],
          },
        }, {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags x11)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other x11 xi)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l x11 xi)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'xext',
      'type': 'none',
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags xext)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other xext)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l xext)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'xfixes',
      'type': 'none',
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags xfixes)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other xfixes)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l xfixes)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'libgcrypt',
      'type': 'none',
      'conditions': [
        ['_toolset=="target" and use_cups==1', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(libgcrypt-config --cflags)',
            ],
          },
          'link_settings': {
            'libraries': [
              '<!@(libgcrypt-config --libs)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'selinux',
      'type': 'none',
      'conditions': [
        ['_toolset=="target"', {
          'link_settings': {
            'libraries': [
              '-lselinux',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'gnome_keyring',
      'type': 'none',
      'conditions': [
        ['use_gnome_keyring==1', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gnome-keyring-1)',
            ],
            'defines': [
              'USE_GNOME_KEYRING',
            ],
            'conditions': [
              ['linux_link_gnome_keyring==0', {
                'defines': ['DLOPEN_GNOME_KEYRING'],
              }],
            ],
          },
          'conditions': [
            ['linux_link_gnome_keyring!=0', {
              'link_settings': {
                'ldflags': [
                  '<!@(<(pkg-config) --libs-only-L --libs-only-other gnome-keyring-1)',
                ],
                'libraries': [
                  '<!@(<(pkg-config) --libs-only-l gnome-keyring-1)',
                ],
              },
            }, {
              'conditions': [
                ['OS=="linux"', {
                 'link_settings': {
                   'libraries': [
                     '-ldl',
                   ],
                 },
                }],
              ],
            }],
          ],
        }],
      ],
    },
    {
      # The unit tests use a few convenience functions from the GNOME
      # Keyring library directly. We ignore linux_link_gnome_keyring and
      # link directly in this version of the target to allow this.
      # *** Do not use this target in the main binary! ***
      'target_name': 'gnome_keyring_direct',
      'type': 'none',
      'conditions': [
        ['use_gnome_keyring==1', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gnome-keyring-1)',
            ],
            'defines': [
              'USE_GNOME_KEYRING',
            ],
            'conditions': [
              ['linux_link_gnome_keyring==0', {
                'defines': ['DLOPEN_GNOME_KEYRING'],
              }],
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other gnome-keyring-1)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l gnome-keyring-1)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'dbus',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags dbus-1)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other dbus-1)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l dbus-1)',
        ],
      },
    },
    {
      # TODO(satorux): Remove this once dbus-glib clients are gone.
      'target_name': 'dbus-glib',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags dbus-glib-1)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other dbus-glib-1)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l dbus-glib-1)',
        ],
      },
    },
    {
      'target_name': 'glib',
      'type': 'none',
      'toolsets': ['host', 'target'],
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags glib-2.0 gobject-2.0 gthread-2.0)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other glib-2.0 gobject-2.0 gthread-2.0)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l glib-2.0 gobject-2.0 gthread-2.0)',
            ],
          },
        }, {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags glib-2.0 gobject-2.0 gthread-2.0)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other glib-2.0 gobject-2.0 gthread-2.0)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l glib-2.0 gobject-2.0 gthread-2.0)',
            ],
          },
        }],
        ['chromeos==1', {
          'link_settings': {
            'libraries': [ '-lXtst' ]
          }
        }],
      ],
    },
    {
      'target_name': 'pangocairo',
      'type': 'none',
      'toolsets': ['host', 'target'],
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags pangocairo)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other pangocairo)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l pangocairo)',
            ],
          },
        }, {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags pangocairo)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other pangocairo)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l pangocairo)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'libresolv',
      'type': 'none',
      'link_settings': {
        'libraries': [
          '-lresolv',
        ],
      },
    },
    {
      'target_name': 'ibus',
      'type': 'none',
      'conditions': [
        ['use_ibus==1', {
          'variables': {
            'ibus_min_version': '1.3.99.20110425',
          },
          'direct_dependent_settings': {
            'defines': ['HAVE_IBUS=1'],
            'cflags': [
              '<!@(<(pkg-config) --cflags "ibus-1.0 >= <(ibus_min_version)")',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other "ibus-1.0 >= <(ibus_min_version)")',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l "ibus-1.0 >= <(ibus_min_version)")',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'wayland',
      'type': 'none',
      'conditions': [
        ['use_wayland == 1', {
          'cflags': [
            '<!@(<(pkg-config) --cflags cairo wayland-client wayland-egl xkbcommon)',
          ],
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags cairo wayland-client wayland-egl xkbcommon)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other wayland-client wayland-egl xkbcommon)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l wayland-client wayland-egl xkbcommon)',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'udev',
      'type': 'none',
      'conditions': [
        # libudev is not available on *BSD
        ['_toolset=="target" and os_bsd!=1', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags libudev)'
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other libudev)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l libudev)',
            ],
          },
        }],
      ],
    },
  ],
}
