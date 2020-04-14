# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import json


def open_file(path):
    """Opens a file and returns its contents.

    :param path str: Path to the file, if it's a
        JSON, then a dict will be returned, otherwise,
        the raw contents (not split by line) will be
        returned.
    :return dict/str: Returns a dict for JSON data, and
        a str for any other type.
    """
    print("Reading %s" % path)
    with open(path) as f:
        if os.path.splitext(path)[-1] == ".json":
            return json.load(f)
        return f.read()


def write_json(data, path, file):
    """Writes data to a JSON file.

    :param data dict: Data to write.
    :param path str: Directory of where the data will be stored.
    :param file str: Name of the JSON file.

    Returns the path of the file.
    """
    path = os.path.join(path, file)
    with open(path, "w+") as f:
        json.dump(data, f)
    return path
