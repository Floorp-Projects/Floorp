Setting Up An Update Server
===========================

The goal of this document is to provide instructions for installing a
locally-served Firefox update for testing purposes. Note that these are not
instructions for how to create or run a production update server. This method of
serving updates is intended to trick Firefox into doing something that it
normally wouldn't do: download and install the same update over and over again.
This is useful for testing but is obviously not the correct behavior for a
production update server.

Obtaining an update MAR
-----------------------

Updates are served as MAR files. There are two common ways to obtain a
MAR to use: download a prebuilt one, or build one yourself.

Downloading a MAR
~~~~~~~~~~~~~~~~~

Prebuilt Nightly MARs can be found
`here <https://archive.mozilla.org/pub/firefox/nightly/>`__ on
archive.mozilla.org. Be sure that you use the one that matches your
machine's configuration. For example, if you want the Nightly MAR from
2019-09-17 for a 64 bit Windows machine, you probably want the MAR
located at
https://archive.mozilla.org/pub/firefox/nightly/2019/09/2019-09-17-09-36-29-mozilla-central/firefox-71.0a1.en-US.win64.complete.mar.

Prebuilt MARs for release and beta can be found
`here <https://archive.mozilla.org/pub/firefox/releases/>`__. Beta
builds are those with a ``b`` in the version string. After locating the
desired version, the MARs will be in the ``update`` directory. You want
to use the MAR labelled ``complete``, not a partial MAR. Here is an
example of an appropriate MAR file to use:
https://archive.mozilla.org/pub/firefox/releases/69.0b9/update/win64/en-US/firefox-69.0b9.complete.mar.

Building a MAR
~~~~~~~~~~~~~~

Building a MAR locally is more complicated. Part of the problem is that
MARs are signed by Mozilla and so you cannot really build an "official"
MAR yourself. This is a security measure designed to prevent anyone from
serving malicious updates. If you want to use a locally-built MAR, the
copy of Firefox being updated will need to be built to allow un-signed
MARs. See :ref:`Building Firefox <Firefox Contributors' Quick Reference>`
for more information on building Firefox locally. In order to use a locally
built MAR, you will need to put this line in the mozconfig file in root of the
build directory (create it if it does not exist):

.. code::

   ac_add_options --enable-unverified-updates

Firefox should otherwise be built normally. After building, you may want
to copy the installation of Firefox elsewhere. If you update the
installation without moving it, attempts at further incremental builds
will not work properly, and a clobber will be needed when building next.
To move the installation, first call ``./mach package``, then copy
``<obj dir>/dist/firefox`` elsewhere. The copied directory will be your
install directory.

If you are running Windows and want the `Mozilla Maintenance
Service <https://support.mozilla.org/en-US/kb/what-mozilla-maintenance-service>`__
to be used, there are a few additional steps to be taken here. First,
the maintenance service needs to be "installed". Most likely, a
different maintenance service is already installed, probably at
``C:\Program Files (x86)\Mozilla Maintenance Service\maintenanceservice.exe``.
Backup that file to another location and replace it with
``<obj dir>/dist/bin/maintenanceservice.exe``. Don't forget to restore
the backup when you are done. Next, you will need to change the
permissions on the Firefox install directory that you created. Both that
directory and its parent directory should have permissions preventing
the current user from writing to it.

Now that you have a build of Firefox capable of using a locally-built
MAR, it's time to build the MAR. First, build Firefox the way you want
it to be after updating. If you want it to be the same before and after
updating, this step is unnecessary and you can use the same build that
you used to create the installation. Then run these commands,
substituting ``<obj dir>``, ``<MAR output path>``, ``<version>`` and
``<channel>`` appropriately:

.. code:: bash

   $ ./mach package
   $ touch "<obj dir>/dist/firefox/precomplete"
   $ MAR="<obj dir>/dist/host/bin/mar.exe" MOZ_PRODUCT_VERSION=<version> MAR_CHANNEL_ID=<channel> ./tools/update-packaging/make_full_update.sh <MAR output path> "<obj dir>/dist/firefox"

For macOS you should use these commands:

.. code:: bash

   $ ./mach package
   $ touch "<obj dir>/dist/firefox/Firefox.app/Contents/Resources/precomplete"
   $ MAR="<obj dir>/dist/host/bin/mar.exe" MOZ_PRODUCT_VERSION=<version> MAR_CHANNEL_ID=<channel> ./tools/update-packaging/make_full_update.sh <MAR output path> "<obj dir>/dist/firefox/Firefox.app"

