Platforms in the CI
===================


.. https://raw.githubusercontent.com/mozilla/treeherder/HEAD/ui/helpers/constants.js
   awk -e /thPlatformMap = {/,/};/ constants.js |grep ""|cut -d: -f2|sed -e s/^/   /|sed -e "s/$/ ,, /g"
   TODO:
      * Leverage verify_docs - https://bugzilla.mozilla.org/show_bug.cgi?id=1636400
      * Add a new column (when executed ? ie always, rarely, etc)
      * asan reporter isn't listed for mac os x

.. csv-table:: Platforms
   :header: "Platform", "Owner", "Why?"
   :widths: 20, 40, 40

   Linux, ,
   Linux DevEdition, ,
   Linux Nightly, , 
   Linux shippable, , 
   Linux 18.04 shippable, , 
   Linux x64, ,
   Linux 18.04 x64 tsan, Christian Holler, Identify threading issues with ThreadSanitizer
   Linux x64 asan, "| Christian Holler
   | Tyson Smith (ubsan)", "| Identify memory issues with :ref:`Address Sanitizer`.
   | Also includes the UndefinedBehaviorSanitizer"
   Linux x64 QuantumRender asan, "| Christian Holler
   | Tyson Smith (ubsan)", "| Identify memory issues with :ref:`Address Sanitizer`.
   | Also includes the UndefinedBehaviorSanitizer"
   Linux x64 asan reporter, Christian Holler, Generate :ref:`ASan Nightly Project <ASan Nightly>` builds
   Linux x64 addon, ,
   Linux x64 DevEdition, , 
   Linux x64 QuantumRender pgo, , 
   Linux x64 QuantumRender Shippable, , 
   Linux x64 QuantumRender, ,
   Linux x64 Nightly, , 
   Linux x64 shippable, , What we ship to our users
   Linux x64 Stylo-Seq, , 
   Linux x64 CCov, Marco Castelluccio , Collect :ref:`Code coverage` information to identify what is tested (or not)
   Linux x64 NoOpt, , 
   Linux AArch64, ,
   Linux 18.04 x64, , 
   Linux 18.04 x64 asan, "| Christian Holler
   | Tyson Smith (ubsan)", "| Identify memory issues with :ref:`Address Sanitizer`.
   | Also includes the UndefinedBehaviorSanitizer"
   Linux 18.04 x64 QuantumRender asan, "| Christian Holler
   | Tyson Smith (ubsan)", "| Identify memory issues with :ref:`Address Sanitizer`.
   | Also includes the UndefinedBehaviorSanitizer"
   Linux 18.04 x64 DevEdition, ,
   Linux 18.04 x64 QuantumRender Shippable, ,
   Linux 18.04 x64 QuantumRender, , 
   Linux 18.04 x64 shippable, , What we ship to our users
   Linux 18.04 x64 Stylo-Seq, ,
   Linux 18.04 x64 CCov, Marco Castelluccio , Collect :ref:`Code coverage` information to identify what is tested (or not)
   OS X 10.14, ,
   OS X Cross Compiled, , 
   OS X 10.14 shippable, , 
   OS X Cross Compiled shippable, , What we ship to our users
   OS X Cross Compiled NoOpt, , 
   OS X Cross Compiled addon, , 
   OS X Cross Compiled CCov, Marco Castelluccio , Collect :ref:`Code coverage` information to identify what is tested (or not)
   OS X Cross Compiled DevEdition, , 
   OS X 10.14, , 
   OS X 10.14 QuantumRender, , 
   OS X 10.14 Shippable, , 
   OS X 10.14 QuantumRender Shippable, , 
   OS X 10.14 DevEdition, , 
   OS X 10.14 Nightly, ,
   OS X 10.14 Cross Compiled CCov, Marco Castelluccio , Collect :ref:`Code coverage` information to identify what is tested (or not)
   Windows 7, ,
   Windows 7 DevEdition, , 
   Windows 7 VM Nightly, ,
   Windows 7 Shippable, , 
   Windows 7 MinGW, Tom Ritter, the Tor project uses MinGW; make sure we test that for them
   Windows 10 x64, , 
   Windows 10 x64 CCov, Marco Castelluccio , Collect :ref:`Code coverage` information to identify what is tested (or not)
   Windows 10 x64 DevEdition, , 
   Windows 10 x64 Nightly, , 
   Windows 10 x64 QuantumRender pgo, , 
   Windows 10 x64 Shippable, , 
   Windows 10 x64 QuantumRender Shippable, , 
   Windows 10 x64 QuantumRender, , 
   Windows 10 x64 2017 Ref HW, , 
   Windows 10 x64 MinGW, Tom Ritter, the Tor project uses MinGW; make sure we test that for them
   Windows 10 AArch64, , 
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
   Windows 2012 x64 asan reporter, Christian Holler, Generate :ref:`ASan Nightly Project <ASan Nightly>` builds
   Windows 2012 x64 addon, ,
   Windows 2012 x64 NoOpt, ,
   Windows 2012 x64 DevEdition, ,
   Windows MinGW, Tom Ritter, the Tor project uses MinGW; make sure we test that for them
   Android 4.0 API16+, , 
   Android 4.0 API16+ Beta, , 
   Android 4.0 API16+ Release, ,
   Android 4.0 API16+ CCov, Marco Castelluccio , Collect :ref:`Code coverage` information to identify what is tested (or not)
   Android 4.0 API16+ GeckoView multi-arch fat AAR, , 
   Android 4.2 x86, , 
   Android 4.2 x86 Beta, , 
   Android 4.2 x86 Release, , 
   Android 4.2 x86, , 
   Android 4.2 x86 Beta, , 
   Android 4.2 x86 Release, , 
   Android 4.3 API16+, , 
   Android 4.3 API16+ Beta, , 
   Android 4.3 API16+ CCov, Marco Castelluccio , Collect :ref:`Code coverage` information to identify what is tested (or not)
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
   Android 7.0 x86-64 QuantumRender, , 
   Android 7.0 x86-64 Beta, , 
   Android 7.0 x86-64 Release, , 
   Android 7.0 MotoG5, , 
   Android 8.0 Pixel2, , 
   Android 8.0 Pixel2 QuantumRender, , 
   Android 8.0 Pixel2 Nightly, ,
   Android 8.0 Pixel2 AArch64, , 
   Android 8.0 Pixel2 AArch64 QuantumRender, , 
   Android 8.0 Pixel2 AArch64 Nightly, ,
   Android, , 
   Gecko Decision Task, , Define the tasks to run and their order
   Firefox Release Tasks, ,
   Devedition Release Tasks, ,
   Fennec Beta Tasks, , 
   Fennec Release Tasks, , 
   Thunderbird Release Tasks, , 
   Diffoscope, Mike Hommey, Make sure the build remains reproducible
   Linting, "| Sylvestre Ledru
   | Andrew Halberstadt", "| Identify :ref:`code quality` earlier
   | Also contains some Bugzilla and :ref:`Documentation jobs<Managing Documentation>`"
   Fetch, ,
   Docker Images, , 
   Packages, ,
   Toolchains, , 
   Other, ,
