Crash ping
==========

This ping is captured after the main Firefox process crashes or after a child process
process crashes, whether or not the crash report is submitted to
crash-stats.mozilla.org. It includes non-identifying metadata about the crash.

This ping is sent either by the ``CrashManager`` or by the crash reporter
client. The ``CrashManager`` is responsible for sending crash pings for the
child processes crashes, which are sent right after the crash is detected,
as well as for main process crashes, which are sent after Firefox restarts
successfully. The crash reporter client sends crash pings only for main process
crashes whether or not the user also reports the crash. The crash reporter
client will not send the crash ping if telemetry has been disabled in Firefox.

The environment block that is sent with this ping varies: if Firefox was running
long enough to record the environment block before the crash, then the environment
at the time of the crash will be recorded and ``hasCrashEnvironment`` will be true.
If Firefox crashed before the environment was recorded, ``hasCrashEnvironment`` will
be false and the recorded environment will be the environment at time of submission.

The client ID is submitted with this ping.

The metadata field holds a subset of the crash annotations, all field values
are stored as strings but some may be interpreted either as numbers or
boolean values. Numbers are integral unless stated otherwise in the
description. Boolean values are set to "1" when true, "0" when false. If
they're absent then they should be interpreted as false.

Structure:

.. code-block:: js

    {
      type: "crash",
      ... common ping data
      clientId: <UUID>,
      environment: { ... },
      payload: {
        crashDate: "YYYY-MM-DD",
        crashTime: <ISO Date>, // per-hour resolution
        version: 1,
        sessionId: <UUID>, // Telemetry ID of crashing session. May be missing for crashes that happen early in startup
        crashId: <UUID>, // Optional, ID of the associated crash
        minidumpSha256Hash: <hash>, // SHA256 hash of the minidump file
        processType: <type>, // Type of process that crashed, see below for a list of types
        stackTraces: { ... }, // Optional, see below
        metadata: { // Annotations saved while Firefox was running. See CrashAnnotations.yaml for more information
          ProductID: "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}",
          ProductName: "Firefox",
          ReleaseChannel: <channel>,
          Version: <version number>,
          BuildID: "YYYYMMDDHHMMSS",
          AsyncShutdownTimeout: <json>, // Optional, present when a shutdown blocker failed to respond within a reasonable amount of time
          AvailablePageFile: <size>, // Windows-only, available paging file in bytes
          AvailablePhysicalMemory: <size>, // Windows-only, available physical memory in bytes
          AvailableSwapMemory: <size>, // macOS- and Linux-only, available swap space
          AvailableVirtualMemory: <size>, // Windows-only, available virtual memory in bytes
          BackgroundTaskName: "task_name", // Optional, if the app was invoked in background task mode via `--backgroundtask task_name`
          BlockedDllList: <list>, // Windows-only, see WindowsDllBlocklist.cpp for details
          BlocklistInitFailed: "1", // Windows-only, present only if the DLL blocklist initialization failed
          CrashTime: <time>, // Seconds since the Epoch
          DOMFissionEnabled: "1", // Optional, if set indicates that a Fission window had been opened
          EventLoopNestingLevel: <levels>, // Optional, present only if >0, indicates the nesting level of the event-loop
          ExperimentalFeatures: <features>, // Optional, a comma-separated string that specifies the enabled experimental features from about:preferences#experimental
          FontName: <name>, // Optional, the font family name that is being loaded when the crash occurred
          GPUProcessLaunchCount: <num>, // Number of times the GPU process was launched
          HeadlessMode: "1", // Optional, "1" if the app was invoked in headless mode via `--headless ...` or `--backgroundtask ...`
          ipc_channel_error: <error string>, // Optional, contains the string processing error reason for an ipc-based content crash
          IsGarbageCollecting: "1", // Optional, if set indicates that the crash occurred while the garbage collector was running
          LowCommitSpaceEvents: <num>, // Windows-only, present only if >0, number of low commit space events detected by the available memory tracker
          MainThreadRunnableName: <name>, // Optional, Nightly-only, name of the currently executing nsIRunnable on the main thread
          MozCrashReason: <reason>, // Optional, contains the string passed to MOZ_CRASH()
          OOMAllocationSize: <size>, // Size of the allocation that caused an OOM
          ProfilerChildShutdownPhase: <string>, // Profiler shutdown phase
          PurgeablePhysicalMemory: <size>, // macOS-only, amount of memory that can be deallocated by the OS in case of memory pressure
          QuotaManagerShutdownTimeout: <log-string>, // Optional, contains a list of shutdown steps and status of the quota manager clients
          RemoteType: <type>, // Optional, type of content process, see below for a list of types
          SecondsSinceLastCrash: <duration>, // Seconds elapsed since the last crash occurred
          ShutdownProgress: <phase>, // Optional, contains a string describing the shutdown phase in which the crash occurred
          SystemMemoryUsePercentage: <percentage>, // Windows-only, percent of memory in use
          StartupCrash: "1", // Optional, if set indicates that Firefox crashed during startup
          TextureUsage: <usage>, // Optional, usage of texture memory in bytes
          TotalPageFile: <size>, // Windows-only, paging file in use expressed in bytes
          TotalPhysicalMemory: <size>, // Windows-only, physical memory in use expressed in bytes
          TotalVirtualMemory: <size>, // Windows-only, virtual memory in use expressed in bytes
          UptimeTS: <duration>, // Seconds since Firefox was started, this can have a fractional component
          User32BeforeBlocklist: "1", // Windows-only, present only if user32.dll was loaded before the DLL blocklist has been initialized
          WindowsErrorReporting: "1", // Windows-only, present only if the crash was intercepted by the WER runtime exception module
          WindowsPackageFamilyName: <string>, // Windows-only, a string containing the "Package Family Name" of Firefox, if installed through an MSIX package
        },
        hasCrashEnvironment: bool
      }
    }

