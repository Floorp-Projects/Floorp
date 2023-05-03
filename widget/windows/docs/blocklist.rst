========================
Windows DLL Blocklisting
========================

--------
Overview
--------

There are many applications which interact with another application, which means
they run their code as a DLL in a different process. This technique is used, for
example, when an antivirus software tries to monitor/block navigation to a
malicious website, or a screen reader tries to access UI parts. If such an
application injects their code into Firefox, and if there is a bug in their code
running in our firefox.exe, it will emerge as Firefox’s bug even though it’s
not.

Firefox for Windows has a feature to prevent DLLs from being loaded into our
processes. If we are aware that a particular DLL causes a problem in our
processes such as a crash or performance degradation, we can stop the problem by
blocking the DLL from being loaded.

This blocklist is about a third-party application which runs outside Firefox but
interacts with Firefox. For add-ons, there is `a different process
<https://extensionworkshop.com/documentation/publish/add-ons-blocking-process/>`_.

This page explains how to request to block a DLL which you think we should block
it as well as technical details about the feature.

-----------------------
Two types of blocklists
-----------------------

There are two types of blocklists in Firefox:

1. A static blocklist that is compiled in to Firefox. This consists of DLLs
   known to cause problems with Firefox, and this blocklist cannot be disabled
   by the user. For more information and instructions on how to add a new DLL
   to this list, see :ref:`Process for blocking a DLL in the static blocklist
   <how-to-block-dll-in-static-blocklist>` below.
2. A dynamic blocklist that users can use to block DLLs that are giving them
   problems. This was added in
   `bug 1744362 <https://bugzilla.mozilla.org/show_bug.cgi?id=1744362>`_.

The static blocklist has ways to specify if only certain versions of a DLL
should be blocked, or only for certain Firefox processes, etc. The dynamic
blocklist does not have this capability; if a DLL is on the list it will always
be blocked.

Regardless of which blocklist the DLL is on, if it meets the criteria for being
blocked Firefox uses the same mechanism to block it. There are more details
below in :ref:`How the blocklist blocks a DLL <how-the-blocklist-blocks-a-dll>`.

.. _how-to-block-dll-in-static-blocklist:

--------------------------------------------------
Process for blocking a DLL in the static blocklist
--------------------------------------------------

But wait, should we really block it?
------------------------------------

Blocking a DLL with the static blocklist should be our last resort to fix a
problem because doing it normally breaks functionality of an application which
installed the DLL. If there is another option, we should always go for it.
Sometimes we can safely bypass a third-party’s problem by changing our code even
though its root cause is not on our side.

When we decide to block it, we must be certain that the issue at hand is so
great that it outweighs the user's choice to install the software, the utility
it provides, and the vendor's freedom to distribute and control their software.

How to request to block a DLL
-----------------------------

Our codebase has the file named
`WindowsDllBlocklistDefs.in <https://searchfox.org/mozilla-central/source/toolkit/xre/dllservices/mozglue/WindowsDllBlocklistDefs.in>`_ from which our build process generates DLL blocklists as C++ header files and compiles them. To block a new DLL, you create a patch to update WindowsDllBlocklistDefs.in and land it on our codebase, following our standard development process. Moreover, you need to fill out a form specific to the DLL blockling request so that reviewers can review the impact and risk as well as the patch itself.

Here are the steps:

1. File `a bug
   <https://bugzilla.mozilla.org/enter_bug.cgi?format=__default__&bug_type=defect&product=Toolkit&component=Blocklist%20Policy%20Requests&op_sys=Windows&short_desc=DLL%20block%20request%3A%20%3CDLL%20name%3E&comment=Please%20go%20through%20https%3A%2F%2Fwiki.mozilla.org%2FBlocklisting%2FDLL%20before%20filing%20a%20new%20bug.>`_
   if it does not exist.
2. Answer all the questions in `this questionnaire
   <https://msmania.github.io/assets/mozilla/third-party-modules/questionnaire.txt>`_,
   and attach it to the bug as a plaintext.
3. Make a patch and start a code review via Phabricator as usual.

How to edit WindowsDllBlocklistDefs.in
--------------------------------------

WindowsDllBlocklistDefs.in defines several variables as a Python Array. When you
add a new entry in the blocklists, you pick one of the variables and add an
entry in the following syntax:

Syntax
******

::

 Variable += [
 ...
     # One-liner comment including a bug number
     EntryType(Name, Version, Flags),
 ...
 ]

Parameters
**********

+-----------+--------------------------------------------------------------------------------+
| Parameter | Value                                                                          |
+===========+================================================================================+
| Variable  | ALL_PROCESSES \| BROWSER_PROCESS \| CHILD_PROCESSES \| GMPLUGIN_PROCESSES \|   |
|           | GPU_PROCESSES \| SOCKET_PROCESSES \| UTILITY_PROCESSES                         |
+-----------+--------------------------------------------------------------------------------+
| EntryType | DllBlocklistEntry \| A11yBlocklistEntry \| RedirectToNoOpEntryPoint            |
+-----------+--------------------------------------------------------------------------------+
| Name      | A case-insensitive string representing a DLL's filename to block               |
+-----------+--------------------------------------------------------------------------------+
| Version   | One of the following formats:                                                  |
|           |                                                                                |
|           |   - ALL_VERSIONS \| UNVERSIONED                                                |
|           |   - A tuple consisting of four digits                                          |
|           |   - A 32-bit integer representing a Unix timestamp with PETimeStamp            |
+-----------+--------------------------------------------------------------------------------+
| Flags     | BLOCK_WIN8_AND_OLDER \| BLOCK_WIN7_AND_OLDER                                   |
+-----------+--------------------------------------------------------------------------------+

