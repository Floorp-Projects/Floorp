Documentation architecture
==========================

The documentation relies on Sphinx and many Sphinx extensions.

The documentation code is in two main directories:

* https://searchfox.org/mozilla-central/source/docs
* https://searchfox.org/mozilla-central/source/tools/moztreedocs

Our documentation supports both rst & markdown syntaxes.

Configuration
-------------

The main configuration file is:

https://searchfox.org/mozilla-central/source/docs/config.yml

It contains the categories, the redirects, the warnings and others configuration aspects.

The dependencies are listed in:

https://searchfox.org/mozilla-central/source/tools/moztreedocs/requirements.in

Be aware that Python libraries stored in `third_party/python` are used in priority (not always for good reasons). See :ref:`Vendor the source of the Python package in-tree <python-vendor>` for more details.


Architecture
------------


`mach_commands <https://searchfox.org/mozilla-central/source/tools/moztreedocs/mach_commands.py>`__
contains:

* `mach doc` arguments managements
* Detection/configuration of the environment (nodejs for jsdoc, pip for dependencies, etc)
* Symlink the doc sources (.rst & .md) from the source tree into the staging directory
* Fails the build if any critical warnings have been identified
* Starts the sphinx build (and serve it if the option is set)
* Manages telemetry

`docs/conf.py <https://searchfox.org/mozilla-central/source/docs/conf.py>`__ defines:

* The list of extensions
* JS source paths
* Various sphinx configuration

At the end of the build documentation process, files will be uploaded to a CDN:

https://searchfox.org/mozilla-central/source/tools/moztreedocs/upload.py
