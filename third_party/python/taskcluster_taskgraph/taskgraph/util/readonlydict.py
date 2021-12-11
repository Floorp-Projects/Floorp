# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# Imported from
# https://searchfox.org/mozilla-central/rev/c3ebaf6de2d481c262c04bb9657eaf76bf47e2ac/python/mozbuild/mozbuild/util.py#115-127


class ReadOnlyDict(dict):
    """A read-only dictionary."""

    def __init__(self, *args, **kwargs):
        dict.__init__(self, *args, **kwargs)

    def __delitem__(self, key):
        raise Exception("Object does not support deletion.")

    def __setitem__(self, key, value):
        raise Exception("Object does not support assignment.")

    def update(self, *args, **kwargs):
        raise Exception("Object does not support update.")
