HTTP Logging
============


Sometimes, while debugging your Web app (or client-side code using
Necko), it can be useful to log HTTP traffic.  This saves a log of HTTP-related
information from your browser run into a file that you can examine (or
upload to Bugzilla if a developer has asked you for a log).

.. note::

   **Note:** The `Web
   Console <https://developer.mozilla.org/en-US/docs/Tools/Web_Console>`__
   also offers the ability to peek at HTTP transactions within Firefox.
   HTTP logging generally provides more detailed logging.

.. _using-about-networking:

Using about:logging
-------------------

This is the best and easiest way to do HTTP logging.  At any point
during while your browser is running, you can turn logging on and off.

.. note::

   **Note:** Before Firefox 108 the logging UI used to be located at `about:networking#logging`

This allows you to capture only the "interesting" part of the browser's
behavior (i.e. your bug), which makes the HTTP log much smaller and
easier to analyze.

#. Launch the browser and get it into whatever state you need to be in
   just before your bug occurs.
#. Open a new tab and type in "about:logging" into the URL bar.
#. Adjust the location of the log file if you don't like the default
#. Adjust the list of modules that you want to log: this list has the
   exact same format as the MOZ_LOG environment variable (see below).
   Generally the default list is OK, unless a Mozilla developer has told
   you to modify it.

   * For cookie issues, use presets ``Cookies``
   * For WebSocket issues, use presets ``WebSockets``
   * For HTTP/3 or QUIC issues, use presets ``HTTP/3``
   * For other networking issues, use presets ``Networking``

#. Click on Start Logging.
#. Reproduce the bug (i.e. go to the web site that is broken for you and
   make the bug happen in the browser)
#. Make a note of the value of "Current Log File".
#. Click on Stop Logging.
#. Go to the folder containing the specified log file, and gather all
   the log files. You will see several files that look like:
   log.txt-main.1806.moz_log, log.txt-child.1954.moz_log,
   log.txt-child.1970.moz_log, etc.  This is because Firefox now uses
   multiple processes, and each process gets its own log file.
