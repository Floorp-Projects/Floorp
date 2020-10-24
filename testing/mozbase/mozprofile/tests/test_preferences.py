#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import mozfile
import os
import shutil
import tempfile

import mozunit
import pytest
from wptserve import server

from mozprofile.cli import MozProfileCLI
from mozprofile.prefs import Preferences, PreferencesReadError
from mozprofile.profile import Profile

here = os.path.dirname(os.path.abspath(__file__))


# preferences from files/prefs_with_comments.js
_prefs_with_comments = {'browser.startup.homepage': 'http://planet.mozilla.org',
                        'zoom.minPercent': 30,
                        'zoom.maxPercent': 300,
                        'webgl.verbose': 'false'}


@pytest.fixture
def run_command():
    """
    invokes mozprofile command line via the CLI factory
    - args : command line arguments (equivalent of sys.argv[1:])
    """
    def inner(*args):
        # instantiate the factory
        cli = MozProfileCLI(list(args))

        # create the profile
        profile = cli.profile()

        # return path to profile
        return profile.profile
    return inner


@pytest.fixture
def compare_generated(run_command):
    """
    writes out to a new profile with mozprofile command line
    reads the generated preferences with prefs.py
    compares the results
    cleans up
    """
    def inner(prefs, commandline):
        profile = run_command(*commandline)
        prefs_file = os.path.join(profile, 'user.js')
        assert os.path.exists(prefs_file)
        read = Preferences.read_prefs(prefs_file)
        if isinstance(prefs, dict):
            read = dict(read)
        assert prefs == read
        shutil.rmtree(profile)
    return inner


def test_basic_prefs(compare_generated):
    """test setting a pref from the command line entry point"""

    _prefs = {"browser.startup.homepage": "http://planet.mozilla.org/"}
    commandline = []
    for pref, value in _prefs.items():
        commandline += ["--pref", "%s:%s" % (pref, value)]
    compare_generated(_prefs, commandline)


def test_ordered_prefs(compare_generated):
    """ensure the prefs stay in the right order"""
    _prefs = [("browser.startup.homepage", "http://planet.mozilla.org/"),
              ("zoom.minPercent", 30),
              ("zoom.maxPercent", 300),
              ("webgl.verbose", 'false')]
    commandline = []
    for pref, value in _prefs:
        commandline += ["--pref", "%s:%s" % (pref, value)]
    _prefs = [(i, Preferences.cast(j)) for i, j in _prefs]
    compare_generated(_prefs, commandline)


def test_ini(compare_generated):
    # write the .ini file
    _ini = """[DEFAULT]
browser.startup.homepage = http://planet.mozilla.org/

[foo]
browser.startup.homepage = http://github.com/
"""
    try:
        fd, name = tempfile.mkstemp(suffix='.ini', text=True)
        os.write(fd, _ini.encode())
        os.close(fd)
        commandline = ["--preferences", name]

        # test the [DEFAULT] section
        _prefs = {'browser.startup.homepage': 'http://planet.mozilla.org/'}
        compare_generated(_prefs, commandline)

        # test a specific section
        _prefs = {'browser.startup.homepage': 'http://github.com/'}
        commandline[-1] = commandline[-1] + ':foo'
        compare_generated(_prefs, commandline)

    finally:
        # cleanup
        os.remove(name)


def test_ini_keep_case(compare_generated):
    """
    Read a preferences config file with a preference in camel-case style.
    Check that the read preference name has not been lower-cased
    """
    # write the .ini file
    _ini = """[DEFAULT]
network.dns.disableIPv6 = True
"""
    try:
        fd, name = tempfile.mkstemp(suffix='.ini', text=True)
        os.write(fd, _ini.encode())
        os.close(fd)
        commandline = ["--preferences", name]

        # test the [DEFAULT] section
        _prefs = {'network.dns.disableIPv6': 'True'}
        compare_generated(_prefs, commandline)

    finally:
        # cleanup
        os.remove(name)


