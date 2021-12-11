# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.


"""
add permissions to the profile
"""

from __future__ import absolute_import

import codecs
import os
import sqlite3

from six import string_types
from six.moves.urllib import parse
import six

__all__ = [
    "MissingPrimaryLocationError",
    "MultiplePrimaryLocationsError",
    "DEFAULT_PORTS",
    "DuplicateLocationError",
    "BadPortLocationError",
    "LocationsSyntaxError",
    "Location",
    "ServerLocations",
    "Permissions",
]

# http://hg.mozilla.org/mozilla-central/file/b871dfb2186f/build/automation.py.in#l28
DEFAULT_PORTS = {"http": "8888", "https": "4443", "ws": "4443", "wss": "4443"}


class LocationError(Exception):
    """Signifies an improperly formed location."""

    def __str__(self):
        s = "Bad location"
        m = str(Exception.__str__(self))
        if m:
            s += ": %s" % m
        return s


class MissingPrimaryLocationError(LocationError):
    """No primary location defined in locations file."""

    def __init__(self):
        LocationError.__init__(self, "missing primary location")


class MultiplePrimaryLocationsError(LocationError):
    """More than one primary location defined."""

    def __init__(self):
        LocationError.__init__(self, "multiple primary locations")


class DuplicateLocationError(LocationError):
    """Same location defined twice."""

    def __init__(self, url):
        LocationError.__init__(self, "duplicate location: %s" % url)


class BadPortLocationError(LocationError):
    """Location has invalid port value."""

    def __init__(self, given_port):
        LocationError.__init__(self, "bad value for port: %s" % given_port)


class LocationsSyntaxError(Exception):
    """Signifies a syntax error on a particular line in server-locations.txt."""

    def __init__(self, lineno, err=None):
        self.err = err
        self.lineno = lineno

    def __str__(self):
        s = "Syntax error on line %s" % self.lineno
        if self.err:
            s += ": %s." % self.err
        else:
            s += "."
        return s


class Location(object):
    """Represents a location line in server-locations.txt."""

    attrs = ("scheme", "host", "port")

    def __init__(self, scheme, host, port, options):
        for attr in self.attrs:
            setattr(self, attr, locals()[attr])
        self.options = options
        try:
            int(self.port)
        except ValueError:
            raise BadPortLocationError(self.port)

    def isEqual(self, location):
        """compare scheme://host:port, but ignore options"""
        return len(
            [i for i in self.attrs if getattr(self, i) == getattr(location, i)]
        ) == len(self.attrs)

    __eq__ = isEqual

    def __hash__(self):
        # pylint --py3k: W1641
        return hash(tuple(getattr(attr) for attr in self.attrs))

    def url(self):
        return "%s://%s:%s" % (self.scheme, self.host, self.port)

    def __str__(self):
        return "%s  %s" % (self.url(), ",".join(self.options))


