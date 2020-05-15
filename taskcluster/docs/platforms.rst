Platforms in the CI
===================


.. https://raw.githubusercontent.com/mozilla/treeherder/HEAD/ui/helpers/constants.js
   awk -e /thPlatformMap = {/,/};/ constants.js |grep ""|cut -d: -f2|sed -e s/^/   /|sed -e "s/$/ ,, /g"
   TODO:
      * Leverage verify_docs - https://bugzilla.mozilla.org/show_bug.cgi?id=1636400
      * Add a new column (when executed ? ie always, rarely, etc)
      * asan reporter isn't listed for mac os x

Build Platforms
---------------

.. csv-table::
   :header: "Platform", "Owner", "Why?"
   :widths: 20, 40, 40

   Linux, ,
   Linux DevEdition, ,
   Linux Nightly, , 
   Linux shippable, ,
   Linux x64, ,
   Linux x64 addon, ,
   Linux x64 DevEdition, , 
   Linux x64 WebRender pgo, ,
   Linux x64 WebRender Shippable, , 
   Linux x64 WebRender, ,
   Linux x64 Nightly, , 
   Linux x64 shippable, , What we ship to our users
   Linux x64 Stylo-Seq, ,
   Linux x64 NoOpt, ,
   Linux AArch64, ,
   OS X 10.14, ,
   OS X Cross Compiled, ,
   OS X 10.14 shippable, , 
   OS X Cross Compiled shippable, , What we ship to our users
   OS X Cross Compiled NoOpt, ,
   OS X Cross Compiled addon, , 
   OS X Cross Compiled DevEdition, ,
   OS X 10.14, ,
   OS X 10.14 WebRender, , 
   OS X 10.14 Shippable, , 
   OS X 10.14 WebRender Shippable, ,
   OS X 10.14 DevEdition, , 
   OS X 10.14 Nightly, ,
   Windows 2012, ,
   Windows 2012 shippable, , 
   Windows 2012 addon, , 
   Windows 2012 NoOpt, , 
   Windows 2012 DevEdition, , 
   Windows 2012 x64, , 
   Windows 2012 x64 shippable, , 
   Windows 2012 AArch64, , 
   Windows 2012 AArch64 Shippable, , 
   Windows 2012 AArch64 DevEdition, , 
   Windows 2012 x64 addon, ,
   Windows 2012 x64 NoOpt, ,
   Windows 2012 x64 DevEdition, ,
   Windows MinGW, Tom Ritter, the Tor project uses MinGW; make sure we test that for them
   Android 4.0 API16+, , 
   Android 4.0 API16+ Beta, ,
   Android 4.0 API16+ Release, ,
   Android 4.0 API16+ GeckoView multi-arch fat AAR, ,
   Android 4.2 x86, ,
   Android 4.2 x86 Beta, , 
   Android 4.2 x86 Release, , 
   Android 4.2 x86, , 
   Android 4.2 x86 Beta, , 
   Android 4.2 x86 Release, , 
   Android 4.3 API16+, , 
   Android 4.3 API16+ Beta, , 
   Android 4.3 API16+ Release, ,
   Android 5.0 AArch64, ,
   Android 5.0 AArch64 Beta, , 
   Android 5.0 AArch64 Release, , 
   Android 5.0 x86-64, , 
   Android 5.0 x86-64 Beta, , 
   Android 5.0 x86-64 Release, , 
   Android 7.0 x86, , 
   Android 7.0 x86 Beta, , 
   Android 7.0 x86 Release, , 
   Android 7.0 x86-64, , 
   Android 7.0 x86-64 WebRender, , 
   Android 7.0 x86-64 Beta, , 
   Android 7.0 x86-64 Release, , 
   Android 7.0 MotoG5, , 
   Android 8.0 Pixel2, , 
   Android 8.0 Pixel2 WebRender, , 
   Android 8.0 Pixel2 Nightly, ,
   Android 8.0 Pixel2 AArch64, , 
   Android 8.0 Pixel2 AArch64 WebRender, , 
   Android 8.0 Pixel2 AArch64 Nightly, ,
   Android, ,
   Packages, ,
   Other, ,

Testing configurations
----------------------

We have some platforms used to run the tests to make sure they run correctly on different versions of the operating systems.

