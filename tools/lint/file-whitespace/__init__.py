# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from mozlint import result
from mozlint.pathutils import expand_exclusions

results = []


def lint(paths, config, fix=None, **lintargs):
    files = list(expand_exclusions(paths, config, lintargs['root']))

    for f in files:
        with open(f, 'rb') as open_file:
            hasFix = False
            content_to_write = []
            for i, line in enumerate(open_file):
                if line.endswith(" \n"):
                    # We found a trailing whitespace
                    if fix:
                        # We want to fix it, strip the trailing spaces
                        content_to_write.append(line.rstrip() + "\n")
                        hasFix = True
                    else:
                        res = {'path': f,
                               'message': "Trailing whitespace",
                               'level': 'error'
                               }
                        results.append(result.from_config(config, **res))
                else:
                    if fix:
                        content_to_write.append(line)
            if hasFix:
                # Only update the file when we found a change to make
                with open(f, 'wb') as open_file_to_write:
                    open_file_to_write.write("".join(content_to_write))

            # We are still using the same fp, let's return to the first
            # line
            open_file.seek(0)
            # Open it as once as we just need to know if there is
            # at least one \r\n
            content = open_file.read()

            if "\r\n" in content:
                if fix:
                    # replace \r\n by \n
                    content = content.replace(b'\r\n', b'\n')
                    with open(f, 'wb') as open_file_to_write:
                        open_file_to_write.write(content)
                else:
                    res = {'path': f,
                           'message': "Windows line return",
                           'level': 'error'
                           }
                    results.append(result.from_config(config, **res))

    return results
