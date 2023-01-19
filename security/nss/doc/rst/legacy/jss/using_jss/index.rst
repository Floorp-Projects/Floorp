.. _mozilla_projects_nss_jss_using_jss:

Using JSS
=========

.. _using_jss:

`Using JSS <#using_jss>`__
--------------------------

.. container::

   *Newsgroup:*\ `mozilla.dev.tech.crypto <news://news.mozilla.org:119/mozilla.dev.tech.crypto>`__

   If you have already `built
   JSS <https://developer.mozilla.org/en-US/docs/JSS/Build_instructions_for_JSS_4.3.x>`__, or if you
   are planning to use a binary release of JSS, here's how to get JSS working with your code.

   | `Gather Components <#components>`__
   | `Setup your runtime environment <#runtime>`__
   | `Initialize JSS in your application <#init>`__

.. _gather_components:

`Gather components <#gather_components>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   #. You need the JSS classes and the NSPR, NSS, and JSS shared libraries.

   #. **NSPR and NSS Shared Libraries**

      JSS uses the NSPR and NSS libraries for I/O and crypto. JSS version 3.0 linked statically with
      NSS, so it only required NSPR. JSS versions 3.1 and later link dynamically with NSS, so they
      also require the NSS shared libraries.

      The exact library names vary according to the convention for each platform. For example, the
      NSPR library is called ``nspr4.dll`` or ``libnspr4.dll`` on Windows and ``libnspr4.so`` on
      Solaris. The following table gives the core names of the libraries, omitting the
      platform-specific prefix and suffix.

      +-------------------+-------------------------------------+--------------------------------------+
      | JSS Dependencies  |                                     |                                      |
      +-------------------+-------------------------------------+--------------------------------------+
      | Core Library Name | Description                         | Binary Release Location              |
      +-------------------+-------------------------------------+--------------------------------------+
      | nspr4             | NSPR OS abstraction layer           | `htt                                 |
      |                   |                                     | p://ftp.mozilla.org/pub/mozilla.org/ |
      |                   |                                     | nspr/releases <http://ftp.mozilla.or |
      |                   |                                     | g/pub/mozilla.org/nspr/releases/>`__ |
      +-------------------+-------------------------------------+--------------------------------------+
      | plc4              |                                     | NSPR standard C library replacement  |
      |                   |                                     | functions                            |
      +-------------------+-------------------------------------+--------------------------------------+
      | plds4             |                                     | NSPR data structure types            |
      +-------------------+-------------------------------------+--------------------------------------+
      | nss3              | NSS crypto, PKCS #11, and utilities | `http://ftp.mozilla.                 |
      |                   |                                     | org/pub/mozilla.org/security/nss/rel |
      |                   |                                     | eases <http://ftp.mozilla.org/pub/mo |
      |                   |                                     | zilla.org/security/nss/releases/>`__ |
      +-------------------+-------------------------------------+--------------------------------------+
      | ssl3              |                                     | NSS SSL library                      |
      +-------------------+-------------------------------------+--------------------------------------+
      | smime3            |                                     | NSS S/MIME functions and types       |
      +-------------------+-------------------------------------+--------------------------------------+
      | nssckbi           |                                     | PKCS #11 module containing built-in  |
      |                   |                                     | root CA certificates. Optional.      |
      +-------------------+-------------------------------------+--------------------------------------+
      | freebl_\*         |                                     | Processor-specific optimized         |
      |                   |                                     | big-number arithmetic library. Not   |
      |                   |                                     | present on all platforms.            |
      |                   |                                     | :ref:`mozilla_projects_nss_introd    |
      |                   |                                     | uction_to_network_security_services` |
      +-------------------+-------------------------------------+--------------------------------------+
      | fort              |                                     | FORTEZZA support. Optional           |
      +-------------------+-------------------------------------+--------------------------------------+
      | swft              |                                     | PKCS #11 module implementing         |
      |                   |                                     | FORTEZZA in software. Optional.      |
      +-------------------+-------------------------------------+--------------------------------------+

      If you built JSS from source, you have these libraries in the ``mozilla/dist/<platform>/lib``
      directory of your build tree. If you are downloading binaries, get them from the binary
      release locations in the above table. You need to select the right version of the components,
      based on the version of JSS you are using. Generally, it is safe to use a later version of a
      component than what JSS was tested with. For example, although JSS 4.2 was tested with NSS
      3.11.

      ================== ========= ==============
      Component Versions
      JSS Version        Component Tested Version
      JSS 4.2            NSPR      4.6.4
      \                  NSS       3.11.4
      JSS 3.4            NSPR      4.2.2
      \                  NSS       3.7.3
      JSS 3.3            NSPR      4.2.2
      \                  NSS       3.6.1 or 3.7
      JSS 3.2            NSPR      4.2 or 4.1.2
      \                  NSS       3.4.2
      JSS 3.1.1          NSPR      4.1.2
      \                  NSS       3.3.1
      JSS 3.1            NSPR      4.1.2
      \                  NSS       3.3
      JSS 3.0            NSPR      3.5.1
      ================== ========= ==============

   #. **JSS Shared Library**

      The JSS shared library is ``jss4.dll`` (Windows) or ``libjss4.so`` (Unix). If you built JSS
      from source, it is in ``mozilla/dist/<platform>/lib``. If you are downloading binaries, get it
      from http://ftp.mozilla.org/pub/mozilla.org/security/jss/releases/.

   #. **JSS classes**

      If you built JSS from source, the compiled JSS classes are in ``mozilla/dist/classes[_dbg]``.
      You can put this directory in your classpath to run applications locally; or, you can package
      the class files into a JAR file for easier distribution:

         .. code::

            cd mozilla/dist/classes[_dbg]
            zip -r ../jss42.jar .

      If you are downloading binaries, get jss42.jar
      from http://ftp.mozilla.org/pub/mozilla.org/security/jss/releases/.

.. _setup_your_runtime_environment:

`Setup your runtime environment <#setup_your_runtime_environment>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   You need to set some environment variables before building and running Java applications with
   JSS.

   ``CLASSPATH``
      Include the path containing the JSS classes you built, or the path to ``jss42.jar``. (The path
      to ``jss34.jar`` ends with the string "/jss42.jar". It is not just the directory that contains
      ``jss42.jar``.)
   ``LD_LIBRARY_PATH`` (Unix) / ``PATH`` (Windows)
      Include the path to the NSPR, NSS, and JSS shared libraries.

.. _initialize_jss_in_your_application:

`Initialize JSS in your application <#initialize_jss_in_your_application>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Before calling any JSS methods, you must initialize JSS by calling one of the
   ``CryptoManager.initialize`` methods. See the `javadoc <javadoc>`__ for more details.