#. For many bugs, the "log.txt-main.moz_log" file is the only thing you need to
   upload as a file attachment to your Bugzilla bug (this is assuming
   you're logging to help a mozilla developer).  Other bugs may require
   all the logs to be uploaded--ask the developer if you're not sure.
#. Pat yourself on the back--a job well done!  Thanks for helping us
   debug Firefox.

.. note::

   **Note:** The log may include sensitive data such as URLs and cookies.
   To protect your privacy, we kindly request you to send the log file or
   the profiler link directly and confidentially to necko@mozilla.com.

Logging HTTP activity by manually setting environment variables
---------------------------------------------------------------

Sometimes the about:logging approach won't work, for instance if your
bug occurs during startup, or you're running on mobile, etc.  In that
case you can set environment variables \*before\* you launch Firefox.
Note that this approach winds up logging the whole browser history, so
files can get rather large (they compress well :)

Setting environment variables differs by operating system. Don't let the
scary-looking command line stuff frighten you off; it's not hard at all!

Windows
~~~~~~~

#. If Firefox is already running, exit out of it.

#. Open a command prompt by holding down the Windows key and pressing "R".

#. Type CMD and press enter, a new Command Prompt window with a black
   background will appear.

#. | Copy and paste the following lines one at a time into the Command
     Prompt window. Press the enter key after each one.:
   | **For 64-bit Windows:**

   ::

      set MOZ_LOG=timestamp,rotate:200,nsHttp:5,cache2:5,nsSocketTransport:5,nsHostResolver:5
      set MOZ_LOG_FILE=%TEMP%\log.txt
      "c:\Program Files\Mozilla Firefox\firefox.exe"

   **For 32-bit Windows:**

   ::

      set MOZ_LOG=timestamp,rotate:200,nsHttp:5,cache2:5,nsSocketTransport:5,nsHostResolver:5
      set MOZ_LOG_FILE=%TEMP%\log.txt
      "c:\Program Files (x86)\Mozilla Firefox\firefox.exe"

   (These instructions assume that you installed Firefox to the default
   location, and that drive C: is your Windows startup disk. Make the
   appropriate adjustments if those aren't the case.)

#. Reproduce whatever problem it is that you're having.

#. Once you've reproduced the problem, exit Firefox and look for the
   generated log files in your temporary directory. You can type
   "%TEMP%" directly into the Windows Explorer location bar to get there
   quickly.

Linux
~~~~~

This section offers information on how to capture HTTP logs for Firefox
running on Linux.

#. Quit out of Firefox if it's running.

#. Open a new shell. The commands listed here assume a bash-compatible
   shell.

#. Copy and paste the following commands into the shell one at a time.
   Make sure to hit enter after each line.

   ::

      export MOZ_LOG=timestamp,rotate:200,nsHttp:5,cache2:5,nsSocketTransport:5,nsHostResolver:5
      export MOZ_LOG_FILE=/tmp/log.txt
      cd /path/to/firefox
      ./firefox

#. Reproduce the problem you're debugging.

#. When the problem has been reproduced, exit Firefox and look for the
   generated log files, which you can find at ``/tmp/log.txt``.

Mac OS X
~~~~~~~~

These instructions show how to log HTTP traffic in Firefox on Mac OS X.

#. Quit Firefox is if it's currently running, by using the Quit option
   in the File menu. Keep in mind that simply closing all windows does
   **not** quit Firefox on Mac OS X (this is standard practice for Mac
   applications).

#. Run the Terminal application, which is located in the Utilities
   subfolder in your startup disk's Applications folder.

#. Copy and paste the following commands into the Terminal window,
   hitting the return key after each line.

   ::

      export MOZ_LOG=timestamp,rotate:200,nsHttp:5,cache2:5,nsSocketTransport:5,nsHostResolver:5
      export MOZ_LOG_FILE=~/Desktop/log.txt
      cd /Applications/Firefox.app/Contents/MacOS
      ./firefox-bin

   (The instructions assume that you've installed Firefox directly into
   your startup disk's Applications folder. If you've put it elsewhere,
   change the path used on the third line appropriately.)

#. Reproduce whatever problem you're trying to debug.

#. Quit Firefox and look for the generated ``log.txt`` log files on your
   desktop.

.. note::

   **Note:** The generated log file uses Unix-style line endings. Older
   editors may have problems with this, but if you're using an even
   reasonably modern Mac OS X application to view the log, you won't
   have any problems.

Start logging using command line arguments
------------------------------------------

Since Firefox 61 it's possible to start logging in a bit simpler way
than setting environment variables: using command line arguments.  Here
is an example for the **Windows** platform, on other platforms we accept
the same form of the arguments:

#. If Firefox is already running, exit out of it.

#. Open a command prompt. On `Windows
   XP <https://commandwindows.com/runline.htm>`__, you can find the
   "Run..." command in the Start menu's "All Programs" submenu. On `all
   newer versions of
   Windows <http://www.xp-vista.com/other/where-is-run-in-windows-vista>`__,
   you can hold down the Windows key and press "R".

#. | Copy and paste the following line into the "Run" command window and
     then press enter:
   | **For 32-bit Windows:**

   ::

      "c:\Program Files (x86)\Mozilla Firefox\firefox.exe" -MOZ_LOG=timestamp,rotate:200,nsHttp:5,cache2:5,nsSocketTransport:5,nsHostResolver:5 -MOZ_LOG_FILE=%TEMP%\log.txt

   **For 64-bit Windows:**

   ::

      "c:\Program Files\Mozilla Firefox\firefox.exe" -MOZ_LOG=timestamp,rotate:200,nsHttp:5,cache2:5,nsSocketTransport:5,nsHostResolver:5 -MOZ_LOG_FILE=%TEMP%\log.txt

   (These instructions assume that you installed Firefox to the default
   location, and that drive C: is your Windows startup disk. Make the
   appropriate adjustments if those aren't the case.)

#. Reproduce whatever problem it is that you're having.

#. Once you've reproduced the problem, exit Firefox and look for the
   generated log files in your temporary directory. You can type
   "%TEMP%" directly into the Windows Explorer location bar to get there
   quickly.

Advanced techniques
-------------------

You can adjust some of the settings listed above to change what HTTP
information get logged.

Limiting the size of the logged data
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

By default there is no limit to the size of log file(s), and they
capture the logging throughout the time Firefox runs, from start to
finish.  These files can get quite large (gigabytes)!  So we have added
a 'rotate:SIZE_IN_MB' option to MOZ_LOG (we use it in the examples
above).  If you are using Firefox >= 51, setting this option saves only
the last N megabytes of logging data, which helps keep them manageable
in size.  (Unknown modules are ignored, so it's OK to use 'rotate' in
your environment even if you're running Firefox <= 50: it will do
nothing).

This is accomplished by splitting the log into up to 4 separate files
(their filenames have a numbered extension, .0, .1, .2, .3)  The logging
back end cycles the files it writes to, while ensuring that the sum of
these files’ sizes will never go over the specified limit.

Note 1: **the file with the largest number is not guaranteed to be the
last file written!**  We don’t move the files, we only cycle.  Using the
rotate module automatically adds timestamps to the log, so it’s always
easy to recognize which file keeps the most recent data.

Note 2: **rotate doesn’t support append**.  When you specify rotate, on
every start all the files (including any previous non-rotated log file)
are deleted to avoid any mixture of information.  The ``append`` module
specified is then ignored.

Use 'sync' if your browser crashes or hangs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

By default, HTTP logging buffers messages and only periodically writes
them to disk (this is more efficient and also makes logging less likely
to interfere with race conditions, etc).  However, if you are seeing
your browser crash (or hang) you should add ",sync" to the list of
logging modules in your MOZ_LOG environment variable.  This will cause
each log message to be immediately written (and fflush()'d), which is
likely to give us more information about your crash.

Turning on QUIC logging
~~~~~~~~~~~~~~~~~~~~~~~

This can be done by setting `MOZ_LOG` to
`timestamp,rotate:200,nsHttp:5,neqo_http3::*:5,neqo_transport::*:5`.

Logging only HTTP request and response headers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There are two ways to do this:

#. Replace MOZ_LOG\ ``=nsHttp:5`` with MOZ_LOG\ ``=nsHttp:3`` in the
   commands above.
#. There's a handy extension for Firefox called `HTTP Header
   Live <https://addons.mozilla.org/firefox/addon/3829>`__ that you can
   use to capture just the HTTP request and response headers. This is a
   useful tool when you want to peek at HTTP traffic.

Turning off logging of socket-level transactions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you're not interested in socket-level log information, either because
it's not relevant to your bug or because you're debugging something that
includes a lot of noise that's hard to parse through, you can do that.
Simply remove the text ``nsSocketTransport:5`` from the commands above.

Turning off DNS query logging
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can turn off logging of host resolving (that is, DNS queries) by
removing the text ``nsHostResolver:5`` from the commands above.

Enable Logging for try server runs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can enable logging on try by passing the `env` argument via `mach try`.
For example:

.. note::

   ``./mach try fuzzy --env "MOZ_LOG=nsHttp:5,SSLTokensCache:5"``

How to enable QUIC logging
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The steps to enable QUIC logging (`QLOG <https://datatracker.ietf.org/doc/draft-ietf-quic-qlog-main-schema/>`__) are:

#. Go to ``about:config``,  search for ``network.http.http3.enable_qlog`` and set it to true.
#. Restart Firefox.
#. QLOG files will be saved in the ``qlog_$PID`` directory located within your system's temporary directory.
#. To visualize the QLOG data, visit https://qvis.quictools.info/. You can upload the QLOG files there to see the visual representation of the flows.

See also
--------

-  There are similar options available to debug mailnews protocols.
   See `this
   document <https://www-archive.mozilla.org/quality/mailnews/mail-troubleshoot.html>`__ for
   more info about mailnews troubleshooting.
-  On the Windows platform, nightly Firefox builds have FTP logging
   built-in (don't ask why this is only the case for Windows!). To
   enable FTP logging, just set ``MOZ_LOG=nsFtp:5`` (in older versions
   of Mozilla, you need to use ``nsFTPProtocol`` instead of ``nsFtp``).
-  When Mozilla's built-in logging capabilities aren't good enough, and
   you need a full-fledged packet tracing tool, two free products are
   `Wireshark <https://www.wireshark.org/>`__
   and `ngrep <https://github.com/jpr5/ngrep/>`__. They are available
   for Windows and most flavors of UNIX (including Linux and Mac OS
   X), are rock solid, and offer enough features to help uncover any
   Mozilla networking problem.
