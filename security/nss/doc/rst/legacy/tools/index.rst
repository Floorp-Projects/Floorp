.. _mozilla_projects_nss_tools:

NSS Tools
=========

.. _nss_security_tools:

`NSS Security Tools <#nss_security_tools>`__
--------------------------------------------

.. container::

   Newsgroup: `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__

`Overview <#overview>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The NSS Security Tools allow developers to test, debug, and manage applications that use NSS. The
   `Tools Information <#tools>`__ table below describes both the tools that are currently working
   and those that are still under development. The links for each tool take you to the source code,
   documentation, plans, and related links for each tool. The links will become active when
   information is available.

   Currently, you must download the NSS 3.1 source and build it to create binary files for the NSS
   tools. For information about downloading the NSS source, see
   :ref:`mozilla_projects_nss_building`.

   If you have feedback or questions, please feel free to post to
   `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__. This newsgroup is
   the preferred forum for all questions about NSS and NSS tools.

.. _overall_objectives:

`Overall Objectives <#overall_objectives>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   #. Provide a tool for analyzing and repairing certificate databases (`dbck <#dbck>`__).
   #. Migrate tools from secutil.h interface to PKCS #11 interface.
   #. Eliminate redundant functionality in tools. Many tools implement private versions of
      PKCS11Init(), OpenCertDB(), etc.
   #. Eliminate use of getopt() and replace with NSPR calls to get command options (to eliminate
      platform dependencies with getopt()).

.. _tools_information:

`Tools information <#tools_information>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +--------------+-----------------------------------------+-----------------------------------------+
   | **Tool**     | **Description**                         | **Links**                               |
   +--------------+-----------------------------------------+-----------------------------------------+
   | certutil 2.0 | Manage certificate and key databases    | `                                       |
   |              | (cert7.db and key3.db).                 | Source <https://dxr.mozilla.org/mozilla |
   |              |                                         | /source/security/nss/cmd/certutil/>`__, |
   |              |                                         | :ref                                    |
   |              |                                         | :`mozilla_projects_nss_tools_certutil`, |
   |              |                                         | :ref:`moz                               |
   |              |                                         | illa_projects_nss_tools_certutil-tasks` |
   +--------------+-----------------------------------------+-----------------------------------------+
   | cmsutil 1.0  | Performs basic CMS operations such as   | `So                                     |
   |              | encrypting, decrypting, and signing     | urce <https://dxr.mozilla.org/mozilla/s |
   |              | messages.                               | ource/security/nss/cmd/smimetools/>`__, |
   |              |                                         | :r                                      |
   |              |                                         | ef:`mozilla_projects_nss_tools_cmsutil` |
   +--------------+-----------------------------------------+-----------------------------------------+
   | crlutil      | Manage certificate revocation lists     | `Source <https://dxr.mozilla.org/mozill |
   |              | (CRLs).                                 | a/source/security/nss/cmd/crlutil/>`__, |
   |              |                                         | :re                                     |
   |              |                                         | f:`mozilla_projects_nss_tools_crlutil`, |
   +--------------+-----------------------------------------+-----------------------------------------+
   | dbck 1.0     | Analyze and repair certificate          | `Source <https://dxr.mozilla.org/moz    |
   |              | databases (not working in NSS 3.2)      | illa/source/security/nss/cmd/dbck/>`__, |
   |              |                                         | :ref:                                   |
   |              |                                         | `mozilla_projects_nss_tools_dbck-tasks` |
   +--------------+-----------------------------------------+-----------------------------------------+
   | modutil 1.1  | Manage the database of PKCS11 modules   | `Source <https://dxr.mozilla.org/mozill |
   |              | (secmod.db). Add modules and modify the | a/source/security/nss/cmd/modutil/>`__, |
   |              | properties of existing modules (such as | :re                                     |
   |              | whether a module is the default         | f:`mozilla_projects_nss_tools_modutil`, |
   |              | provider of some crypto service).       | :ref:`mo                                |
   |              |                                         | zilla_projects_nss_tools_modutil-tasks` |
   +--------------+-----------------------------------------+-----------------------------------------+
   | pk12util 1.0 | Import and export keys and certificates | `                                       |
   |              | between the cert/key databases and      | Source <https://dxr.mozilla.org/mozilla |
   |              | files in PKCS12 format.                 | /source/security/nss/cmd/pk12util/>`__, |
   |              |                                         | :ref                                    |
   |              |                                         | :`mozilla_projects_nss_tools_pk12util`, |
   |              |                                         | :ref:`moz                               |
   |              |                                         | illa_projects_nss_tools_pk12util-tasks` |
   +--------------+-----------------------------------------+-----------------------------------------+
   | signtool 1.3 | Create digitally-signed jar archives    | `                                       |
   |              | containing files and/or code.           | Source <https://dxr.mozilla.org/mozilla |
   |              |                                         | /source/security/nss/cmd/signtool/>`__, |
   |              |                                         | `Do                                     |
   |              |                                         | cumentation <https://docs.oracle.com/ja |
   |              |                                         | vase/8/docs/technotes/guides/security/S |
   |              |                                         | ecurityToolsSummary.html#jarsigner>`__, |
   +--------------+-----------------------------------------+-----------------------------------------+
   | signver 1.1  | Verify signatures on digitally-signed   | `Source <https://dxr.mozilla.org/mozill |
   |              | objects.                                | a/source/security/nss/cmd/signver/>`__, |
   |              |                                         | `Document                               |
   |              |                                         | ation <https://docs.oracle.com/javase/t |
   |              |                                         | utorial/deployment/jar/verify.html>`__, |
   |              |                                         | :ref:`mo                                |
   |              |                                         | zilla_projects_nss_tools_signver-tasks` |
   +--------------+-----------------------------------------+-----------------------------------------+
   | sslstrength  | SSL Strength                            | :ref:`                                  |
   |              |                                         | mozilla_projects_nss_tools_sslstrength` |
   +--------------+-----------------------------------------+-----------------------------------------+
   | ssltap 3.2   | Proxy requests for an SSL server and    | `Source <https://dxr.mozilla.org/mozil  |
   |              | display the contents of the messages    | la/source/security/nss/cmd/ssltap/>`__, |
   |              | exchanged between the client and        | :                                       |
   |              | server. The ssltap tool does not        | ref:`mozilla_projects_nss_tools_ssltap` |
   |              | decrypt data, but it shows things like  |                                         |
   |              | the type of SSL message (clientHello,   |                                         |
   |              | serverHello, etc) and connection data   |                                         |
   |              | (protocol version, cipher suite, etc).  |                                         |
   |              | This tool is very useful for debugging. |                                         |
   +--------------+-----------------------------------------+-----------------------------------------+