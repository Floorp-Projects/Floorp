# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'lastchange',
      'type': 'none',
      'variables': {
        'lastchange_out_path': '<(DEPTH)/build/util/LASTCHANGE',
        'default_lastchange_path': '../LASTCHANGE.in',
      },
      'actions': [
        {
          'action_name': 'lastchange',
          'inputs': [
            # Note:  <(default_lastchange_path) is optional,
            # so it doesn't show up in inputs.
            './lastchange.py',
          ],
          'outputs': [
            '<(lastchange_out_path)',
          ],
          'action': [
            'python', '<@(_inputs)',
            '-o', '<(lastchange_out_path)',
            '-d', '<(default_lastchange_path)',
          ],
          'message': 'Extracting last change to <(lastchange_out_path)',
          'process_outputs_as_sources': '1',
        },
      ],
    },
  ]
}
