# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains miscellaneous utility functions that don't belong anywhere
# in particular.

import errno
import os
import stat
import sys
import time


class LockFile(object):
    """LockFile is used by the lock_file method to hold the lock.

    This object should not be used directly, but only through
    the lock_file method below.
    """

    def __init__(self, lockfile):
        self.lockfile = lockfile

    def __del__(self):
        while True:
            try:
                os.remove(self.lockfile)
                break
            except OSError as e:
                if e.errno == errno.EACCES:
                    # Another process probably has the file open, we'll retry.
                    # Just a short sleep since we want to drop the lock ASAP
                    # (but we need to let some other process close the file
                    # first).
                    time.sleep(0.1)
                else:
                    # Re-raise unknown errors
                    raise


def lock_file(lockfile, max_wait=600):
    """Create and hold a lockfile of the given name, with the given timeout.

    To release the lock, delete the returned object.
    """

    # FUTURE This function and object could be written as a context manager.

    while True:
        try:
            fd = os.open(lockfile, os.O_EXCL | os.O_RDWR | os.O_CREAT)
            # We created the lockfile, so we're the owner
            break
        except OSError as e:
            if e.errno == errno.EEXIST or (
                sys.platform == "win32" and e.errno == errno.EACCES
            ):
                pass
            else:
                # Should not occur
                raise

        try:
            # The lock file exists, try to stat it to get its age
            # and read its contents to report the owner PID
            f = open(lockfile, "r")
            s = os.stat(lockfile)
        except EnvironmentError as e:
            if e.errno == errno.ENOENT or e.errno == errno.EACCES:
                # We didn't create the lockfile, so it did exist, but it's
                # gone now. Just try again
                continue

            raise Exception(
                "{0} exists but stat() failed: {1}".format(lockfile, e.strerror)
            )

        # We didn't create the lockfile and it's still there, check
        # its age
        now = int(time.time())
        if now - s[stat.ST_MTIME] > max_wait:
            pid = f.readline().rstrip()
            raise Exception(
                "{0} has been locked for more than "
                "{1} seconds (PID {2})".format(lockfile, max_wait, pid)
            )

        # It's not been locked too long, wait a while and retry
        f.close()
        time.sleep(1)

    # if we get here. we have the lockfile. Convert the os.open file
    # descriptor into a Python file object and record our PID in it
    f = os.fdopen(fd, "w")
    f.write("{0}\n".format(os.getpid()))
    f.close()

    return LockFile(lockfile)
