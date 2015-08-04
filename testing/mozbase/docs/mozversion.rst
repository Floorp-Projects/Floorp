:mod:`mozversion` --- Get application information
=================================================

`mozversion <https://github.com/mozilla/mozbase/tree/master/mozversion>`_
provides version information such as the application name and the changesets
that it has been built from. This is commonly used in reporting or for
conditional logic based on the application under test.

Note that mozversion can report the version of remote devices (e.g. Firefox OS)
but it requires the :mod:`mozdevice` dependency in that case. You can require it
along with mozversion by using the extra *device* dependency:

.. code-block:: bash

  pip install mozversion[device]


API Usage
---------

.. automodule:: mozversion
    :members: get_version

Examples
````````

Firefox::

    import mozversion

    version = mozversion.get_version(binary='/path/to/firefox-bin')
    for (key, value) in sorted(version.items()):
        if value:
            print '%s: %s' % (key, value)

Firefox for Android::

    version = mozversion.get_version(binary='path/to/firefox.apk')
    print version['application_changeset'] # gets hg revision of build

FirefoxOS::

    version = mozversion.get_version(sources='path/to/sources.xml', dm_type='adb')
    print version['gaia_changeset'] # gets gaia git revision

Command Line Usage
------------------

mozversion comes with a command line program, ``mozversion`` which may be used to
get version information from an application.

Usage::

    mozversion [options]

Options
```````

---binary
'''''''''

This is the path to the target application binary or .apk. If this is omitted
then the current directory is checked for the existance of an
application.ini file. If not found, then it is assumed the target
application is a remote Firefox OS instance.


---sources
''''''''''

The path to the sources.xml that accompanies the target application (Firefox OS
only). If this is omitted then the current directory is checked for the
existance of a sources.xml file.

Examples
````````

Firefox::

    $ mozversion --binary=/path/to/firefox-bin
    application_buildid: 20131205075310
    application_changeset: 39faf812aaec
    application_name: Firefox
    application_repository: http://hg.mozilla.org/releases/mozilla-release
    application_version: 26.0
    platform_buildid: 20131205075310
    platform_changeset: 39faf812aaec
    platform_repository: http://hg.mozilla.org/releases/mozilla-release

Firefox for Android::

    $ mozversion --binary=/path/to/firefox.apk

Firefox OS::

    $ mozversion --sources=/path/to/sources.xml
    application_buildid: 20140106040201
    application_changeset: 14ac61461f2a
    application_name: B2G
    application_repository: http://hg.mozilla.org/mozilla-central
    application_version: 29.0a1
    build_changeset: 59605a7c026ff06cc1613af3938579b1dddc6cfe
    device_firmware_date: 1380051975
    device_firmware_version_incremental: 139
    device_firmware_version_release: 4.0.4
    device_id: msm7627a
    gaia_changeset: 9a222ac02db176e47299bb37112ae40aeadbeca7
    gaia_date: 1389005812
    gecko_changeset: 3a2d8af198510726b063a217438fcf2591f4dfcf
    platform_buildid: 20140106040201
    platform_changeset: 14ac61461f2a
    platform_repository: http://hg.mozilla.org/mozilla-central
