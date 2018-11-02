
"untrustedModules" ping
=======================

This ping contains information about events whereby untrusted modules
were loaded into the Firefox process.

.. code-block:: js

    {
      "type": "untrustedModules",
      ... common ping data
      "clientId": <UUID>,
      "environment": { ... },
      "payload": {
        "structVersion": <number>, // See below
        "errorModules": <number>, // Number of modules that failed to be evaluated
        "events": [ ... ]
        "combinedStacks": { ... },
      }
    }

payload.structVersion
---------------------
Version of this payload structure. Reserved for future use; this is currently
always ``1``.

payload.events
--------------
An array, each element representing the load of an untrusted module. Because
modules can invoke the load of other modules, there may be
multiple modules listed per event.

This array is synchronized with the ``payload.combinedStacks`` object which
contains the captured stack trace for each event.

A maximum of 50 events are captured.

.. code-block:: js

    {
      "processUptimeMS": <number>, // Number of milliseconds since app startup
      "isStartup": <boolean>, // See below
      "threadID": <unsigned integer>, // Thread ID where the event occurred
      "threadName": <string>, // If available, the name of the thread
      "modules": [
        {
          "moduleName": <string>, // See below
          "loaderName": <string>, // See below
          "baseAddress": <string>, // Base address where the module was loaded, e.g. "0x7ffc01260000"
          "fileVersion": <string>, // The module file version, e.g. "1.10.2.6502"
          "moduleTrustFlags": <unsigned integer> // See below
        },
        ... Additional modules
      ]
    }

payload.events[...].isStartup
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* ``true`` if the event represents a module that was already loaded before monitoring was initiated.
* ``false`` if the event was captured during normal execution.

payload.events[...].modules[...].moduleName
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The path to the module file, modified to remove any potentially sensitive
information. In most cases, the directory path is removed leaving only the
file name, e.g. ``foo.dll``. There are two exceptions:

* Paths under ``%ProgramFiles%`` are preserved, e.g. ``%ProgramFiles%\FooApplication\foo.dll``
* Paths under the temporary path are preserved, e.g. ``%TEMP%\bin\foo.dll``

payload.events[...].modules[...].loaderName
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The name of the module as it was requested from the OS. Generally this will be
the same as the above ``moduleName``.

This is treated as a path and is modified for privacy in the same way as
``moduleName`` above.

payload.events[...].modules[...].moduleTrustFlags
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This is a bitfield indicating whether various attributes apply to the module.

* ``1`` if the module is digitally signed by Mozilla
* ``2`` if the module is digitally signed by Microsoft
* ``4`` if the module's version info indicates it's a Microsoft module
* ``8`` if the module is located in the Firefox application directory
* ``16`` if the module has the same location and version information as the Firefox executable
* ``32`` if the module is located in the system directory
* ``64`` if the module is a known keyboard layout DLL

payload.combinedStacks
----------------------
This object holds stack traces that correspond to events in ``payload.events``.

.. code-block:: js

    "combinedStacks": {
      "memoryMap": [
        [
          <string>, // Name of the module symbol file, e.g. ``xul.pdb``
          <string> // Breakpad identifier of the module, e.g. ``08A541B5942242BDB4AEABD8C87E4CFF2``
        ],
        ... Additional modules
      ],
      "stacks": [
        [
          [
            <integer>, // The module index or -1 for invalid module indices
            <unsigned integer> // The program counter relative to its module base, or an absolute pc
          ],
          ... Additional stack frames (maximum 500)
        ],
        ... Additional stack traces (maximum 50)
      ]
    },

Notes
~~~~~
* The client id is submitted with this ping.
* The :doc:`Telemetry Environment <../data/environment>` is submitted in this ping.
* String fields within ``payload`` are limited in length to 260 characters.
* This ping is only enabled on Nightly builds of Firefox Desktop for Windows.
* This ping is sent once daily.
* Only events occurring on the main browser process are recorded.
* If there are no events to report, this ping is not sent.
