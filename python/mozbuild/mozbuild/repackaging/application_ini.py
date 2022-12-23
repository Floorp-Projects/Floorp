# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from mozpack.files import FileFinder
from six import string_types
from six.moves import configparser


def get_application_ini_value(
    finder_or_application_directory, section, value, fallback=None
):
    """Find string with given `section` and `value` in any `application.ini`
    under given directory or finder.

    If string is not found and `fallback` is given, find string with given
    `section` and `fallback` instead.

    Raises an `Exception` if no string is found."""

    return next(
        get_application_ini_values(
            finder_or_application_directory,
            dict(section=section, value=value, fallback=fallback),
        )
    )


def get_application_ini_values(finder_or_application_directory, *args):
    """Find multiple strings for given `section` and `value` pairs.
    Additional `args` should be dictionaries with keys `section`, `value`,
    and optional `fallback`.  Returns an iterable of strings, one for each
    dictionary provided.

    `fallback` is treated as with `get_application_ini_value`.

    Raises an `Exception` if any string is not found."""

    if isinstance(finder_or_application_directory, string_types):
        finder = FileFinder(finder_or_application_directory)
    else:
        finder = finder_or_application_directory

    # Packages usually have a top-level `firefox/` directory; search below it.
    for p, f in finder.find("**/application.ini"):
        data = f.open().read().decode("utf-8")
        parser = configparser.ConfigParser()
        parser.read_string(data)

        for d in args:
            rc = None
            try:
                rc = parser.get(d["section"], d["value"])
            except configparser.NoOptionError:
                if "fallback" not in d:
                    raise
                else:
                    rc = parser.get(d["section"], d["fallback"])

            if rc is None:
                raise Exception("Input does not contain an application.ini file")

            yield rc

        # Process only the first `application.ini`.
        break