def test_reset_should_remove_added_prefs():
    """Check that when we call reset the items we expect are updated"""
    profile = Profile()
    prefs_file = os.path.join(profile.profile, 'user.js')

    # we shouldn't have any initial preferences
    initial_prefs = Preferences.read_prefs(prefs_file)
    assert not initial_prefs
    initial_prefs = open(prefs_file).read().strip()
    assert not initial_prefs

    # add some preferences
    prefs1 = [("mr.t.quotes", "i aint getting on no plane!")]
    profile.set_preferences(prefs1)
    assert prefs1 == Preferences.read_prefs(prefs_file)
    lines = open(prefs_file).read().strip().splitlines()
    assert any(line.startswith('#MozRunner Prefs Start') for line in lines)
    assert any(line.startswith('#MozRunner Prefs End') for line in lines)

    profile.reset()
    assert prefs1 != Preferences.read_prefs(os.path.join(profile.profile, 'user.js'))


def test_reset_should_keep_user_added_prefs():
    """Check that when we call reset the items we expect are updated"""
    profile = Profile()
    prefs_file = os.path.join(profile.profile, 'user.js')

    # we shouldn't have any initial preferences
    initial_prefs = Preferences.read_prefs(prefs_file)
    assert not initial_prefs
    initial_prefs = open(prefs_file).read().strip()
    assert not initial_prefs

    # add some preferences
    prefs1 = [("mr.t.quotes", "i aint getting on no plane!")]
    profile.set_persistent_preferences(prefs1)
    assert prefs1 == Preferences.read_prefs(prefs_file)
    lines = open(prefs_file).read().strip().splitlines()
    assert any(line.startswith('#MozRunner Prefs Start') for line in lines)
    assert any(line.startswith('#MozRunner Prefs End') for line in lines)

    profile.reset()
    assert prefs1 == Preferences.read_prefs(os.path.join(profile.profile, 'user.js'))


def test_magic_markers():
    """ensure our magic markers are working"""

    profile = Profile()
    prefs_file = os.path.join(profile.profile, 'user.js')

    # we shouldn't have any initial preferences
    initial_prefs = Preferences.read_prefs(prefs_file)
    assert not initial_prefs
    initial_prefs = open(prefs_file).read().strip()
    assert not initial_prefs

    # add some preferences
    prefs1 = [("browser.startup.homepage", "http://planet.mozilla.org/"),
              ("zoom.minPercent", 30)]
    profile.set_preferences(prefs1)
    assert prefs1 == Preferences.read_prefs(prefs_file)
    lines = open(prefs_file).read().strip().splitlines()
    assert bool([line for line in lines
                 if line.startswith('#MozRunner Prefs Start')])
    assert bool([line for line in lines
                 if line.startswith('#MozRunner Prefs End')])

    # add some more preferences
    prefs2 = [("zoom.maxPercent", 300),
              ("webgl.verbose", 'false')]
    profile.set_preferences(prefs2)
    assert prefs1 + prefs2 == Preferences.read_prefs(prefs_file)
    lines = open(prefs_file).read().strip().splitlines()
    assert len([line for line in lines
                if line.startswith('#MozRunner Prefs Start')]) == 2
    assert len([line for line in lines
                if line.startswith('#MozRunner Prefs End')]) == 2

    # now clean it up
    profile.clean_preferences()
    final_prefs = Preferences.read_prefs(prefs_file)
    assert not final_prefs
    lines = open(prefs_file).read().strip().splitlines()
    assert '#MozRunner Prefs Start' not in lines
    assert '#MozRunner Prefs End' not in lines


def test_preexisting_preferences():
    """ensure you don't clobber preexisting preferences"""

    # make a pretend profile
    tempdir = tempfile.mkdtemp()

    try:
        # make a user.js
        contents = """
user_pref("webgl.enabled_for_all_sites", true);
user_pref("webgl.force-enabled", true);
"""
        user_js = os.path.join(tempdir, 'user.js')
        f = open(user_js, 'w')
        f.write(contents)
        f.close()

        # make sure you can read it
        prefs = Preferences.read_prefs(user_js)
        original_prefs = [('webgl.enabled_for_all_sites', True), ('webgl.force-enabled', True)]
        assert prefs == original_prefs

        # now read this as a profile
        profile = Profile(tempdir, preferences={"browser.download.dir": "/home/jhammel"})

        # make sure the new pref is now there
        new_prefs = original_prefs[:] + [("browser.download.dir", "/home/jhammel")]
        prefs = Preferences.read_prefs(user_js)
        assert prefs == new_prefs

        # clean up the added preferences
        profile.cleanup()
        del profile

        # make sure you have the original preferences
        prefs = Preferences.read_prefs(user_js)
        assert prefs == original_prefs
    finally:
        shutil.rmtree(tempdir)


