# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This script takes a log from the replace-malloc logalloc library on stdin
and munges it so that it can be used with the logalloc-replay tool.

Given the following output:
  13663 malloc(42)=0x7f0c33502040
  13663 malloc(24)=0x7f0c33503040
  13663 free(0x7f0c33502040)
The resulting output is:
  1 malloc(42)=#1
  1 malloc(24)=#2
  1 free(#1)

See README for more details.
"""

from __future__ import print_function
import sys
from collections import (
    defaultdict,
    deque,
)

class IdMapping(object):
    """Class to map values to ids.

    Each value is associated to an increasing id, starting from 1.
    When a value is removed, its id is recycled and will be reused for
    subsequent values.
    """
    def __init__(self):
        self.id = 1
        self._values = {}
        self._recycle = deque()

    def __getitem__(self, value):
        if value not in self._values:
            if self._recycle:
                self._values[value] = self._recycle.popleft()
            else:
                self._values[value] = self.id
                self.id += 1
        return self._values[value]

    def __delitem__(self, value):
        if value == 0:
            return
        self._recycle.append(self._values[value])
        del self._values[value]

    def __contains__(self, value):
        return value == 0 or value in self._values


class Ignored(Exception): pass


def split_log_line(line):
    try:
        # The format for each line is:
        # <pid> [<tid>] <function>([<args>])[=<result>]
        #
        # The original format didn't include the tid, so we try to parse
        # lines whether they have one or not.
        pid, func_call = line.split(' ', 1)
        call, result = func_call.split(')')
        func, args = call.split('(')
        args = args.split(',') if args else []
        if result:
            if result[0] != '=':
                raise Ignored('Malformed input')
            result = result[1:]
        if ' ' in func:
            tid, func = func.split(' ', 1)
        else:
            tid = pid
        return pid, tid, func, args, result
    except:
        raise Ignored('Malformed input')


NUM_ARGUMENTS = {
    'jemalloc_stats': 0,
    'free': 1,
    'malloc': 1,
    'posix_memalign': 2,
    'aligned_alloc': 2,
    'calloc': 2,
    'realloc': 2,
    'memalign': 2,
    'valloc': 1,
}


def main():
    pids = IdMapping()
    processes = defaultdict(lambda: { 'pointers': IdMapping(),
                                      'tids': IdMapping() })
    for line in sys.stdin:
        line = line.strip()

        try:
            pid, tid, func, args, result = split_log_line(line)

            # Replace pid with an id.
            pid = pids[int(pid)]

            process = processes[pid]
            tid = process['tids'][int(tid)]

            pointers = process['pointers']

            if func not in NUM_ARGUMENTS:
                raise Ignored('Unknown function')

            if len(args) != NUM_ARGUMENTS[func]:
                raise Ignored('Malformed input')

            if func in ('jemalloc_stats', 'free') and result:
                raise Ignored('Malformed input')

            if func in ('free', 'realloc'):
                ptr = int(args[0], 16)
                if ptr and ptr not in pointers:
                    raise Ignored('Did not see an alloc for pointer')
                args[0] = "#%d" % pointers[ptr]
                del pointers[ptr]

            if result:
                result = int(result, 16)
                if not result:
                    raise Ignored('Result is NULL')
                result = "#%d" % pointers[result]

            print('%d %d %s(%s)%s' % (pid, tid, func, ','.join(args),
                '=%s' % result if result else ''))

        except Exception as e:
            print('Ignored "%s": %s' % (line, e.message), file=sys.stderr)


if __name__ == '__main__':
    main()