For a local build, ``<channel>`` can be ``default``, and ``<version>``
can be the value from ``browser/config/version.txt`` (or something
arbitrarily large like ``2000.0a1``).

.. container:: blockIndicator note

   Note: It can be a bit tricky to get the ``make_full_update.sh``
   script to accept paths with spaces.

Serving the update
------------------

Preparing the update files
~~~~~~~~~~~~~~~~~~~~~~~~~~

First, create the directory that updates will be served from and put the
MAR file in it. Then, create a file within called ``update.xml`` with
these contents, replacing ``<mar name>``, ``<hash>`` and ``<size>`` with
the MAR's filename, its sha512 hash, and its file size in bytes.

::

   <?xml version="1.0" encoding="UTF-8"?>
   <updates>
       <update type="minor" displayVersion="2000.0a1" appVersion="2000.0a1" platformVersion="2000.0a1" buildID="21181002100236">
           <patch type="complete" URL="http://127.0.0.1:8000/<mar name>" hashFunction="sha512" hashValue="<hash>" size="<size>"/>
       </update>
   </updates>

If you've downloaded the MAR you're using, you'll find the sha512 value
in a file called SHA512SUMS in the root of the release directory on
archive.mozilla.org for a release or beta build (you'll have to search
it for the file name of your MAR, since it includes the sha512 for every
file that's part of that release), and for a nightly build you'll find a
file with a .checksums extension adjacent to your MAR that contains that
information (for instance, for the MAR file at
https://archive.mozilla.org/pub/firefox/nightly/2019/09/2019-09-17-09-36-29-mozilla-central/firefox-71.0a1.en-US.win64.complete.mar,
the file
https://archive.mozilla.org/pub/firefox/nightly/2019/09/2019-09-17-09-36-29-mozilla-central/firefox-71.0a1.en-US.win64.checksums
contains the sha512 for that file as well as for all the other win64
files that are part of that nightly release).

If you've built your own MAR, you can obtain its sha512 checksum by
running the following command, which should work in Linux, macOS, or
Windows in the MozillaBuild environment:

.. code::

   shasum --algorithm 512 <filename>

On Windows, you can get the exact file size in bytes for your MAR by
right clicking on it in the file explorer and selecting Properties.
You'll find the correct size in bytes at the end of the line that begins
"Size", **not** the one that begins "Size on disk". Be sure to remove
the commas when you paste this number into the XML file.

On macOS, you can get the exact size of your MAR by running the command:

.. code::

   stat -f%z <filename>

Or on Linux, the same command would be:

.. code::

   stat --format "%s" <filename>

Starting your update server
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Now, start an update server to serve the update files on port 8000. An
easy way to do this is with Python. Remember to navigate to the correct
directory before starting the server. This is the Python2 command:

.. code:: bash

   $ python -m SimpleHTTPServer 8000

or, this is the Python3 command:

.. code:: bash

   $ python3 -m http.server 8000

.. container:: blockIndicator note

   If you aren't sure that you started the server correctly, try using a
   web browser to navigate to ``http://127.0.0.1:8000/update.xml`` and
   make sure that you get the XML file you created earlier.

Installing the update
---------------------

You may want to start by deleting any pending updates to ensure that no
previously found updates interfere with installing the desired update.
You can use this command with Firefox's browser console to determine the
update directory:

.. code::

   ChromeUtils.importESModule("resource://gre/modules/FileUtils.sys.mjs").FileUtils.getDir("UpdRootD", []).path

Once you have determined the update directory, close Firefox, browse to
the directory and remove the subdirectory called ``updates``.

| Next, you need to change the update URL to point to the local XML
  file. This can be done most reliably with an enterprise policy. The
  policy file location depends on the operating system you are using.
| Windows/Linux: ``<install dir>/distribution/policies.json``
| macOS: ``<install dir>/Contents/Resources/distribution/policies.json``
| Create the ``distribution`` directory, if necessary, and put this in
  ``policies.json``:

::

   {
     "policies": {
       "AppUpdateURL": "http://127.0.0.1:8000/update.xml"
     }
   }

Now you are ready to update! Launch Firefox out of its installation
directory and navigate to the Update section ``about:preferences``. You
should see it downloading the update to the update directory. Since the
transfer is entirely local this should finish quickly, and a "Restart to
Update" button should appear. Click it to restart and apply the update.
