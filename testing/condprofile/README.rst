Conditioned Profile
===================

This project provides a command-line tool that is used to generate and maintain
a collection of Gecko profiles.

Unlike testing/profiles, the **conditioned profiles** are a collection of full
Gecko profiles that are dynamically updated every day.

Each profile is created or updated using a **scenario** and a
**customization**, and eventually uploaded as an artifact in TaskCluster.

The goal of the project is to build a collection of profiles that we can use in
our performance or functional tests instead of the empty profile that we
usually create on the fly with **mozprofile**.

Having a collection of realistic profiles we can use when running some tests
gives us the ability to check the impact of user profiles on page loads or
other tests.

A full cycle of how this tool is used in Taskcluster looks like this:

For each combination of scenario, customization and platform:

- grabs an existing profile in Taskcluster
- browses the web using the scenario, via the WebDriver client
- recreates a tarball with the updated profile
- uploads it as an index artifact into TaskCluster - maintains a changelog of each change

It's based on the Arsenic webdriver client https://github.com/HDE/arsenic

A client that wants to use a profile can download it from the indexed artifacts
by using a simple HTTP client or the provided client in **condprof.client**.


Scenario
========

Scenarii are coroutines registered under a unique name in condprof/scenarii.
They get a **session** object and some **options**.

The scenario can do whatever it wants with the browser, through the session
instance.

See Arsenic's `API documentation <https://arsenic.readthedocs.io/en/latest/reference/session.html>`_ for the session class.

Adding a new scenario is done by adding a module in condprof/scenarii/
and register it in condprof/scenarii/__init__.py


Customization
=============

A customization is a configuration file that can be used to set some
prefs in the browser and install some webextensions.

Customizations are JSON files registered into condprof/customizations,
and they provide four keys:

- **name**: the name of the customization
- **addons**: a mapping of add-ons to install.
- **prefs**: a mapping of prefs to set
- **scenario**: a mapping of options to pass to a specific scenario

In the example below, we install uBlock, set a pref, and pass the
**max_urls** option to the **heavy** scenario.

  {
      "name": "intermediate",
      "addons":{
         "uBlock":"https://addons.mozilla.org/firefox/downloads/file/3361355/ublock_origin-1.21.2-an+fx.xpi"
      },
      "prefs":{
         "accessibility.tabfocus": 9
      },
      "scenario": {
         "heavy": {"max_urls": 10}
      }
   }


Getting conditioned profiles
============================

Unlike the profile creator, the client is Python 2 and 3 compatible.

You can grab a conditioned profile using the client API::

   >>> from condprof.client import get_profile
   >>> get_profile(".", "win64", "settled", "default")

or the **cp-client** script that gets install when you run the
conditioned profile installer.

Running locally
===============

Unfortunately, we can't hook the conditioned profile builder into mach
at this point. We need to wait for everything in the tree to be fully
Python 3 compatible.

Until then, if you want to build profiles locally, to try out one
of your scenario for instance, you can install a local Python 3
virtual env and use the script from there.

Get a mozilla-central source clone and do the following::

   $ cd testing/condprofile
   $ virtualenv .

From there you can trigger profiles creation using **bin/cp-creator**.
