# Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.


def _HasLocalChanges(input_api):
  ret = input_api.subprocess.call(['git', 'diff', '--quiet'])
  return ret != 0


def CheckPatchFormatted(input_api, output_api):
  results = []
  file_filter = lambda x: x.LocalPath().endswith('.pyl')
  affected_files = input_api.AffectedFiles(include_deletes=False,
                                           file_filter=file_filter)

  for f in affected_files:
    cmd = ['yapf', '-i', f.AbsoluteLocalPath()]
    if input_api.subprocess.call(cmd):
      results.append(output_api.PresubmitError('Error calling "' + cmd + '"'))

  if _HasLocalChanges(input_api):
    msg = ('Diff found after running "yapf -i" on modified .pyl files.\n'
           'Please commit or discard the new changes.')
    results.append(output_api.PresubmitError(msg))

  return results


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(CheckPatchFormatted(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(CheckPatchFormatted(input_api, output_api))
  return results
