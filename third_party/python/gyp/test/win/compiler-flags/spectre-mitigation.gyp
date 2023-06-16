# Copyright (c) 2023 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_sm_notset',
      'product_name': 'test_sm_notset',
      'type': 'executable',
      'msvs_configuration_attributes': {
        'SpectreMitigation': 'false'
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_sm_spectre',
      'product_name': 'test_sm_spectre',
      'type': 'executable',
      'msvs_configuration_attributes': {
        'SpectreMitigation': 'Spectre'
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_sm_spectre_load',
      'product_name': 'test_sm_spectre_load',
      'type': 'executable',
      'msvs_configuration_attributes': {
        'SpectreMitigation': 'SpectreLoad'
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_sm_spectre_load_cf',
      'product_name': 'test_sm_spectre_load_cf',
      'type': 'executable',
      'msvs_configuration_attributes': {
        'SpectreMitigation': 'SpectreLoadCF'
      },
      'sources': ['hello.cc'],
    }
  ]
}