class ServerLocations(object):
    """Iterable collection of locations.
    Use provided functions to add new locations, rather that manipulating
    _locations directly, in order to check for errors and to ensure the
    callback is called, if given.
    """

    def __init__(self, filename=None, add_callback=None):
        self.add_callback = add_callback
        self._locations = []
        self.hasPrimary = False
        if filename:
            self.read(filename)

    def __iter__(self):
        return self._locations.__iter__()

    def __len__(self):
        return len(self._locations)

    def add(self, location, suppress_callback=False):
        if "primary" in location.options:
            if self.hasPrimary:
                raise MultiplePrimaryLocationsError()
            self.hasPrimary = True

        self._locations.append(location)
        if self.add_callback and not suppress_callback:
            self.add_callback([location])

    def add_host(self, host, port="80", scheme="http", options="privileged"):
        if isinstance(options, string_types):
            options = options.split(",")
        self.add(Location(scheme, host, port, options))

    def read(self, filename, check_for_primary=True):
        """
        Reads the file and adds all valid locations to the ``self._locations`` array.

        :param filename: in the format of server-locations.txt_
        :param check_for_primary: if True, a ``MissingPrimaryLocationError`` exception is raised
          if no primary is found

        .. _server-locations.txt: http://searchfox.org/mozilla-central/source/build/pgo/server-locations.txt # noqa

        The only exception is that the port, if not defined, defaults to 80 or 443.

        FIXME: Shouldn't this default to the protocol-appropriate port?  Is
        there any reason to have defaults at all?
        """

        locationFile = codecs.open(filename, "r", "UTF-8")
        lineno = 0
        new_locations = []

        for line in locationFile:
            line = line.strip()
            lineno += 1

            # check for comments and blank lines
            if line.startswith("#") or not line:
                continue

            # split the server from the options
            try:
                server, options = line.rsplit(None, 1)
                options = options.split(",")
            except ValueError:
                server = line
                options = []

            # parse the server url
            if "://" not in server:
                server = "http://" + server
            scheme, netloc, path, query, fragment = parse.urlsplit(server)
            # get the host and port
            try:
                host, port = netloc.rsplit(":", 1)
            except ValueError:
                host = netloc
                port = DEFAULT_PORTS.get(scheme, "80")

            try:
                location = Location(scheme, host, port, options)
                self.add(location, suppress_callback=True)
            except LocationError as e:
                raise LocationsSyntaxError(lineno, e)

            new_locations.append(location)

        # ensure that a primary is found
        if check_for_primary and not self.hasPrimary:
            raise LocationsSyntaxError(lineno + 1, MissingPrimaryLocationError())

        if self.add_callback:
            self.add_callback(new_locations)


