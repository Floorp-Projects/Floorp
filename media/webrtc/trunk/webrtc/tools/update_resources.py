#!/usr/bin/env python
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Downloads WebRTC resources files from a remote host."""

from optparse import OptionParser
from urlparse import urljoin
import os
import shutil
import sys
import tarfile
import tempfile
import urllib2

DESIRED_VERSION = 16
REMOTE_URL_BASE = 'http://commondatastorage.googleapis.com/webrtc-resources'
VERSION_FILENAME = 'webrtc-resources-version'
FILENAME_PREFIX = 'webrtc-resources-'
EXTENSION = '.tgz'
RELATIVE_OUTPUT_PATH = '../../'


def main():
  """
  Downloads WebRTC resources files from a remote host.

  This script will download WebRTC resource files used for testing, like audio
  and video files. It will check the current version in the DEPS file and
  compare it with the one downloaded (kept in a text file in the download dir).
  If the DEPS version is different than the one downloaded, the correct version
  will be downloaded.
  """

  # Make it possible to skip download using an environment variable:
  if os.getenv('WEBRTC_SKIP_RESOURCES_DOWNLOAD'):
    print 'Skipping resources download since WEBRTC_SKIP_RESOURCES_DOWNLOAD set'
    return

  # Define and parse arguments.
  parser = OptionParser()
  parser.add_option('-f', '--force', action='store_true', dest='force',
                    help='forces download and removal of existing resources.')
  parser.add_option('-b', '--base_url', dest='base_url',
                    help= 'Overrides the default Base URL (%s) and uses the '
                    'supplied URL instead.' % REMOTE_URL_BASE)
  parser.add_option('-p', dest='path', help= 'path of resources directory'
                    'relative to this script', default=RELATIVE_OUTPUT_PATH)
  options = parser.parse_args()[0]

  project_root_dir = os.path.normpath(sys.path[0] + '/' + options.path)
  downloads_dir = os.path.join(project_root_dir, 'resources')
  current_version_file = os.path.join(downloads_dir, VERSION_FILENAME)

  # Ensure the downloads dir is created.
  if not os.path.isdir(downloads_dir):
    os.mkdir(downloads_dir)

  # Download archive if forced or DEPS version is different than our current.
  current_version = _get_current_version(current_version_file)
  if DESIRED_VERSION != current_version or options.force:
    base_url = options.base_url or REMOTE_URL_BASE
    _perform_download(base_url, DESIRED_VERSION, downloads_dir)
  else:
    print 'Already have correct version: %s' % current_version


def _get_current_version(current_version_file):
  """Returns the version already downloaded (if any).

  Args:
      current_version_file: The filename of the text file containing the
          currently downloaded version (if any) on local disk.
  Returns:
      The version number, or 0 if no downloaded version exists.
  """
  current_version = 0
  if os.path.isfile(current_version_file):
    f = open(current_version_file)
    current_version = int(f.read())
    f.close()
    print 'Found downloaded resources: version: %s' % current_version
  return current_version


def _perform_download(base_url, desired_version, downloads_dir):
  """Performs the download and extracts the downloaded resources.

  Args:
      base_url: URL that holds the resource downloads.
      desired_version: Desired version, which decides the filename.
  """
  temp_dir = tempfile.mkdtemp(prefix='webrtc-resources-')
  try:
    archive_name = '%s%s%s' % (FILENAME_PREFIX, desired_version, EXTENSION)
    # urljoin requires base URL to end with slash to construct a proper URL
    # to our file:
    if not base_url[-1:] == '/':
      base_url += '/'
    remote_archive_url = urljoin(base_url, archive_name)
    # Download into the temporary directory with display of progress, inspired
    # by the Stack Overflow post at http://goo.gl/JIrbo
    temp_filename = os.path.join(temp_dir, archive_name)
    print 'Downloading: %s' % remote_archive_url

    response = urllib2.urlopen(remote_archive_url)
    temp_file = open(temp_filename, 'wb')
    meta = response.info()
    file_size_kb = int(meta.getheaders('Content-Length')[0]) / 1024
    print 'Progress: %s : %s kB' % (archive_name, file_size_kb)

    file_size_dl_kb = 0
    block_size = 65536
    while True:
      file_buffer = response.read(block_size)
      if not file_buffer:
        break
      file_size_dl_kb += len(file_buffer) / 1024
      temp_file.write(file_buffer)
      status = r'%10d kB [%3.2f%%]' % (file_size_dl_kb,
                                       file_size_dl_kb * 100. / file_size_kb)
      status += chr(8) * (len(status) + 1)
      print status,
    print
    temp_file.close()

    # Clean up the existing resources dir.
    print 'Removing old resources in %s' % downloads_dir
    shutil.rmtree(downloads_dir)
    os.mkdir(downloads_dir)

    # Extract the archive.
    archive = tarfile.open(temp_filename, 'r:gz')
    archive.extractall(downloads_dir)
    archive.close()
    print 'Extracted resource files into %s' % downloads_dir

    # Write the downloaded version to a text file in the resources dir to avoid
    # re-download of the same version in the future.
    new_version_filename = os.path.join(downloads_dir, VERSION_FILENAME)
    version_file = open(new_version_filename, 'w')
    version_file.write('%d' % desired_version)
    version_file.close()

  finally:
    # Clean up the temp dir.
    shutil.rmtree(temp_dir)

if __name__ == '__main__':
  main()
