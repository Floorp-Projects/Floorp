[Mozprofile](https://github.com/mozilla/mozbase/tree/master/mozprofile)
is a python tool for creating and managing profiles for Mozilla's
applications (Firefox, Thunderbird, etc.). In addition to creating profiles,
mozprofile can install [addons](https://developer.mozilla.org/en/addons)
and set
[preferences](https://developer.mozilla.org/En/A_Brief_Guide_to_Mozilla_Preferences).
Mozprofile can be utilized from the command line or as an API.


# Command Line Usage

mozprofile may be used to create profiles, set preferences in
profiles, or install addons into profiles.

The profile to be operated on may be specified with the `--profile`
switch. If a profile is not specified, one will be created in a
temporary directory which will be echoed to the terminal:

    (mozmill)> mozprofile
    /tmp/tmp4q1iEU.mozrunner
    (mozmill)> ls /tmp/tmp4q1iEU.mozrunner
    user.js

To run mozprofile from the command line enter:
`mozprofile --help` for a list of options.


# API Usage

To use mozprofile as an API you can import
[mozprofile.profile](https://github.com/mozilla/mozbase/tree/master/mozprofile/mozprofile/profile.py)
and/or the
[AddonManager](https://github.com/mozilla/mozbase/tree/master/mozprofile/mozprofile/addons.py).

`mozprofile.profile` features a generic `Profile` class.  In addition,
subclasses `FirefoxProfile` and `ThundebirdProfile` are available
with preset preferences for those applications.

`mozprofile.profile:Profile`:

    def __init__(self,
                 profile=None, # Path to the profile
                 addons=None,  # String of one or list of addons to install
                 addon_manifests=None,  # Manifest for addons, see http://ahal.ca/blog/2011/bulk-installing-fx-addons/
                 preferences=None, # Dictionary or class of preferences
                 locations=None, # locations to proxy
                 proxy=False, # setup a proxy
                 restore=True # If true remove all installed addons preferences when cleaning up
                 ):

    def reset(self):
        """reset the profile to the beginning state"""

    def set_preferences(self, preferences, filename='user.js'):
        """Adds preferences dict to profile preferences"""

    def clean_preferences(self):
        """Removed preferences added by mozrunner."""

    def cleanup(self):
        """Cleanup operations for the profile."""


`mozprofile.addons:AddonManager`:

    def __init__(self, profile):
        """profile - the path to the profile for which we install addons"""

    def install_addons(self, addons=None, manifests=None):
        """
        Installs all types of addons
        addons - a list of addon paths to install
        manifest - a list of addon manifests to install
        """

    @classmethod
    def get_amo_install_path(self, query):
        """
        Return the addon xpi install path for the specified AMO query.
        See: https://developer.mozilla.org/en/addons.mozilla.org_%28AMO%29_API_Developers%27_Guide/The_generic_AMO_API
        for query documentation.
        """

    @classmethod
    def addon_details(cls, addon_path):
        """
        returns a dictionary of details about the addon
        - addon_path : path to the addon directory
        Returns:
        {'id':      u'rainbow@colors.org', # id of the addon
         'version': u'1.4',                # version of the addon
         'name':    u'Rainbow',            # name of the addon
         'unpack': False } # whether to unpack the addon
        """

    def clean_addons(self):
        """Cleans up addons in the profile."""


# Installing Addons

Addons may be installed individually or from a manifest.

Example:

	from mozprofile import FirefoxProfile
	
	# create new profile to pass to mozmill/mozrunner
	profile = FirefoxProfile(addons=["adblock.xpi"])


# Setting Preferences

Preferences can be set in several ways:

- using the API: You can pass preferences in to the Profile class's
  constructor: `obj = FirefoxProfile(preferences=[("accessibility.typeaheadfind.flashBar", 0)])`
- using a JSON blob file: `mozprofile --preferences myprefs.json`
- using a `.ini` file: `mozprofile --preferences myprefs.ini`
- via the command line: `mozprofile --pref key:value --pref key:value [...]`

When setting preferences from  an `.ini` file or the `--pref` switch,
the value will be interpolated as an integer or a boolean
(`true`/`false`) if possible.

# Setting Permissions

mozprofile also takes care of adding permissions to the profile.
See https://github.com/mozilla/mozbase/blob/master/mozprofile/mozprofile/permissions.py


# Resources

Other Mozilla programs offer additional and overlapping functionality
for profiles.  There is also substantive documentation on profiles and
their management.

- [ProfileManager](https://developer.mozilla.org/en/Profile_Manager) : 
  XULRunner application for managing profiles. Has a GUI and CLI.
- [python-profilemanager](http://k0s.org/mozilla/hg/profilemanager/) : python CLI interface similar to ProfileManager
- profile documentation : http://support.mozilla.com/en-US/kb/Profiles