class Permissions(object):
    """Allows handling of permissions for ``mozprofile``"""

    def __init__(self, profileDir, locations=None):
        self._profileDir = profileDir
        self._locations = ServerLocations(add_callback=self.write_db)
        if locations:
            if isinstance(locations, ServerLocations):
                self._locations = locations
                self._locations.add_callback = self.write_db
                self.write_db(self._locations._locations)
            elif isinstance(locations, list):
                for l in locations:
                    self._locations.add_host(**l)
            elif isinstance(locations, dict):
                self._locations.add_host(**locations)
            elif os.path.exists(locations):
                self._locations.read(locations)

    def write_db(self, locations):
        """write permissions to the sqlite database"""

        # Open database and create table
        permDB = sqlite3.connect(os.path.join(self._profileDir, "permissions.sqlite"))
        cursor = permDB.cursor()

        # SQL copied from
        # http://searchfox.org/mozilla-central/source/extensions/permissions/PermissionManager.cpp
        cursor.execute(
            """CREATE TABLE IF NOT EXISTS moz_hosts (
              id INTEGER PRIMARY KEY
             ,origin TEXT
             ,type TEXT
             ,permission INTEGER
             ,expireType INTEGER
             ,expireTime INTEGER
             ,modificationTime INTEGER
           )"""
        )

        rows = cursor.execute("PRAGMA table_info(moz_hosts)")
        count = len(rows.fetchall())

        using_origin = False
        # if the db contains 7 columns, we're using user_version 5
        if count == 7:
            statement = "INSERT INTO moz_hosts values(NULL, ?, ?, ?, 0, 0, 0)"
            cursor.execute("PRAGMA user_version=5;")
            using_origin = True
        # if the db contains 9 columns, we're using user_version 4
        elif count == 9:
            statement = "INSERT INTO moz_hosts values(NULL, ?, ?, ?, 0, 0, 0, 0, 0)"
            cursor.execute("PRAGMA user_version=4;")
        # if the db contains 8 columns, we're using user_version 3
        elif count == 8:
            statement = "INSERT INTO moz_hosts values(NULL, ?, ?, ?, 0, 0, 0, 0)"
            cursor.execute("PRAGMA user_version=3;")
        else:
            statement = "INSERT INTO moz_hosts values(NULL, ?, ?, ?, 0, 0)"
            cursor.execute("PRAGMA user_version=2;")

        for location in locations:
            # set the permissions
            permissions = {"allowXULXBL": "noxul" not in location.options}
            for perm, allow in six.iteritems(permissions):
                if allow:
                    permission_type = 1
                else:
                    permission_type = 2

                if using_origin:
                    # This is a crude approximation of the origin generation
                    # logic from ContentPrincipal and nsStandardURL. It should
                    # suffice for the permissions which the test runners will
                    # want to insert into the system.
                    origin = location.scheme + "://" + location.host
                    if (location.scheme != "http" or location.port != "80") and (
                        location.scheme != "https" or location.port != "443"
                    ):
                        origin += ":" + str(location.port)

                    cursor.execute(statement, (origin, perm, permission_type))
                else:
                    # The database is still using a legacy system based on hosts
                    # We can insert the permission as a host
                    #
                    # XXX This codepath should not be hit, as tests are run with
                    # fresh profiles. However, if it was hit, permissions would
                    # not be added to the database correctly (bug 1183185).
                    cursor.execute(statement, (location.host, perm, permission_type))

        # Commit and close
        permDB.commit()
        cursor.close()

    def network_prefs(self, proxy=None):
        """
        take known locations and generate preferences to handle permissions and proxy
        returns a tuple of prefs, user_prefs
        """

        prefs = []

        if proxy:
            user_prefs = self.pac_prefs(proxy)
        else:
            user_prefs = []

        return prefs, user_prefs

    def pac_prefs(self, user_proxy=None):
        """
        return preferences for Proxy Auto Config.
        """
        proxy = DEFAULT_PORTS.copy()

        # We need to proxy every server but the primary one.
        origins = ["'%s'" % l.url() for l in self._locations]
        origins = ", ".join(origins)
        proxy["origins"] = origins

        for l in self._locations:
            if "primary" in l.options:
                proxy["remote"] = l.host
                proxy[l.scheme] = l.port

        # overwrite defaults with user specified proxy
        if isinstance(user_proxy, dict):
            proxy.update(user_proxy)

        # TODO: this should live in a template!
        # If you must escape things in this string with backslashes, be aware
        # of the multiple layers of escaping at work:
        #
        # - Python will unescape backslashes;
        # - Writing out the prefs will escape things via JSON serialization;
        # - The prefs file reader will unescape backslashes;
        # - The JS engine parser will unescape backslashes.
        pacURL = (
            """data:text/plain,
var knownOrigins = (function () {
  return [%(origins)s].reduce(function(t, h) { t[h] = true; return t; }, {})
})();
var uriRegex = new RegExp('^([a-z][-a-z0-9+.]*)' +
                          '://' +
                          '(?:[^/@]*@)?' +
                          '(.*?)' +
                          '(?::(\\\\d+))?/');
var defaultPortsForScheme = {
  'http': 80,
  'ws': 80,
  'https': 443,
  'wss': 443
};
var originSchemesRemap = {
  'ws': 'http',
  'wss': 'https'
};
var proxyForScheme = {
  'http': 'PROXY %(remote)s:%(http)s',
  'https': 'PROXY %(remote)s:%(https)s',
  'ws': 'PROXY %(remote)s:%(ws)s',
  'wss': 'PROXY %(remote)s:%(wss)s'
};

function FindProxyForURL(url, host)
{
  var matches = uriRegex.exec(url);
  if (!matches)
    return 'DIRECT';
  var originalScheme = matches[1];
  var host = matches[2];
  var port = matches[3];
  if (!port && originalScheme in defaultPortsForScheme) {
    port = defaultPortsForScheme[originalScheme];
  }
  var schemeForOriginChecking = originSchemesRemap[originalScheme] || originalScheme;

  var origin = schemeForOriginChecking + '://' + host + ':' + port;
  if (!(origin in knownOrigins))
    return 'DIRECT';
  return proxyForScheme[originalScheme] || 'DIRECT';
}"""
            % proxy
        )
        pacURL = "".join(pacURL.splitlines())

        prefs = []
        prefs.append(("network.proxy.type", 2))
        prefs.append(("network.proxy.autoconfig_url", pacURL))

        return prefs

    def clean_db(self):
        """Removed permissions added by mozprofile."""

        sqlite_file = os.path.join(self._profileDir, "permissions.sqlite")
        if not os.path.exists(sqlite_file):
            return

        # Open database and create table
        permDB = sqlite3.connect(sqlite_file)
        cursor = permDB.cursor()

        # TODO: only delete values that we add, this would require sending
        # in the full permissions object
        cursor.execute("DROP TABLE IF EXISTS moz_hosts")

        # Commit and close
        permDB.commit()
        cursor.close()