Variable
********

Choose one of the following predefined variables.

- **ALL_PROCESSES**: DLLs defined here are blocked in BROWSER_PROCESS +
  CHILD_PROCESSES
- **BROWSER_PROCESS**: DLLs defined here are blocked in the browser process
- **CHILD_PROCESSES**: DLLs defined here are blocked in non-browser processes
- **GMPLUGIN_PROCESSES**: DLLs defined here are blocked in GMPlugin processes
- **GPU_PROCESSES**: DLLs defined here are blocked in GPU processes
- **SOCKET_PROCESSES**: DLLs defined here are blocked in socket processes
- **UTILITY_PROCESSES**: DLLs defined here are blocked in utility processes

EntryType
*********
Choose one of the following predefined EntryTypes.

- **DllBlocklistEntry**: Use this EntryType unless your case matches the other
  EntryTypes.
- **A11yBlocklistEntry**: If you want to block a module only when it’s loaded by
  an accessibility application such as a screen reader, you can use this
  EntryType.
- **RedirectToNoOpEntryPoint**: If a modules is injected via Import Directory
  Table, adding the module as DllBlocklistEntry breaks process launch, meaning
  DllBlocklistEntry is not an option. You can use RedirectToNoOpEntryPoint
  instead.

Name
****
A case-insensitive string representing a DLL's filename to block. Don’t include a directory name.

Version
*******

A maximum version to be blocked. If you specify a value, a module with the
specified version, older versions, and a module with no version are blocked.

| If you want to block a module regardless of its version, use ALL_VERSIONS.
| If you want to block a module with no version, use UNVERSIONED.


To specify a version, you can use either of the following formats:

- | A tuple consisting of four digits. This is compared to the version that is embedded in a DLL as a version resource.
  | Example: (1, 2, 3, 4)
- | A 32-bit integer representing a Unix timestamp with PETimeStamp. This is compared to an integer of IMAGE_FILE_HEADER::TimeDateStamp.
  | Example: PETimeStamp(0x12345678)

Flags
*****

If you know a problem happens only on older Windows versions, you can use one of
the following flags to narrow down the affected platform.

- BLOCK_WIN8_AND_OLDER
- BLOCK_WIN7_AND_OLDER


-----------------
Technical details
-----------------

.. _how-the-blocklist-blocks-a-dll:

How the blocklist blocks a DLL
------------------------------

Briefly speaking, we make ntdll!NtMapViewOfSection return
``STATUS_ACCESS_DENIED`` if a given module is on the blocklist, thereby a
third-party’s code, or even Firefox’s legitimate code, which tries to load a DLL
in our processes in any way such as LoadLibrary API fails and receives an
access-denied error.

Cases where we should not block a module
----------------------------------------

As our blocklist works as explained above, there are the cases where we should not block a module.

- | A module is loaded via `Import Directory Table <https://docs.microsoft.com/en-us/windows/win32/debug/pe-format#import-directory-table>`_
  | Blocking this type of module blocks even a process from launching. You may be able to block this type of module with RedirectToNoOpEntryPoint.
- | A module is loaded as a `Layered Service Provider <https://docs.microsoft.com/en-us/windows/win32/winsock/categorizing-layered-service-providers-and-applications>`_
  | Blocking this type of module on Windows 8 or newer breaks networking. Blocking a LSP on Windows 7 is ok.

(we used to have to avoid blocking modules loaded via a
`Window hook <https://docs.microsoft.com/en-us/windows/win32/winmsg/hooks>`_ because blocking this type of
module would cause repetitive attempts to load a module, resulting in slow performance
like `Bug 1633718 <https://bugzilla.mozilla.org/show_bug.cgi?id=1633718>`_, but this should be fixed
as of `Bug 1823412 <https://bugzilla.mozilla.org/show_bug.cgi?id=1823412>`_.)

Third-party-module ping
-----------------------

We’re collecting the :ref:`third-party-module ping <third-party-modules-ping>`
which captures a moment when a third-party module is loaded into the
Browser/Tab/RDD process. As it’s asked in the request form, it’s important to
check the third-party-module ping and see whether a module we want to block
appears in the ping or not. If it appears, you may be able to know how a module
is loaded by looking at a callstack in the ping.

How to view callstacks in the ping
**********************************

1. You can run a query on BigQuery console or STMO.  (BigQuery console is much
   faster and can handle larger data.)

   - BigQuery console (visit
     `here <https://docs.telemetry.mozilla.org/cookbooks/bigquery.html#gcp-bigquery-console>`_
     to request access): https://console.cloud.google.com/bigquery
   - STMO: https://sql.telemetry.mozilla.org/