.. note::

  For "crash" pings generated by the crashreporter we are deliberately truncating the ``creationTime``
  field to hours. See `bug 1345108 <https://bugzilla.mozilla.org/show_bug.cgi?id=1345108>`_ for context.

Process Types
-------------

The ``processType`` field contains the type of process that crashed. There are
currently multiple process types defined in ``nsICrashService`` but crash pings
are sent only for the ones below:

+---------------+---------------------------------------------------+
| Type          | Description                                       |
+===============+===================================================+
| main          | Main process, also known as the browser process   |
+---------------+---------------------------------------------------+
| content       | Content process                                   |
+---------------+---------------------------------------------------+
| gpu           | GPU process                                       |
+---------------+---------------------------------------------------+
| vr            | VR process                                        |
+---------------+---------------------------------------------------+

.. _remote-process-types:

Remote Process Types
--------------------

The optional ``remoteType`` field contains the type of the content process that
crashed. As such it is present only if ``processType`` contains the ``content``
value. The following content process types are currently defined:

+-----------+--------------------------------------------------------+
| Type      | Description                                            |
+===========+========================================================+
| web       | The content process was running code from a web page   |
+-----------+--------------------------------------------------------+
| file      | The content process was running code from a local file |
+-----------+--------------------------------------------------------+
| extension | The content process was running code from an extension |
+-----------+--------------------------------------------------------+

Stack Traces
------------

The crash ping may contain a ``stackTraces`` field which has been populated
with stack traces for all threads in the crashed process. The format of this
field is similar to the one used by Socorro for representing a crash. The main
differences are that redundant fields are not stored and that the module a
frame belongs to is referenced by index in the module array rather than by its
file name.

Note that this field does not contain data from the application; only bare
stack traces and module lists are stored.

.. code-block:: js

    {
      status: <string>, // Status of the analysis, "OK" or an error message
      crash_info: { // Basic crash information
        type: <string>, // Type of crash, SIGSEGV, assertion, etc...
        address: <addr>, // Crash address crash, hex format, see the notes below
        crashing_thread: <index> // Index in the thread array below
      },
      main_module: <index>, // Index of Firefox' executable in the module list
      modules: [{
        base_addr: <addr>, // Base address of the module, hex format
        end_addr: <addr>, // End address of the module, hex format
        code_id: <string>, // Unique ID of this module, see the notes below
        debug_file: <string>, // Name of the file holding the debug information
        debug_id: <string>, // ID or hash of the debug information file
        filename: <string>, // File name
        version: <string>, // Library/executable version
      },
      ... // List of modules ordered by base memory address
      ],
      threads: [{ // Stack traces for every thread
        frames: [{
          module_index: <index>, // Index of the module this frame belongs to
          ip: <ip>, // Program counter, hex format
          trust: <string> // Trust of this frame, see the notes below
        },
        ... // List of frames, the first frame is the topmost
        ]
      }]
    }

Notes
~~~~~

Memory addresses and instruction pointers are always stored as strings in
hexadecimal format (e.g. "0x4000"). They can be made of up to 16 characters for
64-bit addresses.

