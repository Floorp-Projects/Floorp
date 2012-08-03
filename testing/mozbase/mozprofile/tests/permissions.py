#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
try:
    import sqlite3
except ImportError:
    from pysqlite2 import dbapi2 as sqlite3
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
        self.locations_file = tempfile.NamedTemporaryFile()
        self.locations_file.write(self.locations)
        self.locations_file.flush()

    def tearDown(self):
        if self.profile_dir:
            shutil.rmtree(self.profile_dir)
        if self.locations_file:
            self.locations_file.close()

    def test_permissions_db(self):
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

        perms.clean_db()
        # table should be removed
        cur.execute("select * from sqlite_master where type='table'")
        entries = cur.fetchall()
        self.assertEqual(len(entries), 0)
        
    def test_nw_prefs(self):
        perms = Permissions(self.profile_dir, self.locations_file.name)

        prefs, user_prefs = perms.network_prefs(False)

        self.assertEqual(len(user_prefs), 0)
        self.assertEqual(len(prefs), 6)

        self.assertEqual(prefs[0], ('capability.principal.codebase.p1.granted',
                                    'UniversalXPConnect'))
        self.assertEqual(prefs[1], ('capability.principal.codebase.p1.id',
                                    'http://mochi.test:8888'))
        self.assertEqual(prefs[2], ('capability.principal.codebase.p1.subjectName', ''))

        self.assertEqual(prefs[3], ('capability.principal.codebase.p2.granted',
                                    'UniversalXPConnect'))
        self.assertEqual(prefs[4], ('capability.principal.codebase.p2.id',
                                    'http://127.0.0.1:8888'))
        self.assertEqual(prefs[5], ('capability.principal.codebase.p2.subjectName', ''))


        prefs, user_prefs = perms.network_prefs(True)
        self.assertEqual(len(user_prefs), 2)
        self.assertEqual(user_prefs[0], ('network.proxy.type', 2))
        self.assertEqual(user_prefs[1][0], 'network.proxy.autoconfig_url')

        origins_decl = "var origins = ['http://mochi.test:8888', 'http://127.0.0.1:80', 'http://127.0.0.1:8888'];"
        self.assertTrue(origins_decl in user_prefs[1][1])

        proxy_check = "if (isHttp || isHttps || isWebSocket || isWebSocketSSL)    return 'PROXY mochi.test:8888';"
        self.assertTrue(proxy_check in user_prefs[1][1])


if __name__ == '__main__':
    unittest.main()
