# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'depfile_target',
      'type': 'none',
      'actions': [
        {
          'action_name': 'depfile_action',
          'inputs': [
            'input.txt',
          ],
          'outputs': [
            'output.txt',
          ],
          'depfile': 'depfile_action.d',
          'action': [
            'python', 'touch.py', '<(PRODUCT_DIR)/<(_depfile)',
          ],
          'msvs_cygwin_shell': 0,
        },
        {
          'action_name': 'depfile_action_intermediate_dir',
          'inputs': [
            'input.txt',
          ],
          'outputs': [
            'output-intermediate.txt',
          ],
          'depfile': '<(INTERMEDIATE_DIR)/depfile_action_intermediate_dir.d',
          'action': [
            'python', 'touch.py', '<(_depfile)',
          ],
          'msvs_cygwin_shell': 0,
        },
      ],
    },
  ],
}
