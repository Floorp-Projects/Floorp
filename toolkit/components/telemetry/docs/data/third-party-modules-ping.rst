
"third-party-modules" ping
==========================

This ping contains information about events whereby third-party modules
were loaded into Firefox processes.

.. code-block:: js

    {
      "type": "third-party-modules",
      ... common ping data
      "clientId": <UUID>,
      "environment": { ... },
      "payload": {
        "structVersion": 1,
        "modules" [
          {
            // The sanitized name of the module as resolved by the Windows loader.
            "resolvedDllName": <string>,
            // Version of the DLL as contained in its resources's fixed version information.
            "fileVersion": <string>,
            // The value of the CompanyName field as extracted from the DLL's version information. This property is only present when such version info is present, and when the 'signedBy' property is absent.
            "companyName": <string>,
            // The organization whose certificate was used to sign the DLL. Only present for signed modules.
            "signedBy": <string>,
            // Flags that indicate this module's level of trustworthiness. This corresponds to one or more mozilla::ModuleTrustFlags OR'd together.
            "trustFlags": <unsigned int>
          },
          ... Additional modules (maximum 100)
        ],
        "processes": {
          <string containing pid, formatted as "0x%x">: {
            // Except for Default (which is remapped to "browser"), one of the process string names specified in xpcom/build/GeckoProcessTypes.h.
            "processType": <string>,
            // Elapsed time since process creation that this object was generated, in seconds.
            "elapsed": <number>,
            // Time spent loading xul.dll in milliseconds.
            "xulLoadDurationMS": <number>,
            // Number of dropped events due to failures sanitizing file paths.
            "sanitizationFailures": <int>,
            // Number of dropped events due to failures computing trust levels.
            "trustTestFailures": <int>,
            // Array of module load events for this process. The entries of this array are ordered to be in sync with the combinedStacks.stacks array (see below)
            "events": [
              {
                // Elapsed time since process creation that this event was generated, in milliseconds.
                "processUptimeMS": <int>,
                // Time spent loading this module, in milliseconds.
                "loadDurationMS": <number>,
                // Thread ID for the thread that loaded the module.
                "threadID": <int>,
                // Name of the thread that loaded the module, when applicable.
                "threadName": <string>,
                // The sanitized name of the module that was requested by the invoking code. Only exists when it is different from resolvedDllName.
                "requestedDllName": <string>,
                // The base address to which the loader mapped the module.
                "baseAddress": <string formatted as "0x%x">,
                // Index of the element in the modules array that contains details about the module that was loaded during this event.
                "moduleIndex": <int>,
                // True if the module is included in the executable's Import Directory Table.
                "isDependent": <bool>
              },
              ... Additional events (maximum 50)
            ],
            "combinedStacks": [
              "memoryMap": [
                [
                  // Name of the module symbol file, e.g. ``xul.pdb``
                  <string>,
                  // Breakpad identifier of the module, e.g. ``08A541B5942242BDB4AEABD8C87E4CFF2``
                  <string>
                ],
                ... Additional modules
              ],
              // Array of stacks for this process. These entries are ordered to be in sync with the events array
              "stacks": [
                [
                  [
                    // The module index or -1 for invalid module indices
                    <integer>,
                    // The program counter relative to its module base, or an absolute pc if the module index is -1
                    <unsigned integer>
                  ],
                  ... Additional stack frames (maximum 512)
                ],
                ... Additional stack traces (maximum 50)
              ]
            ]
          },
          ... Additional processes (maximum 100)
        }
      }
    }

payload.processes[...].events[...].resolvedDllName
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The effective path to the module file, sanitized to remove any potentially
sensitive information. In most cases, the directory path is removed leaving only
the leaf name, e.g. ``foo.dll``. There are three exceptions:

* Paths under ``%ProgramFiles%`` are preserved, e.g. ``%ProgramFiles%\FooApplication\foo.dll``
* Paths under ``%SystemRoot%`` are preserved, e.g. ``%SystemRoot%\System32\DriverStore\FileRepository\nvlt.inf_amd64_97992900c592012e\nvinitx.dll``
* Paths under the temporary path are preserved, e.g. ``%TEMP%\bin\foo.dll``

payload.processes[...].events[...].requestedDllName
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The name of the module as it was requested from the OS. This string is also
sanitized in a similar fashion to to ``resolvedDllName``. This string is
omitted from the ping when it is identical to ``resolvedDllName``.

Notes
~~~~~
* The client id is submitted with this ping.
* The :doc:`Telemetry Environment <../data/environment>` is submitted in this ping.
* String fields within ``payload`` are limited in length to 260 characters.
* This ping is sent once daily.
* If there are no events to report, this ping is not sent.

Version History
~~~~~~~~~~~~~~~
- Firefox 77: Added ``isDependent`` (`bug 1620118 <https://bugzilla.mozilla.org/show_bug.cgi?id=1620118>`_).
- Firefox 71: Renamed from untrustedModules to third-party-modules with a revised schema (`bug 1542830 <https://bugzilla.mozilla.org/show_bug.cgi?id=1542830>`_).
- Firefox 70: Added ``%SystemRoot%`` as an exemption to path sanitization (`bug 1573275 <https://bugzilla.mozilla.org/show_bug.cgi?id=1573275>`_).
- Firefox 66:
   - Added Windows Side-by-side directory trust flag (`bug 1514694 <https://bugzilla.mozilla.org/show_bug.cgi?id=1514694>`_).
   - Added module load times (``xulLoadDurationMS``, ``loadDurationMS``) and xul.dll trust flag (`bug 1518490 <https://bugzilla.mozilla.org/show_bug.cgi?id=1518490>`_).
   - Added SysWOW64 trust flag (`bug 1518798 <https://bugzilla.mozilla.org/show_bug.cgi?id=1518798>`_).
- Firefox 65: Initial support (`bug 1435827 <https://bugzilla.mozilla.org/show_bug.cgi?id=1435827>`_).
