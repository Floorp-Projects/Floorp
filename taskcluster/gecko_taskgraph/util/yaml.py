# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os

from yaml.loader import SafeLoader


class UnicodeLoader(SafeLoader):
    def construct_yaml_str(self, node):
        return self.construct_scalar(node)


UnicodeLoader.add_constructor("tag:yaml.org,2002:str", UnicodeLoader.construct_yaml_str)


def load_stream(stream):
    """
    Parse the first YAML document in a stream
    and produce the corresponding Python object.
    """
    loader = UnicodeLoader(stream)
    try:
        return loader.get_single_data()
    finally:
        loader.dispose()


def load_yaml(*parts):
    """Convenience function to load a YAML file in the given path.  This is
    useful for loading kind configuration files from the kind path."""
    filename = os.path.join(*parts)
    with open(filename, "rb") as f:
        return load_stream(f)
