# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
""" Manages a metadata file.
"""
import os
import datetime
import collections
import json

from condprof.util import LOG


METADATA_NAME = "condprofile.json"


class Metadata(collections.MutableMapping):
    """ dict-like class that holds metadata for a profile.
    """

    def __init__(self, profile_dir):
        self.metadata_file = os.path.join(profile_dir, METADATA_NAME)
        LOG("Reading existing metadata at %s" % self.metadata_file)
        if not os.path.exists(self.metadata_file):
            LOG("Could not find the metadata file in that profile")
            self._data = {}
        else:
            with open(self.metadata_file) as f:
                self._data = json.loads(f.read())

    def __getitem__(self, key):
        return self._data[self.__keytransform__(key)]

    def __setitem__(self, key, value):
        self._data[self.__keytransform__(key)] = value

    def __delitem__(self, key):
        del self._data[self.__keytransform__(key)]

    def __iter__(self):
        return iter(self._data)

    def __len__(self):
        return len(self._data)

    def __keytransform__(self, key):
        return key

    def _days2age(self, days):
        if days < 7:
            return "days"
        if days < 30:
            return "weeks"
        if days < 30 * 6:
            return "months"
        return "old"  # :)

    def _delta(self, created, updated):
        created = created[:26]
        updated = updated[:26]
        # tz..
        format = "%Y-%m-%d %H:%M:%S.%f"
        created = datetime.datetime.strptime(created, format)
        updated = datetime.datetime.strptime(updated, format)
        delta = created - updated
        return delta.days

    def write(self, **extras):
        # writing metadata
        LOG("Creating metadata...")
        self._data.update(**extras)
        ts = str(datetime.datetime.now())
        if "created" not in self._data:
            self._data["created"] = ts
        self._data["updated"] = ts
        # XXX need android arch version here
        days = self._delta(self._data["created"], self._data["updated"])
        self._data["days"] = days
        self._data["age"] = self._days2age(days)
        # adding info about the firefox version
        # XXX build ID ??
        # XXX android ??
        LOG("Saving metadata file in %s" % self.metadata_file)
        with open(self.metadata_file, "w") as f:
            f.write(json.dumps(self._data))
