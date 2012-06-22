#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

__author__ = 'ivinnichenko@webrtc.org (Illya Vinnichenko)'

"""This script will prune sufficiently old files and empty directories.

   The algorithm is to look into the provided directory and delete any files
   that is older than x days, recursively. Then all empty directories will be
   deleted (we can't look at timestamps there since the act of deleting a file
   will refresh the directory's timestamp).

   Note: This script has only been tested on Linux.
"""

from optparse import OptionParser
import os
import sys
import time

# The path is considered whitelisted if any of these entries appear
# at some point in the path
WHITELIST = ['buildbot', 'buildbot.tac', 'Makefile', 'master.cfg',
             'public_html', 'slaves.cfg', 'state.sqlite', 'twistd.pid']


def is_whitelisted(path):
  '''Check if file is whitelisted.

    Args:
      path: file path.
  '''
  for entry in WHITELIST:
    if entry in path:
      return True
  return False


def delete_directory(directory):
  try:
    os.rmdir(directory)
    return True
  except OSError as exception:
    if 'not empty' in str(exception):
      # This is normal, ignore it
      pass
    else:
      print 'Could not remove directory %s: reason %s.' % (directory, exception)
  return False


def delete_file(file):
  try:
    os.remove(file)
  except OSError as exception:
    print 'Unexpectedly failed to remove file %s: reason %s.' % (file,
                                                                 exception)


def log_removal(file_or_directory, time_stamp, verbose):
  if verbose:
    str_stamp = time.strftime('%a, %d %b %Y %H:%M:%S +0000',
                              time.gmtime(time_stamp))
    print 'Removing [%s], stamped on %s' % (file_or_directory, str_stamp)


def remove_old_files_and_directories(path, num_days, verbose, skip_dirs):
  '''Removes all files under path that are older than num_days days.
     The algorithm also tried to delete all directories, except for those who
     contain files that are sufficiently new.

     Implementation note: it doesn't make sense to look at timestamps for
     directories since their timestamps are updated when a file is deleted.

    Args:
      path: The starting point.
      num_days: days limit for removal.
      verbose: print every cmd?
  '''
  current_time = time.time()
  limit = 60 * 60 * 24 * num_days

  # Walk bottom-up so directories are deleted in the right order.
  for root, directories, files in os.walk(path, topdown=False):
    for file in files:
      current_file = os.path.join(root, file)
      time_stamp = os.stat(current_file).st_mtime

      if is_whitelisted(current_file):
        continue

      if (current_time - time_stamp) > limit:
        delete_file(current_file)
        log_removal(current_file, time_stamp, verbose)

    if not skip_dirs:
      for directory in directories:
        current_directory = os.path.join(root, directory)
        time_stamp = os.stat(current_directory).st_mtime
        if delete_directory(current_directory):
          log_removal(current_directory, time_stamp, verbose)


def main():
  usage = 'usage: %prog -p <base path> -n <number of days> [-q] [-d]'
  parser = OptionParser(usage)
  parser.add_option('-p', '--path', dest='cleanup_path', help='base directory')
  parser.add_option('-n', '--num_days', dest='num_days', help='number of days')
  parser.add_option('-q', '--quiet',
                    action='store_false', dest='verbose', default=True,
                    help='do not print status messages to stdout')
  parser.add_option('-d', '--delete-dirs-too',
                    action='store_false', dest='skip_dirs', default=True,
                    help='number of days')

  options, args = parser.parse_args()
  if not options.cleanup_path:
    print 'You must specify base directory'
    sys.exit(2)
  if not options.num_days:
    print 'You must specify number of days old'
    sys.exit(2)

  if options.verbose:
    print 'Cleaning up everything in %s older than %s days' % (
        options.cleanup_path, options.num_days)
  remove_old_files_and_directories(options.cleanup_path, int(options.num_days),
                                   options.verbose, options.skip_dirs)

if __name__ == '__main__':
  main()
