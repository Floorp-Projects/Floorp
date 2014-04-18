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

    profile_dir = None
    locations_file = None

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

    def write_perm_db(self, version=3):
        permDB = sqlite3.connect(os.path.join(self.profile_dir, "permissions.sqlite"))
        cursor = permDB.cursor()

        cursor.execute("PRAGMA user_version=%d;" % version)

        if version == 3:
            cursor.execute("""CREATE TABLE IF NOT EXISTS moz_hosts (
               id INTEGER PRIMARY KEY,
               host TEXT,
               type TEXT,
               permission INTEGER,
               expireType INTEGER,
               expireTime INTEGER,
               appId INTEGER,
               isInBrowserElement INTEGER)""")
        elif version == 2:
            cursor.execute("""CREATE TABLE IF NOT EXISTS moz_hosts (
               id INTEGER PRIMARY KEY,
               host TEXT,
               type TEXT,
               permission INTEGER,
               expireType INTEGER,
               expireTime INTEGER)""")
        else:
            raise Exception("version must be 2 or 3")

        permDB.commit()
        cursor.close()

    def test_create_permissions_db(self):
        perms = Permissions(self.profile_dir, self.locations_file.name)
        perms_db_filename = os.path.join(self.profile_dir, 'permissions.sqlite')

        select_stmt = 'select host, type, permission from moz_hosts'

        con = sqlite3.connect(perms_db_filename)
        cur = con.cursor()
        cur.execute(select_stmt)
        entries = cur.fetchall()

        self.assertEqual(len(entries), 3)

        self.assertEqual(entries[0][0], 'mochi.test')
        self.assertEqual(entries[0][1], 'allowXULXBL')
        self.assertEqual(entries[0][2], 1)

        self.assertEqual(entries[1][0], '127.0.0.1')
        self.assertEqual(entries[1][1], 'allowXULXBL')
        self.assertEqual(entries[1][2], 2)

        self.assertEqual(entries[2][0], '127.0.0.1')
        self.assertEqual(entries[2][1], 'allowXULXBL')
        self.assertEqual(entries[2][2], 1)

        perms._locations.add_host('a.b.c', options='noxul')

        cur.execute(select_stmt)
        entries = cur.fetchall()

        self.assertEqual(len(entries), 4)
        self.assertEqual(entries[3][0], 'a.b.c')
        self.assertEqual(entries[3][1], 'allowXULXBL')
        self.assertEqual(entries[3][2], 2)

        # when creating a DB we should default to user_version==2
        cur.execute('PRAGMA user_version')
        entries = cur.fetchall()
        self.assertEqual(entries[0][0], 2)

        perms.clean_db()
        # table should be removed
        cur.execute("select * from sqlite_master where type='table'")
        entries = cur.fetchall()
        self.assertEqual(len(entries), 0)

    def test_nw_prefs(self):
        perms = Permissions(self.profile_dir, self.locations_file.name)

        prefs, user_prefs = perms.network_prefs(False)

        self.assertEqual(len(user_prefs), 0)
        self.assertEqual(len(prefs), 0)

        prefs, user_prefs = perms.network_prefs(True)
        self.assertEqual(len(user_prefs), 2)
        self.assertEqual(user_prefs[0], ('network.proxy.type', 2))
        self.assertEqual(user_prefs[1][0], 'network.proxy.autoconfig_url')

        origins_decl = "var knownOrigins = (function () {  return ['http://mochi.test:8888', 'http://127.0.0.1:80', 'http://127.0.0.1:8888'].reduce"
        self.assertTrue(origins_decl in user_prefs[1][1])

        proxy_check = ("'http': 'PROXY mochi.test:8888'",
                       "'https': 'PROXY mochi.test:4443'",
                       "'ws': 'PROXY mochi.test:4443'",
                       "'wss': 'PROXY mochi.test:4443'")
        self.assertTrue(all(c in user_prefs[1][1] for c in proxy_check))

    def verify_user_version(self, version):
        """Verifies that we call INSERT statements using the correct number
        of columns for existing databases.
        """
        self.write_perm_db(version=version)
        Permissions(self.profile_dir, self.locations_file.name)
        perms_db_filename = os.path.join(self.profile_dir, 'permissions.sqlite')

        select_stmt = 'select * from moz_hosts'

        con = sqlite3.connect(perms_db_filename)
        cur = con.cursor()
        cur.execute(select_stmt)
        entries = cur.fetchall()

        self.assertEqual(len(entries), 3)

        columns = 8 if version == 3 else 6
        self.assertEqual(len(entries[0]), columns)
        for x in range(4, columns):
            self.assertEqual(entries[0][x], 0)

    def test_existing_permissions_db_v2(self):
        self.verify_user_version(2)

    def test_existing_permissions_db_v3(self):
        self.verify_user_version(3)


if __name__ == '__main__':
    unittest.main()
