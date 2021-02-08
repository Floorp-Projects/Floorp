# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozlint import result
from mozlint.pathutils import expand_exclusions

results = []


def lint(paths, config, fix=None, **lintargs):
    files = list(expand_exclusions(paths, config, lintargs["root"]))
    log = lintargs["log"]
    fixed = 0

    for f in files:
        with open(f, "rb") as open_file:
            hasFix = False
            content_to_write = []

            try:
                lines = open_file.readlines()
                # Check for Empty spaces or newline character at end of file
                if lines[:].__len__() != 0 and lines[-1:][0].strip().__len__() == 0:
                    # return file pointer to first
                    open_file.seek(0)
                    if fix:
                        fixed += 1
                        # fix Empty lines at end of file
                        for i, line in reversed(list(enumerate(open_file))):
                            # determine if line is empty
                            if line.strip() != b"":
                                with open(f, "wb") as write_file:
                                    # determine if file's last line have \n, if not then add a \n
                                    if not lines[i].endswith(b"\n"):
                                        lines[i] = lines[i] + b"\n"
                                    # write content to file
                                    for e in lines[: i + 1]:
                                        write_file.write(e)
                                # end the loop
                                break
                    else:
                        res = {
                            "path": f,
                            "message": "Empty Lines at end of file",
                            "level": "error",
                            "lineno": open_file.readlines()[:].__len__(),
                        }
                        results.append(result.from_config(config, **res))
            except Exception as ex:
                log.debug("Error: " + str(ex) + ", in file: " + f)

            # return file pointer to first
            open_file.seek(0)

            lines = open_file.readlines()
            # Detect missing newline at the end of the file
            if lines[:].__len__() != 0 and not lines[-1].endswith(b"\n"):
                if fix:
                    fixed += 1
                    with open(f, "wb") as write_file:
                        # add a newline character at end of file
                        lines[-1] = lines[-1] + b"\n"
                        # write content to file
                        for e in lines:
                            write_file.write(e)
                else:
                    res = {
                        "path": f,
                        "message": "File does not end with newline character",
                        "level": "error",
                        "lineno": lines.__len__(),
                    }
                    results.append(result.from_config(config, **res))

            # return file pointer to first
            open_file.seek(0)

            for i, line in enumerate(open_file):
                if line.endswith(b" \n"):
                    # We found a trailing whitespace
                    if fix:
                        # We want to fix it, strip the trailing spaces
                        content_to_write.append(line.rstrip() + b"\n")
                        fixed += 1
                        hasFix = True
                    else:
                        res = {
                            "path": f,
                            "message": "Trailing whitespace",
                            "level": "error",
                            "lineno": i + 1,
                        }
                        results.append(result.from_config(config, **res))
                else:
                    if fix:
                        content_to_write.append(line)
            if hasFix:
                # Only update the file when we found a change to make
                with open(f, "wb") as open_file_to_write:
                    open_file_to_write.write(b"".join(content_to_write))

            # We are still using the same fp, let's return to the first
            # line
            open_file.seek(0)
            # Open it as once as we just need to know if there is
            # at least one \r\n
            content = open_file.read()

            if b"\r\n" in content:
                if fix:
                    fixed += 1
                    content = content.replace(b"\r\n", b"\n")
                    with open(f, "wb") as open_file_to_write:
                        open_file_to_write.write(content)
                else:
                    res = {
                        "path": f,
                        "message": "Windows line return",
                        "level": "error",
                    }
                    results.append(result.from_config(config, **res))

    return {"results": results, "fixed": fixed}