The crash type is both OS and CPU dependent and can be either a descriptive
string (e.g. SIGSEGV, EXCEPTION_ACCESS_VIOLATION) or a raw numeric value. The
crash address meaning depends on the type of crash. In a segmentation fault the
crash address will be the memory address whose access caused the fault; in a
crash triggered by an illegal instruction exception the address will be the
instruction pointer where the invalid instruction resides.
See `breakpad <https://chromium.googlesource.com/breakpad/breakpad/+/c99d374dde62654a024840accfb357b2851daea0/src/processor/minidump_processor.cc#675>`__'s
relevant code for further information.

Since it's not always possible to establish with certainty the address of the
previous frame while walking the stack, every frame has a trust value that
represents how it was found and thus how certain we are that it's a real frame.
The trust levels are (from least trusted to most trusted):

+---------------+---------------------------------------------------+
| Trust         | Description                                       |
+===============+===================================================+
| context       | Given as instruction pointer in a context         |
+---------------+---------------------------------------------------+
| prewalked     | Explicitly provided by some external stack walker |
+---------------+---------------------------------------------------+
| cfi           | Derived from call frame info                      |
+---------------+---------------------------------------------------+
| frame_pointer | Derived from frame pointer                        |
+---------------+---------------------------------------------------+
| cfi_scan      | Found while scanning stack using call frame info  |
+---------------+---------------------------------------------------+
| scan          | Scanned the stack, found this                     |
+---------------+---------------------------------------------------+
| none          | Unknown, this is most likely not a valid frame    |
+---------------+---------------------------------------------------+

The ``code_id`` field holds a unique ID used to distinguish between different
versions and builds of the same module. See `breakpad <https://chromium.googlesource.com/breakpad/breakpad/+/24f5931c5e0120982c0cbf1896641e3ef2bdd52f/src/google_breakpad/processor/code_module.h#60>`__'s
description for further information. This field is populated only on Windows.

Version History
---------------

- Firefox 58: Added ipc_channel_error (`bug 1410143 <https://bugzilla.mozilla.org/show_bug.cgi?id=1410143>`_).
- Firefox 62: Added LowCommitSpaceEvents (`bug 1464773 <https://bugzilla.mozilla.org/show_bug.cgi?id=1464773>`_).
- Firefox 63: Added RecordReplayError (`bug 1481009 <https://bugzilla.mozilla.org/show_bug.cgi?id=1481009>`_).
- Firefox 64: Added MemoryErrorCorrection (`bug 1498609 <https://bugzilla.mozilla.org/show_bug.cgi?id=1498609>`_).
- Firefox 68: Added IndexedDBShutdownTimeout and LocalStorageShutdownTimeout
  (`bug 1539750 <https://bugzilla.mozilla.org/show_bug.cgi?id=1539750>`_).
- Firefox 74: Added AvailableSwapMemory and PurgeablePhysicalMemory
  (`bug 1587721 <https://bugzilla.mozilla.org/show_bug.cgi?id=1587721>`_).
- Firefox 74: Added MainThreadRunnableName (`bug 1608158 <https://bugzilla.mozilla.org/show_bug.cgi?id=1608158>`_).
- Firefox 76: Added DOMFissionEnabled (`bug 1602918 <https://bugzilla.mozilla.org/show_bug.cgi?id=1602918>`_).
- Firefox 79: Added ExperimentalFeatures (`bug 1644544 <https://bugzilla.mozilla.org/show_bug.cgi?id=1644544>`_).
- Firefox 85: Added QuotaManagerShutdownTimeout, removed IndexedDBShutdownTimeout and LocalStorageShutdownTimeout
  (`bug 1672369 <https://bugzilla.mozilla.org/show_bug.cgi?id=1672369>`_).
- Firefox 89: Added GPUProcessLaunchCount (`bug 1710448 <https://bugzilla.mozilla.org/show_bug.cgi?id=1710448>`_)
  and ProfilerChildShutdownPhase (`bug 1704680 <https://bugzilla.mozilla.org/show_bug.cgi?id=1704680>`_).
- Firefox 90: Removed MemoryErrorCorrection (`bug 1710152 <https://bugzilla.mozilla.org/show_bug.cgi?id=1710152>`_)
  and added WindowsErrorReporting (`bug 1703761 <https://bugzilla.mozilla.org/show_bug.cgi?id=1703761>`_).
- Firefox 95: Added HeadlessMode and BackgroundTaskName (`bug 1697875 <https://bugzilla.mozilla.org/show_bug.cgi?id=1697875>`_).
- Firefox 96: Added WindowsPackageFamilyName (`bug 1738375 <https://bugzilla.mozilla.org/show_bug.cgi?id=1738375>`_).
- Firefox 103: Removed ContainsMemoryReport (`bug 1776279 <https://bugzilla.mozilla.org/show_bug.cgi?id=1776279>` _).
