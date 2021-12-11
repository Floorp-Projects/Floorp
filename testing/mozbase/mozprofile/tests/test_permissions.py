#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import sqlite3

import mozunit
import pytest

from mozprofile.permissions import Permissions

LOCATIONS = """http://mochi.test:8888  primary,privileged
http://127.0.0.1:80             noxul
http://127.0.0.1:8888           privileged
"""


@pytest.fixture
def locations_file(tmpdir):
    locations_file = tmpdir.join("locations.txt")
    locations_file.write(LOCATIONS)
    return locations_file.strpath


@pytest.fixture
def perms(tmpdir, locations_file):
    return Permissions(tmpdir.mkdir("profile").strpath, locations_file)


def test_create_permissions_db(perms):
    profile_dir = perms._profileDir
    perms_db_filename = os.path.join(profile_dir, "permissions.sqlite")

    select_stmt = "select origin, type, permission from moz_hosts"

    con = sqlite3.connect(perms_db_filename)
    cur = con.cursor()
    cur.execute(select_stmt)
    entries = cur.fetchall()

    assert len(entries) == 3

    assert entries[0][0] == "http://mochi.test:8888"
    assert entries[0][1] == "allowXULXBL"
    assert entries[0][2] == 1

    assert entries[1][0] == "http://127.0.0.1"
    assert entries[1][1] == "allowXULXBL"
    assert entries[1][2] == 2

    assert entries[2][0] == "http://127.0.0.1:8888"
    assert entries[2][1] == "allowXULXBL"
    assert entries[2][2] == 1

    perms._locations.add_host("a.b.c", port="8081", scheme="https", options="noxul")

    cur.execute(select_stmt)
    entries = cur.fetchall()

    assert len(entries) == 4
    assert entries[3][0] == "https://a.b.c:8081"
    assert entries[3][1] == "allowXULXBL"
    assert entries[3][2] == 2

    # when creating a DB we should default to user_version==5
    cur.execute("PRAGMA user_version")
    entries = cur.fetchall()
    assert entries[0][0] == 5

    perms.clean_db()
    # table should be removed
    cur.execute("select * from sqlite_master where type='table'")
    entries = cur.fetchall()
    assert len(entries) == 0


def test_nw_prefs(perms):
    prefs, user_prefs = perms.network_prefs(False)

    assert len(user_prefs) == 0
    assert len(prefs) == 0

    prefs, user_prefs = perms.network_prefs(True)
    assert len(user_prefs) == 2
    assert user_prefs[0] == ("network.proxy.type", 2)
    assert user_prefs[1][0] == "network.proxy.autoconfig_url"

    origins_decl = (
        "var knownOrigins = (function () {  return ['http://mochi.test:8888', "
        "'http://127.0.0.1:80', 'http://127.0.0.1:8888'].reduce"
    )
    assert origins_decl in user_prefs[1][1]

    proxy_check = (
        "'http': 'PROXY mochi.test:8888'",
        "'https': 'PROXY mochi.test:4443'",
        "'ws': 'PROXY mochi.test:4443'",
        "'wss': 'PROXY mochi.test:4443'",
    )
    assert all(c in user_prefs[1][1] for c in proxy_check)


@pytest.fixture
def perms_db_filename(tmpdir):
    return tmpdir.join("permissions.sqlite").strpath


@pytest.fixture
def permDB(perms_db_filename):
    permDB = sqlite3.connect(perms_db_filename)
    yield permDB
    permDB.cursor().close()


# pylint: disable=W1638
@pytest.fixture(params=range(2, 6))
def version(request, perms_db_filename, permDB, locations_file):
    version = request.param

    cursor = permDB.cursor()
    cursor.execute("PRAGMA user_version=%d;" % version)

    if version == 5:
        cursor.execute(
            """CREATE TABLE IF NOT EXISTS moz_hosts (
          id INTEGER PRIMARY KEY,
          origin TEXT,
          type TEXT,
          permission INTEGER,
          expireType INTEGER,
          expireTime INTEGER,
          modificationTime INTEGER)"""
        )
    elif version == 4:
        cursor.execute(
            """CREATE TABLE IF NOT EXISTS moz_hosts (
           id INTEGER PRIMARY KEY,
           host TEXT,
           type TEXT,
           permission INTEGER,
           expireType INTEGER,
           expireTime INTEGER,
           modificationTime INTEGER,
           appId INTEGER,
           isInBrowserElement INTEGER)"""
        )
    elif version == 3:
        cursor.execute(
            """CREATE TABLE IF NOT EXISTS moz_hosts (
           id INTEGER PRIMARY KEY,
           host TEXT,
           type TEXT,
           permission INTEGER,
           expireType INTEGER,
           expireTime INTEGER,
           appId INTEGER,
           isInBrowserElement INTEGER)"""
        )
    elif version == 2:
        cursor.execute(
            """CREATE TABLE IF NOT EXISTS moz_hosts (
           id INTEGER PRIMARY KEY,
           host TEXT,
           type TEXT,
           permission INTEGER,
           expireType INTEGER,
           expireTime INTEGER)"""
        )
    else:
        raise Exception("version must be 2, 3, 4 or 5")
    permDB.commit()

    # Create a permissions object to read the db
    Permissions(os.path.dirname(perms_db_filename), locations_file)
    return version


def test_verify_user_version(version, permDB):
    """Verifies that we call INSERT statements using the correct number
    of columns for existing databases.
    """
    select_stmt = "select * from moz_hosts"

    cur = permDB.cursor()
    cur.execute(select_stmt)
    entries = cur.fetchall()

    assert len(entries) == 3

    columns = {
        1: 6,
        2: 6,
        3: 8,
        4: 9,
        5: 7,
    }[version]

    assert len(entries[0]) == columns
    for x in range(4, columns):
        assert entries[0][x] == 0


def test_schema_version(perms, locations_file):
    profile_dir = perms._profileDir
    perms_db_filename = os.path.join(profile_dir, "permissions.sqlite")
    perms.write_db(open(locations_file, "w+b"))

    stmt = "PRAGMA user_version;"

    con = sqlite3.connect(perms_db_filename)
    cur = con.cursor()
    cur.execute(stmt)
    entries = cur.fetchall()

    schema_version = entries[0][0]
    assert schema_version == 5


if __name__ == "__main__":
    mozunit.main()
