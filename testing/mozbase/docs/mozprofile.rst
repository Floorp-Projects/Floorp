:mod:`mozprofile` --- Create and modify Mozilla application profiles
====================================================================

Mozprofile_ is a python tool for creating and managing profiles for Mozilla's
applications (Firefox, Thunderbird, etc.). In addition to creating profiles,
mozprofile can install addons_ and set preferences_ Mozprofile can be utilized
from the command line or as an API.

The preferred way of setting up profile data (addons, permissions, preferences
etc) is by passing them to the profile_ constructor.

Addons
------

.. automodule:: mozprofile.addons
   :members:

Addons may be installed individually or from a manifest.

Example::

	from mozprofile import FirefoxProfile
	
	# create new profile to pass to mozmill/mozrunner
	profile = FirefoxProfile(addons=["adblock.xpi"])

Command Line Interface
----------------------

.. automodule:: mozprofile.cli
   :members:

The profile to be operated on may be specified with the ``--profile``
switch. If a profile is not specified, one will be created in a
temporary directory which will be echoed to the terminal::

    (mozmill)> mozprofile
    /tmp/tmp4q1iEU.mozrunner
    (mozmill)> ls /tmp/tmp4q1iEU.mozrunner
    user.js

To run mozprofile from the command line enter:
``mozprofile --help`` for a list of options.

Permissions
-----------

.. automodule:: mozprofile.permissions
   :members:

You can set permissions by creating a ``ServerLocations`` object that you pass
to the ``Profile`` constructor. Hosts can be added to it with
``add_host(host, port)``. ``port`` can be 0.

Preferences
-----------

.. automodule:: mozprofile.prefs
   :members:

Preferences can be set in several ways:

- using the API: You can make a dictionary with the preferences and pass it to
  the ``Profile`` constructor. You can also add more preferences with the
  ``Profile.set_preferences`` method.
- using a JSON blob file: ``mozprofile --preferences myprefs.json``
- using a ``.ini`` file: ``mozprofile --preferences myprefs.ini``
- via the command line: ``mozprofile --pref key:value --pref key:value [...]``

When setting preferences from  an ``.ini`` file or the ``--pref`` switch,
the value will be interpolated as an integer or a boolean
(``true``/``false``) if possible.

Profile
--------------------

.. automodule:: mozprofile.profile
   :members:

Resources
-----------
Other Mozilla programs offer additional and overlapping functionality
for profiles.  There is also substantive documentation on profiles and
their management.

- ProfileManager_: XULRunner application for managing profiles. Has a GUI and CLI.
- python-profilemanager_: python CLI interface similar to ProfileManager
- profile documentation_ 


.. _Mozprofile: https://github.com/mozilla/mozbase/tree/master/mozprofile
.. _addons: https://developer.mozilla.org/en/addons
.. _preferences: https://developer.mozilla.org/En/A_Brief_Guide_to_Mozilla_Preferences
.. _mozprofile.profile: https://github.com/mozilla/mozbase/tree/master/mozprofile/mozprofile/profile.py
.. _AddonManager: https://github.com/mozilla/mozbase/tree/master/mozprofile/mozprofile/addons.py
.. _here: https://github.com/mozilla/mozbase/blob/master/mozprofile/mozprofile/permissions.py
.. _ProfileManager: https://developer.mozilla.org/en/Profile_Manager
.. _python-profilemanager: http://k0s.org/mozilla/hg/profilemanager/
.. _documentation: http://support.mozilla.com/en-US/kb/Profiles
