.. _mozilla_projects_nss_jss_build_instructions_for_jss_4_3_x:

Build instructions for JSS 4.3.x
================================

.. _build_instructions_for_jss_4.3.x:

`Build Instructions for JSS 4.3.x <#build_instructions_for_jss_4.3.x>`__
------------------------------------------------------------------------

.. container::

   Newsgroup: `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__

   Before building JSS, you need to set up your system as follows:

   #. Build NSPR/NSS by following the
      :ref:`mozilla_projects_nss_reference_building_and_installing_nss_build_instructions`,
   #. To check that NSS built correctly, run ``all.sh`` (in ``mozilla/security/nss/tests``) and
      examine the results (in
      ``mozilla/test_results/security/``\ *computername*.#\ ``/results.html``.
   #. Install a Java compiler and runtime. JSS supports Java version 1.5 or later. We suggest you
      use the latest.
   #. You must have Perl version 5.005 or later.

   Now you are ready to build JSS. Follow these steps:

   #. Switch to the appropriate directory and check out JSS from the root of your source tree.

      .. code::

         cvs co -r JSS_4_3_1_RTM mozilla/security/jss

      or

      .. code::

         cvs co -r JSS_4_3_RTM mozilla/security/jss

   #. Setup environment variables needed for compiling Java source. The ``JAVA_HOME`` variable
      indicates the directory containing your Java SDK installation. Note, on Windows platforms it
      is best to have JAVA_HOME set to a directory path that doest not have spaces. 

      **Unix**

      .. code::

         setenv JAVA_HOME /usr/local/jdk1.5.0 (or wherever your JDK is installed)

      **Windows**

      .. code::

         set JAVA_HOME=c:\programs\jdk1.5.0 (or wherever your JDK is installed)

      **Windows (Cygnus)**

      .. code::

         JAVA_HOME=/cygdrive/c/programs/jdk1.5.0 (or wherever your JDK is installed)
         export JAVA_HOME

      | **Windows build Configurations WINNT vs WIN95**

      .. code::

         As of NSS 3.15.4, NSPR/NSS/JSS build generates a "WIN95" configuration by default on Windows.
         We recommend most applications use the "WIN95" configuration. If you want JSS to be used
         with your applet and the Firefox browser than you must build WIN95. (See JSS FAQ)
         The "WIN95" configuration supports all versions of Windows. The "WIN95" name is historical;
         it should have been named "WIN32".
         To generate a "WINNT" configuration, set OS_TARGET=WINNT and build NSPR/NSS/JSS WIN95.

      | Mac OS X
      | It has been recently reported that special build instructions are necessary to succeed
        building JSS on OSX. Please
        see `HOWTO_successfully_compile_JSS_and_NSS_for_32_and_64_bits_on_OSX_10.6_(10.6.7) </HOWTO_successfully_compile_JSS_and_NSS_for_32_and_64_bits_on_OSX_10.6_(10.6.7)>`__
        for contributed instructions.
      |  

   #. Build JSS.

         .. code::

            cd mozilla/security/jss
            gmake

   #. Sign the JSS jar.

         .. code::

            If you're intention is to modify and build the JSS source you
            need to Apply for your own  JCE code-signing certificate

            If you made no changes and your goal is to build JSS you can use the
            signed binary release of the jss4.jar from ftp.mozilla.org.
            with your built jss4 JNI shared library.

   Next, you should read the instructions on `using JSS <Using_JSS>`__.