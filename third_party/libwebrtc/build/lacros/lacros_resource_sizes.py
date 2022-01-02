#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reports binary size metrics for LaCrOS build artifacts.

More information at //docs/speed/binary_size/metrics.md.
"""

import argparse
import contextlib
import json
import logging
import os
import subprocess
import sys


@contextlib.contextmanager
def _SysPath(path):
  """Library import context that temporarily appends |path| to |sys.path|."""
  if path and path not in sys.path:
    sys.path.insert(0, path)
  else:
    path = None  # Indicates that |sys.path| is not modified.
  try:
    yield
  finally:
    if path:
      sys.path.pop(0)


DIR_SOURCE_ROOT = os.environ.get(
    'CHECKOUT_SOURCE_ROOT',
    os.path.abspath(
        os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)))

BUILD_COMMON_PATH = os.path.join(DIR_SOURCE_ROOT, 'build', 'util', 'lib',
                                 'common')

TRACING_PATH = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'catapult',
                            'tracing')

with _SysPath(BUILD_COMMON_PATH):
  import perf_tests_results_helper  # pylint: disable=import-error

with _SysPath(TRACING_PATH):
  from tracing.value import convert_chart_json  # pylint: disable=import-error

_BASE_CHART = {
    'format_version': '0.1',
    'benchmark_name': 'resource_sizes',
    'benchmark_description': 'LaCrOS resource size information.',
    'trace_rerun_options': [],
    'charts': {}
}


class _Group:
  """A group of build artifacts whose file sizes are summed and tracked.

  Build artifacts for size tracking fall under these categories:
  * File: A single file.
  * Group: A collection of files.
  * Dir: All files under a directory.

  Attributes:
    paths: A list of files or directories to be tracked together.
    title: The display name of the group.
    track_compressed: Whether to also track summed compressed sizes.
  """

  def __init__(self, paths, title, track_compressed=False):
    self.paths = paths
    self.title = title
    self.track_compressed = track_compressed


# List of disjoint build artifact groups for size tracking. This list should be
# synched with chromeos-amd64-generic-lacros-rel builder contents (specified in
# //infra/config/subprojects/chromium/ci.star) and
# chromeos-amd64-generic-lacros-internal builder (specified in src-internal).
_TRACKED_GROUPS = [
    _Group(paths=['chrome'], title='File: chrome', track_compressed=True),
    _Group(paths=['crashpad_handler'], title='File: crashpad_handler'),
    _Group(paths=['icudtl.dat'], title='File: icudtl.dat'),
    _Group(paths=['nacl_helper'], title='File: nacl_helper'),
    _Group(paths=['nacl_irt_x86_64.nexe'], title='File: nacl_irt_x86_64.nexe'),
    _Group(paths=['resources.pak'], title='File: resources.pak'),
    _Group(paths=[
        'chrome_100_percent.pak', 'chrome_200_percent.pak', 'headless_lib.pak'
    ],
           title='Group: Other PAKs'),
    _Group(paths=['snapshot_blob.bin'], title='Group: Misc'),
    _Group(paths=['locales/'], title='Dir: locales'),
    _Group(paths=['swiftshader/'], title='Dir: swiftshader'),
    _Group(paths=['WidevineCdm/'], title='Dir: WidevineCdm'),
]


def _visit_paths(base_dir, paths):
  """Itemizes files specified by a list of paths.

  Args:
    base_dir: Base directory for all elements in |paths|.
    paths: A list of filenames or directory names to specify files whose sizes
      to be counted. Directories are recursed. There's no de-duping effort.
      Non-existing files or directories are ignored (with warning message).
  """
  for path in paths:
    full_path = os.path.join(base_dir, path)
    if os.path.exists(full_path):
      if os.path.isdir(full_path):
        for dirpath, _, filenames in os.walk(full_path):
          for filename in filenames:
            yield os.path.join(dirpath, filename)
      else:  # Assume is file.
        yield full_path
    else:
      logging.critical('Not found: %s', path)


def _get_filesize(filename):
  """Returns the size of a file, or 0 if file is not found."""
  try:
    return os.path.getsize(filename)
  except OSError:
    logging.critical('Failed to get size: %s', filename)
  return 0


def _get_gzipped_filesize(filename):
  """Returns the gzipped size of a file, or 0 if file is not found."""
  BUFFER_SIZE = 65536
  if not os.path.isfile(filename):
    return 0
  try:
    # Call gzip externally instead of using gzip package since it's > 2x faster.
    cmd = ['gzip', '-c', filename]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    # Manually counting bytes instead of using len(p.communicate()[0]) to avoid
    # buffering the entire compressed data (can be ~100 MB).
    ret = 0
    while True:
      chunk = len(p.stdout.read(BUFFER_SIZE))
      if chunk == 0:
        break
      ret += chunk
    return ret
  except OSError:
    logging.critical('Failed to get gzipped size: %s', filename)
  return 0


def _dump_chart_json(output_dir, chartjson):
  """Writes chart histogram to JSON files.

  Output files:
    results-chart.json contains the chart JSON.
    perf_results.json contains histogram JSON for Catapult.

  Args:
    output_dir: Directory to place the JSON files.
    chartjson: Source JSON data for output files.
  """
  results_path = os.path.join(output_dir, 'results-chart.json')
  logging.critical('Dumping chartjson to %s', results_path)
  with open(results_path, 'w') as json_file:
    json.dump(chartjson, json_file, indent=2)

  # We would ideally generate a histogram set directly instead of generating
  # chartjson then converting. However, perf_tests_results_helper is in
  # //build, which doesn't seem to have any precedent for depending on
  # anything in Catapult. This can probably be fixed, but since this doesn't
  # need to be super fast or anything, converting is a good enough solution
  # for the time being.
  histogram_result = convert_chart_json.ConvertChartJson(results_path)
  if histogram_result.returncode != 0:
    raise Exception('chartjson conversion failed with error: ' +
                    histogram_result.stdout)

  histogram_path = os.path.join(output_dir, 'perf_results.json')
  logging.critical('Dumping histograms to %s', histogram_path)
  with open(histogram_path, 'w') as json_file:
    json_file.write(histogram_result.stdout)


def _run_resource_sizes(args):
  """Main flow to extract and output size data."""
  chartjson = _BASE_CHART.copy()
  report_func = perf_tests_results_helper.ReportPerfResult
  total_size = 0
  total_gzipped = 0
  for group in _TRACKED_GROUPS:
    group_size = sum(map(_get_filesize, _visit_paths(args.out_dir,
                                                     group.paths)))
    group_gzipped = sum(
        map(_get_gzipped_filesize, _visit_paths(args.out_dir, group.paths)))
    report_func(chart_data=chartjson,
                graph_title=group.title,
                trace_title='size',
                value=group_size,
                units='bytes')
    if group.track_compressed:
      report_func(chart_data=chartjson,
                  graph_title=group.title + ' (Gzipped)',
                  trace_title='size',
                  value=group_gzipped,
                  units='bytes')
    total_size += group_size
    # Summing compressed size of separate groups (instead of concatanating
    # first) to get a conservative estimate. File metadata and overheads are
    # assumed to be negligible.
    total_gzipped += group_gzipped

  report_func(chart_data=chartjson,
              graph_title='Total',
              trace_title='size',
              value=total_size,
              units='bytes')

  report_func(chart_data=chartjson,
              graph_title='Total (Gzipped)',
              trace_title='size',
              value=total_gzipped,
              units='bytes')

  _dump_chart_json(args.output_dir, chartjson)


def main():
  """Parses arguments and runs high level flows."""
  argparser = argparse.ArgumentParser(description='Writes LaCrOS size metrics.')

  argparser.add_argument('--chromium-output-directory',
                         dest='out_dir',
                         required=True,
                         type=os.path.realpath,
                         help='Location of the build artifacts.')

  output_group = argparser.add_mutually_exclusive_group()

  output_group.add_argument('--output-dir',
                            default='.',
                            help='Directory to save chartjson to.')

  # Accepted to conform to the isolated script interface, but ignored.
  argparser.add_argument('--isolated-script-test-filter',
                         help=argparse.SUPPRESS)
  argparser.add_argument('--isolated-script-test-perf-output',
                         type=os.path.realpath,
                         help=argparse.SUPPRESS)

  output_group.add_argument(
      '--isolated-script-test-output',
      type=os.path.realpath,
      help='File to which results will be written in the simplified JSON '
      'output format.')

  args = argparser.parse_args()

  isolated_script_output = {'valid': False, 'failures': []}
  if args.isolated_script_test_output:
    test_name = 'lacros_resource_sizes'
    args.output_dir = os.path.join(
        os.path.dirname(args.isolated_script_test_output), test_name)
    if not os.path.exists(args.output_dir):
      os.makedirs(args.output_dir)

  try:
    _run_resource_sizes(args)
    isolated_script_output = {'valid': True, 'failures': []}
  finally:
    if args.isolated_script_test_output:
      results_path = os.path.join(args.output_dir, 'test_results.json')
      with open(results_path, 'w') as output_file:
        json.dump(isolated_script_output, output_file)
      with open(args.isolated_script_test_output, 'w') as output_file:
        json.dump(isolated_script_output, output_file)


if __name__ == '__main__':
  main()
