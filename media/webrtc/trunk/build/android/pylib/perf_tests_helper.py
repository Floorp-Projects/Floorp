# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


# Valid values of result type.
RESULT_TYPES = {'unimportant': 'RESULT ',
                'default': '*RESULT ',
                'informational': ''}


def _EscapePerfResult(s):
  """Escapes |s| for use in a perf result."""
  # Colons (:) and equal signs (=) are not allowed, and we chose an arbitrary
  # limit of 40 chars.
  return re.sub(':|=', '_', s[:40])


def PrintPerfResult(measurement, trace, values, units, result_type='default',
                    print_to_stdout=True):
  """Prints numerical data to stdout in the format required by perf tests.

  The string args may be empty but they must not contain any colons (:) or
  equals signs (=).

  Args:
    measurement: A description of the quantity being measured, e.g. "vm_peak".
    trace: A description of the particular data point, e.g. "reference".
    values: A list of numeric measured values.
    units: A description of the units of measure, e.g. "bytes".
    result_type: A  tri-state that accepts values of ['unimportant', 'default',
        'informational']. 'unimportant' prints RESULT, 'default' prints *RESULT
        and 'informational' prints nothing.
    print_to_stdout: If True, prints the output in stdout instead of returning
        the output to caller.

    Returns:
      String of the formated perf result.
  """
  assert result_type in RESULT_TYPES, 'result type: %s is invalid' % result_type

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

  trace_name = _EscapePerfResult(trace)
  output = '%s%s: %s%s%s %s' % (
    RESULT_TYPES[result_type],
    _EscapePerfResult(measurement),
    trace_name,
    # Do not show equal sign if the trace is empty. Usually it happens when
    # measurement is enough clear to describe the result.
    '= ' if trace_name else '',
    value,
    units)
  if avg:
    output += '\nAvg %s: %f%s' % (measurement, avg, units)
  if print_to_stdout:
    print output
  return output