def test_can_read_prefs_with_multiline_comments():
    """
    Ensure that multiple comments in the file header do not break reading
    the prefs (https://bugzilla.mozilla.org/show_bug.cgi?id=1233534).
    """
    user_js = tempfile.NamedTemporaryFile(suffix='.js', delete=False)
    try:
        with user_js:
            user_js.write("""
# Mozilla User Preferences

/* Do not edit this file.
*
* If you make changes to this file while the application is running,
* the changes will be overwritten when the application exits.
*
* To make a manual change to preferences, you can visit the URL about:config
*/

user_pref("webgl.enabled_for_all_sites", true);
user_pref("webgl.force-enabled", true);
""".encode())
        assert Preferences.read_prefs(user_js.name) == [
                ('webgl.enabled_for_all_sites', True),
                ('webgl.force-enabled', True)
        ]
    finally:
        mozfile.remove(user_js.name)


def test_json(compare_generated):
    _prefs = {"browser.startup.homepage": "http://planet.mozilla.org/"}
    json = '{"browser.startup.homepage": "http://planet.mozilla.org/"}'

    # just repr it...could use the json module but we don't need it here
    with mozfile.NamedTemporaryFile(suffix='.json') as f:
        f.write(json.encode())
        f.flush()

        commandline = ["--preferences", f.name]
        compare_generated(_prefs, commandline)


def test_json_datatypes():
    # minPercent is at 30.1 to test if non-integer data raises an exception
    json = """{"zoom.minPercent": 30.1, "zoom.maxPercent": 300}"""

    with mozfile.NamedTemporaryFile(suffix='.json') as f:
        f.write(json.encode())
        f.flush()

        with pytest.raises(PreferencesReadError):
            Preferences.read_json(f._path)


def test_prefs_write():
    """test that the Preferences.write() method correctly serializes preferences"""

    _prefs = {'browser.startup.homepage': "http://planet.mozilla.org",
              'zoom.minPercent': 30,
              'zoom.maxPercent': 300}

    # make a Preferences manager with the testing preferences
    preferences = Preferences(_prefs)

    # write them to a temporary location
    path = None
    read_prefs = None
    try:
        with mozfile.NamedTemporaryFile(suffix='.js', delete=False, mode='w+t') as f:
            path = f.name
            preferences.write(f, _prefs)

        # read them back and ensure we get what we put in
        read_prefs = dict(Preferences.read_prefs(path))

    finally:
        # cleanup
        if path and os.path.exists(path):
            os.remove(path)

    assert read_prefs == _prefs


def test_read_prefs_with_comments():
    """test reading preferences from a prefs.js file that contains comments"""

    path = os.path.join(here, 'files', 'prefs_with_comments.js')
    assert dict(Preferences.read_prefs(path)) == _prefs_with_comments


def test_read_prefs_with_interpolation():
    """test reading preferences from a prefs.js file whose values
    require interpolation"""

    expected_prefs = {
        "browser.foo": "http://server-name",
        "zoom.minPercent": 30,
        "webgl.verbose": "false",
        "browser.bar": "somethingxyz"
    }
    values = {
        "server": "server-name",
        "abc": "something"
    }
    path = os.path.join(here, 'files', 'prefs_with_interpolation.js')
    read_prefs = Preferences.read_prefs(path, interpolation=values)
    assert dict(read_prefs) == expected_prefs


def test_read_prefs_ttw():
    """test reading preferences through the web via wptserve"""

    # create a WebTestHttpd instance
    docroot = os.path.join(here, 'files')
    host = '127.0.0.1'
    port = 8888
    httpd = server.WebTestHttpd(host=host, port=port, doc_root=docroot)

    # create a preferences instance
    prefs = Preferences()

    try:
        # start server
        httpd.start(block=False)

        # read preferences through the web
        read = prefs.read_prefs('http://%s:%d/prefs_with_comments.js' % (host, port))
        assert dict(read) == _prefs_with_comments
    finally:
        httpd.stop()


if __name__ == '__main__':
    mozunit.main()