2. Make your own query based on `this template
   <https://msmania.github.io/assets/mozilla/third-party-modules/query-template.txt>`_.
3. Run the query.
4. Save the result as a JSON file.

   - In BigQuery console, click [SAVE RESULTS] and choose [JSON (local file)].
   - In STMO, click [...] at the right-top corner and select [Show API Key],
     then you can download a JSON from a URL shown in the [Results in JSON format].

5. | Go to https://msmania.github.io/assets/mozilla/third-party-modules/
   |   (A temporal link. Need to find a permanent place.)
6. Click [Upload JSON] and select the file you saved at the step 4.
7. Click a row in the table to view a callstack


How to see the versions of a specific module in the ping
********************************************************

You can use `this template query
<https://msmania.github.io/assets/mozilla/third-party-modules/query-groupby-template.txt>`_
to query which versions of a specific module are captured in the ping. This
tells the product versions which are actively used including the crashing
versions and the working versions.

You can also get the crashing versions by querying the crash reports or the
Socorro table. Having two version lists, you can decide whether you can specify
the Version parameter in a blocklist entry.

Initialization
--------------

In order to have the most effective blocking of DLLs, the blocklist is
initialized very early during browser startup. If the :ref:`launcher process
<launcher-process>` is available, the steps are:

- Launcher process loads dynamic blocklist from disk (see
  `DynamicBlocklist::LoadFile()
  <https://searchfox.org/mozilla-central/search?q=DynamicBlocklist%3A%3ALoadFile&path=&case=false&regexp=false>`_)
- Launcher process puts dynamic blocklist data in shared section (see
  `SharedSection::AddBlocklist()
  <https://searchfox.org/mozilla-central/search?q=SharedSection%3A%3AAddBlocklist&path=&case=false&regexp=false>`_)
- Launcher process creates the browser process in a suspended mode, sets up its
  dynamic blocklist, then starts it. (see `LauncherMain()
  <https://searchfox.org/mozilla-central/search?q=LauncherMain&path=&case=false&regexp=false>`_)

  - This is so (ideally) no DLLs can be injected before the blocklist is set up.

If the launcher process is not available, a different blocklist is used, defined
in `mozglue/WindowsDllBlocklist.cpp
<https://searchfox.org/mozilla-central/source/toolkit/xre/dllservices/mozglue/WindowsDllBlocklist.cpp>`_.
This code does not currently support the dynamic blocklist. This is intended to
only be used in testing and other non-deployed scenarios, so this shouldn't be
a problem for users.

Note that the mozglue blocklist also has a feature to block threads that start
in ``LoadLibrary`` and variants. This code is currently only turned on in
Nightly builds because it breaks some third-party DLP products.

Dynamic blocklist file location
-------------------------------

Because the blocklist is loaded so early during startup, we don't have access to
what profile is going to be loaded, so the blocklist file can't be stored there.
Instead, by default the blocklist file is stored in the Windows user's roaming
app data directory, specifically

``<Roaming AppData directory>\Mozilla\Firefox\blocklist-<install hash>``

Note that the install hash here is what is returned by `GetInstallHash()
<https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/common/commonupdatedir.cpp#404>`_,
and is suitable for uniquely identifying the particular Firefox installation
that is running.

On first launch, this location will be written to the registry, and can be
overriden by setting that key to a different file location. The registry key is
``HKEY_CURRENT_USER\Software\Mozilla\Firefox\Launcher``, and the name is the
full path to firefox.exe with "\|Blocklist" appended. This code is in
`LauncherRegistryInfo
<https://searchfox.org/mozilla-central/source/toolkit/xre/LauncherRegistryInfo.cpp>`_.

Adding to and removing from the dynamic blocklist
-------------------------------------------------

Users can add or remove DLLs from the dynamic blocklist by navigating to
``about:third-party``, finding the entry for the DLL they are interested in, and
clicking on the dash icon. They will then be prompted to restart the browser, as
the change will only take effect after the browser restarts.

Disabling the dynamic blocklist
-------------------------------

It is possible that users can get Firefox into a bad state by putting a DLL on
the dynamic blocklist. One possibility is that the user blocks only one of a set
of DLLs that interact, which could make Firefox behave in unpredictable ways or
crash.

By launching Firefox with ``--disableDynamicBlocklist``\, the dynamic blocklist
will be loaded but not used to block DLLs. This lets the user go to
``about:third-party`` and attempt to fix the problem by unblocking or blocking
DLLs.

Similarly, in safe mode the dynamic blocklist is also disabled.

Enterprise policy
-----------------

The dynamic blocklist can be disabled by setting a registry key at
``HKEY_CURRENT_USER\Software\Policies\Mozilla\Firefox`` with a name of
DisableThirdPartyModuleBlocking and a DWORD value of 1. This will have the
effect of not loading the dynamic blocklist, and no icons will show up in
``about:third-party`` to allow blocking DLLs.

-------
Contact
-------

Any questions or feedback are welcome!

**Matrix**: `#hardening <https://app.element.io/#/room/#hardening:mozilla.org>`_
