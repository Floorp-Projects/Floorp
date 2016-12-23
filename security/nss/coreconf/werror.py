#!/usr/bin/env python

import os
import subprocess

def main():
    cc = os.environ.get('CC', 'cc')
    sink = open(os.devnull, 'wb')
    try:
        cc_is_clang = 'clang' in subprocess.check_output(
          [cc, '--version'], universal_newlines=True, stderr=sink)
    except OSError:
        # We probably just don't have CC/cc.
        return

    def warning_supported(warning):
        return subprocess.call([cc, '-x', 'c', '-E', '-Werror',
                                '-W%s' % warning, os.devnull], stdout=sink, stderr=sink) == 0
    def can_enable():
        # This would be a problem
        if not warning_supported('all'):
            return False

        # If we aren't clang, make sure we have gcc 4.8 at least
        if not cc_is_clang:
            try:
                v = subprocess.check_output([cc, '-dumpversion'], stderr=sink)
                v = v.strip(' \r\n').split('.')
                v = list(map(int, v))
                if v[0] < 4 or (v[0] == 4 and v[1] < 8):
                    # gcc 4.8 minimum
                    return False
            except OSError:
                return False
        return True

    if not can_enable():
        print('-DNSS_NO_GCC48')
        return

    print('-Werror')
    print('-Wall')

    def set_warning(warning, contra=''):
        if warning_supported(warning):
            print('-W%s%s' % (contra, warning))

    if cc_is_clang:
        # clang is unable to handle glib's expansion of strcmp and similar for
        # optimized builds, so disable the resulting errors.
        # See https://llvm.org/bugs/show_bug.cgi?id=20144
        for w in ['array-bounds', 'unevaluated-expression',
                  'parentheses-equality']:
            set_warning(w, 'no-')
        print('-Qunused-arguments')

    # set_warning('shadow') # Bug 1309068

if __name__ == '__main__':
    main()
