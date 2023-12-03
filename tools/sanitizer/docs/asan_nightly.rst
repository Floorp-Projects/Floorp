ASan Nightly
============

The **ASan Nightly Project** involves building a Firefox Nightly browser
with the popular
`AddressSanitizer <https://github.com/google/sanitizers/wiki/AddressSanitizer>`__
tool and enhancing it with remote crash reporting capabilities for any
errors detected.

The purpose of the project is to find subtle memory corruptions
occurring during regular browsing that would either not crash at all or
crash in a way that we cannot figure out what the exact problem is just
from the crash dump. We have a lot of inactionable crash reports and
AddressSanitizer traces are usually a lot more actionable on their own
(especially use-after-free traces). Part of this project is to figure
out if and how many actionable crash reports ASan can give us just by
surfing around. The success of the project of course also depends on the
number of participants.

You can download the latest build using one of the links below. The
builds are self-updating daily like regular nightly builds (like with
regular builds, you can go to *"Help"* → *"About Nightly"* to force an
update check or confirm that you run the latest version).

.. note::

   If you came here looking for regular ASan builds (e.g. for fuzzing or
   as a developer to reproduce a crash), you should probably go to the
   :ref:`Address Sanitizer` doc instead.

.. _Requirements:

Requirements
~~~~~~~~~~~~

Current requirements are:

-  Windows or Linux-based Operating System
-  16 GB of RAM recommended
-  Special ASan Nightly Firefox Build

   -  `Linux
      Download <https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/gecko.v2.mozilla-central.shippable.latest.firefox.linux64-asan-reporter-opt/artifacts/public/build/target.tar.bz2>`__
   -  `Windows
      Download <https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/gecko.v2.mozilla-central.shippable.latest.firefox.win64-asan-reporter-shippable-repackage-signing/artifacts/public/build/target.installer.exe>`__

If you are already using regular Nightly, it should be safe to share the
profile with the regular Nightly instance. If you normally use a beta or
release build (and you would like to be able to switch back to these),
you should consider using a second profile.

.. warning::

   **Windows Users:** Please note that the Windows builds currently show
   an error during setup (see "*Known Issues*" section below), but
   installation works nonetheless. We are working on the problem.

.. note::

   If you run in an environment with any sorts of additional security
   restrictions (e.g. custom process sandboxing), please make sure that
   your /tmp directory is writable and the shipped ``llvm-symbolizer``
   binary is executable from within the Firefox process.

Preferences
~~~~~~~~~~~

If you wish for your crash report to be identifiable, you can go to
``about:config`` and set the **``asanreporter.clientid``** to your
**valid email address**. This isn't mandatory, you can of course report
crash traces anonymously. If you decide to send reports with your email
address and you have a Bugzilla account, consider using the same email
as your Bugzilla account uses. We will then Cc you on any bugs filed
from your crash reports. If your email does not belong to a Bugzilla
account, then we will not publish it but only use it to resolve
questions about your crash reports.

.. note::

   Setting this preference helps us to get back to you in case we have
   questions about your setup/OS. Please consider using it so we can get
   back to you if necessary.

Bug Bounty Program
~~~~~~~~~~~~~~~~~~

As a special reward for participating in the program, we decided to
treat all submitted reports as if they were filed directly in Bugzilla.
This means that reports that

-  indicate a security issue of critical or high rating
-  **and** that can be fixed by our developers

are eligible for a bug bounty according to our `client bug bounty
program
rules <https://www.mozilla.org/security/client-bug-bounty/>`__. As
the report will usually not include any steps to reproduce or a test
case, it will most likely receive a lower-end bounty. Like with regular
bug reports, we would typically reward the first (identifiable) report of
an issue.

.. warning::

   If you would like to participate in the bounty program, make sure you
   set your **``asanreporter.clientid``** preference as specified above.
   We cannot reward any reports that are submitted with no email
   address.


Known Issues
~~~~~~~~~~~~

This section lists all currently known limitations of the ASan Nightly
builds that are considered bugs.

-  [STRIKEOUT:Flash is currently not working]
-  `Bug
   1477490 <https://bugzilla.mozilla.org/show_bug.cgi?id=1477490>`__\ [STRIKEOUT:-
   Windows: Stack instrumentation disabled due to false positives]
-  `Bug
   1478096 <https://bugzilla.mozilla.org/show_bug.cgi?id=1478096>`__ -
   **Windows:** Error during install with maintenanceservice_tmp.exe
-  It has been reported that ASan Nightly performance is particularly
   bad if you run on a screen with 120hz refresh rate. Switching to 60hz
   should improve performance drastically.

Note that these bugs are **specific** to ASan Nightly as listed in the
`tracking bug dependency
list <https://bugzilla.mozilla.org/showdependencytree.cgi?id=1386297&hide_resolved=0>`__.
For the full list of bugs found by this project, see `this
list <https://bugzilla.mozilla.org/showdependencytree.cgi?id=1479399&hide_resolved=0>`__
instead and note that some bugs might not be shown because they are
security bugs.

If you encounter a bug not listed here, please file a bug at
`bugzilla.mozilla.org <https://bugzilla.mozilla.org/>`__ or send an
email to
`choller@mozilla.com <mailto:choller@mozilla.com?subject=%5BASan%20Nightly%20Project%5D%5BBug%20Report%5D>`__.
When filing a bug, it greatly helps if you Cc that email address and
make the bug block `bug
1386297 <https://bugzilla.mozilla.org/show_bug.cgi?id=1386297>`__.

FAQ
~~~

What additional data is collected?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The project only collects ASan traces and (if you set it in the
preferences) your email address. We don't collect any other browser
data, in particular not the sites you were visiting or page contents. It
is really just crash traces submitted to a remote location.

.. note::

   The ASan Nightly browser also still has all the data collection
   capabilities of a regular Nightly browser. The answer above only
   refers to what this project collects **in addition** to what the
   regular Nightly browser can collect.

What's the performance impact?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ASan Nightly build only comes with a slight slowdown at startup and
browsing, sometimes it is not even noticeable. The RAM consumption
however is much higher than with a regular build. Be prepared to restart
your browser sometimes, especially if you use a lot of tabs at once.
Also, the updates are larger than the regular ones, so download times
for updates will be higher, especially if you have a slower internet
connection.

.. warning::

   If you experience performance issues, see also the *"Known Issues"*
   section above, in particular the problem about screen refresh rate
   slowing down Firefox.

What about stability?
^^^^^^^^^^^^^^^^^^^^^

The browser is as stable as a regular Nightly build. Various people have
been surfing around with it for their daily work for weeks now and we
have barely received any crash reports.

How do I confirm that I'm running the correct build?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you open ``about:config`` and type *"asanreporter"* into the search
field, you should see an entry called ``asanreporter.apiurl`` associated
with a URL. Do not modify this value.

.. warning::

   Since Firefox 64, the *"ASan Crash Reporter"* feature is no longer
   listed in ``about:support``

Will there be support for Mac?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

We are working on support for Mac, but it might take longer because we
have no ASan CI coverage on Mac due to hardware constraints. If you work
on Release Engineering and would like to help make e.g. Mac happen
earlier, feel free to `contact
me <mailto:choller@mozilla.com?subject=%5BASan%20Nightly%20Project%5D%20>`__.
