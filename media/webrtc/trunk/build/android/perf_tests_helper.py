# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


def _EscapePerfResult(s):
  """Escapes |s| for use in a perf result."""
  # Colons (:) and equal signs (=) are not allowed, and we chose an arbitrary
  # limit of 40 chars.
  return re.sub(':|=', '_', s[:40])


def PrintPerfResult(measurement, trace, values, units, important=True,
                    print_to_stdout=True):
  """Prints numerical data to stdout in the format required by perf tests.

  The string args may be empty but they must not contain any colons (:) or
  equals signs (=).

  Args:
    measurement: A description of the quantity being measured, e.g. "vm_peak".
    trace: A description of the particular data point, e.g. "reference".
    values: A list of numeric measured values.
    units: A description of the units of measure, e.g. "bytes".
    important: If True, the output line will be specially marked, to notify the
        post-processor.

    Returns:
      String of the formated perf result.
  """
  important_marker = '*' if important else ''

  assert isinstance(values, list)
  assert len(values)
  assert '/' not in measurement
  avg = None
  if len(values) > 1:
    try:
      value = '[%s]' % ','.join([str(v) for v in values])
      avg = sum([float(v) for v in values]) / len(values)
    except ValueError:
      value = ", ".join(values)
  else:
    value = values[0]

  output = '%sRESULT %s: %s= %s %s' % (important_marker,
                                       _EscapePerfResult(measurement),
                                       _EscapePerfResult(trace),
                                       value, units)
  if avg:
    output += '\nAvg %s: %d%s' % (measurement, avg, units)
  if print_to_stdout:
    print output
  return output
