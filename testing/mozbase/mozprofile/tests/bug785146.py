#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import mozfile
import os
import shutil
import sqlite3
import tempfile
import unittest
from mozprofile.permissions import Permissions

class PermissionsTest(unittest.TestCase):

    locations = """http://mochi.test:8888  primary,privileged
http://127.0.0.1:80             noxul
http://127.0.0.1:8888           privileged
"""

    def setUp(self):
        self.profile_dir = tempfile.mkdtemp()
        self.locations_file = mozfile.NamedTemporaryFile()
        self.locations_file.write(self.locations)
        self.locations_file.flush()

    def tearDown(self):
        if self.profile_dir:
            shutil.rmtree(self.profile_dir)
        if self.locations_file:
            self.locations_file.close()

    def test_schema_version(self):
        perms = Permissions(self.profile_dir, self.locations_file.name)
        perms_db_filename = os.path.join(self.profile_dir, 'permissions.sqlite')
        perms.write_db(self.locations_file)

        stmt = 'PRAGMA user_version;'

        con = sqlite3.connect(perms_db_filename)
        cur = con.cursor()
        cur.execute(stmt)
        entries = cur.fetchall()

        schema_version = entries[0][0]
        self.assertEqual(schema_version, 5)

if __name__ == '__main__':
    unittest.main()