.. csv-table::
   :header: "Platform", "Owner", "Why?"
   :widths: 20, 40, 40

   Linux 18.04 shippable, ,
   Linux 18.04 x64, ,
   Linux 18.04 x64 DevEdition, ,
   Linux 18.04 x64 WebRender Shippable, ,
   Linux 18.04 x64 WebRender, ,
   Linux 18.04 x64 shippable, , What we ship to our users
   Linux 18.04 x64 Stylo-Seq, ,
   Windows 7, ,
   Windows 7 DevEdition, ,
   Windows 7 VM Nightly, ,
   Windows 7 Shippable, ,
   Windows 7 MinGW, Tom Ritter, the Tor project uses MinGW; make sure we test that for them
   Windows 10 x64, ,
   Windows 10 x64 DevEdition, ,
   Windows 10 x64 Nightly, ,
   Windows 10 x64 WebRender pgo, ,
   Windows 10 x64 Shippable, ,
   Windows 10 x64 WebRender Shippable, ,
   Windows 10 x64 WebRender, ,
   Windows 10 x64 2017 Ref HW, ,
   Windows 10 x64 MinGW, Tom Ritter, the Tor project uses MinGW; make sure we test that for them
   Windows 10 AArch64, ,


Quality platforms
-----------------

We have many platforms used to run various quality tools. They aren't directly focussing on user quality but on code quality,
or prevening some classes of errors (memory, threading, etc).

.. csv-table::
   :header: "Platform", "Owner", "Why?"
   :widths: 20, 40, 40

   Linux 18.04 x64 tsan, Christian Holler, Identify threading issues with ThreadSanitizer
   Linux x64 asan, "| Christian Holler
   | Tyson Smith (ubsan)", "| Identify memory issues with :ref:`Address Sanitizer`.
   | Also includes the UndefinedBehaviorSanitizer"
   Linux x64 WebRender asan, "| Christian Holler
   | Tyson Smith (ubsan)", "| Identify memory issues with :ref:`Address Sanitizer`.
   | Also includes the UndefinedBehaviorSanitizer"
   Linux x64 asan reporter, Christian Holler, Generate :ref:`ASan Nightly Project <ASan Nightly>` builds
   Linux x64 CCov, Marco Castelluccio , Collect :ref:`Code coverage` information to identify what is tested (or not)
   Linux 18.04 x64 asan, "| Christian Holler
   | Tyson Smith (ubsan)", "| Identify memory issues with :ref:`Address Sanitizer`.
   | Also includes the UndefinedBehaviorSanitizer"
   Linux 18.04 x64 WebRender asan, "| Christian Holler
   | Tyson Smith (ubsan)", "| Identify memory issues with :ref:`Address Sanitizer`.
   | Also includes the UndefinedBehaviorSanitizer"
   Linux 18.04 x64 CCov, Marco Castelluccio , Collect :ref:`Code coverage` information to identify what is tested (or not)
   OS X Cross Compiled CCov, Marco Castelluccio , Collect :ref:`Code coverage` information to identify what is tested (or not)
   OS X 10.14 Cross Compiled CCov, Marco Castelluccio , Collect :ref:`Code coverage` information to identify what is tested (or not)
   Windows 2012 x64 asan reporter, Christian Holler, Generate :ref:`ASan Nightly Project <ASan Nightly>` builds
   Windows 10 x64 CCov, Marco Castelluccio , Collect :ref:`Code coverage` information to identify what is tested (or not)
   Android 4.0 API16+ CCov, Marco Castelluccio , Collect :ref:`Code coverage` information to identify what is tested (or not)
   Android 4.3 API16+ CCov, Marco Castelluccio , Collect :ref:`Code coverage` information to identify what is tested (or not)
   Diffoscope, Mike Hommey, Make sure the build remains reproducible
   Linting, "| Sylvestre Ledru
   | Andrew Halberstadt", "| Identify :ref:`code quality` earlier
   | Also contains some Bugzilla and :ref:`Documentation jobs<Managing Documentation>`"



Infrastructure tasks
--------------------

The decision tasks responsible for creating the task graph.

.. csv-table::
   :header: "Task", "Owner", "Why?"
   :widths: 20, 40, 40

   Gecko Decision Task, , Define the tasks to run and their order
   Firefox Release Tasks, ,
   Devedition Release Tasks, ,
   Fennec Beta Tasks, ,
   Fennec Release Tasks, ,
   Thunderbird Release Tasks, ,


Others
------

.. csv-table::
   :header: "Platform", "Owner", "Why?"
   :widths: 20, 40, 40

   Fetch, ,
   Docker Images, ,
   Toolchains, ,