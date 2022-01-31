.. _mozilla_projects_nss_python_binding_for_nss:

Python binding for NSS
======================

.. _project_information:

`Project Information <#project_information>`__
----------------------------------------------

.. container::

   python-nss is a Python binding for NSS (Network Security Services) and NSPR (Netscape Portable
   Runtime). NSS provides cryptography services supporting SSL, TLS, PKI, PKIX, X509, PKCS*, etc.
   NSS is an alternative to OpenSSL and used extensively by major software projects. NSS is FIPS-140
   certified.

   NSS is built upon NSPR because NSPR provides an abstraction of common operating system services,
   particularly in the areas of networking and process management. Python also provides an
   abstraction of common operating system services but because NSS and NSPR are tightly bound
   python-nss exposes elements of NSPR.

   For information on NSS and NSPR, see the following:

   -  :ref:`mozilla_projects_nss`. NSS project page.
   -  `Netscape Portable Runtime </docs/NSPR>`__. NSPR project page.
   -  `NSPR Reference </docs/NSPR_API_Reference>`__. NSPR API documentation.

.. _design_goals:

`Design Goals <#design_goals>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSS and NSPR are C language API's which python-nss "wraps" and exposes to Python programs. The
   design of python-nss follows these basic guiding principles:

   -  Be a thin layer with almost a one-to-one mapping of NSS/NSPR calls to python methods and
      functions. Programmers already familiar with NSS/NSPR will be quite comfortable with
      python-nss.
   -  Be "Pythonic". The term Pythonic means to follow accepted Python paradigms and idoms in the
      Python language and libraries. Thus when deciding if the NSS/NSPR API should be rigidly
      followed or a more Pythonic API provided the Pythonic implementation wins because Python
      programmers do not want to write C programs in Python, rather they want their Python code to
      feel like Python code with the richness of full Python.
   -  Identifer names follow the preferred Python style instead of the style in the NSS/NSPR C
      header files.

      -  Classes are camel-case. Class names always begin with a upper case letter and are then
         followed by a mix of lower and upper case letters, a upper case letter is used to separate
         words. Acronyms always appear as a contiguous string of upper case letters.
      -  Method, function and property names are always lower case with words separated by
         underscores.
      -  Constants are all upper case with words separated by underscores, they match the NSS/NSPR C
         API.

   -  Every module, class, function, and method has associated documentation and is exposed via the
      standard Python methodology. This documentation is available via the numerous Python
      documentation extraction tools. Also see the `generated HTML
      documentation <https://mozilla.github.io/python-nss-docs/>`__ provided with each release.
   -  NSS/NSPR structs are exposed as Python objects.
   -  NSS/NSPR functions which operate on a NSS/NSPR object (i.e. struct) become methods of that
      object.
   -  NSS/NSPR objects which are collections support the Python iteration protocol. In other words
      they can be iterated over, indexed by position, or used as slices.
   -  NSS/NSPR objects whose collection elements can be referenced by name support associative
      indexing.
   -  NSS/NSPR objects which have "get" and "set" API function calls are exposed as Python
      properties.
   -  All NSS/NSPR Python objects can print their current value by evaluting the Python object in a
      string context or by using the Python str() function.
   -  Support threading. The Python Global Interpreter Lock (GIL) is released prior to calling
      NSS/NSPR C functions and reaquired after the NSS/NSPR C function returns. This allows other
      Python threads to execute during the time a NSS/NSPR function is progress in another thread.
      Also, any "global" values which are set in python-nss are actually thread-local. Examples of
      this are the various callbacks which can be set and their parameters. Thus each thread gets it
      own set of callbacks.
   -  Many methods/functions provide sane default (keyword) parameters freeing the Python programmer
      from having to specify all parameters yet allowing them to be overriden when necessary.
   -  Error codes are *never* returned from methods/functions. python-nss follows the existing
      Python exception mechanism. Any error reported by NSS/NSPR is converted into a Python
      exception and raised. The exact error code, error description, and often contextual error
      information will be present in the exception object.
   -  Enumerated constants used in the NSS/NSPR API's are available in the Python module under the
      *exact* same name as they appear in the C header files of NSS/NSPR.
   -  Convenience functions are provided to translate between the numeric value of an enumerated
      constant and it's string representation and visa versa.
   -  python-nss internally supports UTF-8. Strings may be Python str objects or Python unicode
      objects. If a Python unicode object is passed to a NSS/NSPR function it will be encoded as
      UTF-8 first before being passed to NSS/NSPR.
   -  python-nss tries to be flexible when generating a print representation of complex objects. For
      simplicity you can receive a block of formatted text but if you need more control, such as
      when building GUI elments you can access a list of "lines", each line is paired with an
      indentation level value. The (indent, text) pairs allow you to insert the item into a GUI tree
      structure or simply change the indentation formatting.
   -  Deprecated elements of the python-nss API are marked with Python deprecation warnings as well
      as being documented in the nss module documentation. As of Python 2.7 deprecation warnings are
      no longer reported by default. It is suggested Python developers using python-nss periodically
      run their code with deprecation warnings enabled. Depercated elements will persist for a least
      two releases before being removed from the API entirely.

.. _project_history:

`Project History <#project_history>`__
--------------------------------------

.. container::

   Red Hat utilizes both NSS and Python in many of it's projects, however it was not previously
   possible to call NSS directly from Python. To solve this problem Red Hat generously funded the
   initial development of python-nss as well as it's continued maintenance. Red Hat following it's
   open source philosophy has contributed the source to the Mozilla security project. Red Hat
   welcomes all interested contributors who would like to contribute the python-nss project as part
   of an open source community. The initial release of python-nss occurred in September 2008 with
   it's inclusion in the Fedora distribution. The source code to python-nss was first imported into
   the Mozilla CVS repository on June 9th 2009. python-nss is currently available in:

   -  Fedora
   -  RHEL 6

   The principal developer of python-nss is John Dennis jdennis@redhat.com. Additional contributors
   are:

   -  Miloslav Trmač mitr@redhat.com
   -  Bohuslav Kabrda slavek@redhat.com

   The python-nss binding is still young despite having been utilized in several major software
   projects. Thus it's major version number is still at zero. This is primarily so the developers
   can make changes to the API as experiece grows with it. For example it is already known there are
   some naming inconsistencies. Elments of the API are probably not ideally partitioned into proper
   namespaces via Python modules. Some functionality and interface have already been deprecated due
   to lessons learned. Thus at some point in the future when it is felt the API has solidified and
   been further proven in the field a 1.0 release will be made. At that point in time existing users
   of the python-nss API will need to some elements of their code. A migration script will be
   provided to assist them.

.. _licensing_information:

`Licensing Information <#licensing_information>`__
--------------------------------------------------

.. container::

   python-nss is available under the Mozilla Public License, the GNU General Public License, and the
   GNU Lesser General Public License. For information on downloading python-nss releases as tar
   files, see `Source Download <#sourcedownload>`__.

`Documentation <#documentation>`__
----------------------------------

.. container::

   .. rubric:: python-nss API documentation
      :name: python-nss_api_documentation

   The python-nss API documentation for the current release can be viewed at `python-nss API
   documentation <https://mozilla.github.io/python-nss-docs/>`__.

   The API documentation is generated from the python-nss source code and compiled modules. You can
   build it yourself via ``./setup.py build_doc``. Most distributions include the python-nss API
   documentation in the python-nss packaging. Consult your distribution for more information.

   .. rubric:: Example Code
      :name: example_code

   The doc/examples directory contains numerous examples of python-nss programs and libraries you
   may wish to consult. They illustrate suggested usage and best practice.

   .. rubric:: Test Code
      :name: test_code

   In addition the test directory contains unit tests that also illustrate python-nss usage, however
   unlike the examples the unit tests are geared towards testing rather than expository
   illustration.

   .. rubric:: Other Documentation
      :name: other_documentation

   The doc directory contains other files you may wish to review.

.. _how_to_report_a_bug:

`How to Report a Bug <#how_to_report_a_bug>`__
----------------------------------------------

.. container::

   python-nss bugs are currently being tracked in the Red Hat bugzilla system for Fedora. You can
   enter a bug report
   `here <https://bugzilla.redhat.com/enter_bug.cgi?product=Fedora;component=python-nss>`__.

.. _source_download_area:

`Source Download Area <#source_download_area>`__
------------------------------------------------

.. container::

   Source downloads are maintained
   `here <https://ftp.mozilla.org/pub/mozilla.org/security/python-nss/releases/>`__. Links to
   download URL for a specific release can be found in the `Release Information <#release_info>`__
   section.

.. _mozilla_source_code_management_(scm)_information:

`Mozilla Source Code Management (SCM) Information <#mozilla_source_code_management_(scm)_information>`__
--------------------------------------------------------------------------------------------------------

.. container::

   On March 21, 2013 the NSS project switched from using CVS as it's source code manager (SCM) to
   Mercurial, also known as ``hg``. All prior CVS information (including release tags) were imported
   into the new Mercurial repositories, as such there is no need to utilize the deprecated CVS
   repositories, use Mercurial instead.

   To check out python-nss source code from Mercurial do this:

   ``hg clone https://hg.mozilla.org/projects/python-nss``

   The SCM tags for various python-nss releases can be found in the `Release
   Information <#release_info>`__.

   You may want to review the `Getting Mozilla Source Code Using
   Mercurial <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Source_Code/Mercurial>`__
   documentation for more information with working with Mercurial.

   The old deprecated CVS documentation can be found here: `Getting Mozilla Source Code Using
   CVS <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Source_Code/CVS>`__.

   The old deprecated python-nss CVS source code location is ``mozilla/security/python/nss``.

.. _release_information:

`Release Information <#release_information>`__
----------------------------------------------

.. container::

.. _release_1.0.1:

`Release 1.0.1 <#release_1.0.1>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2017-02-28                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         | PYNSS_RELEASE_1_0_1                             |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 | https://ftp.mozilla.org/pub/mozilla.org/securi  |
   |                                                 | ty/python-nss/releases/PYNSS_RELEASE_1_0_1/src/ |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | -  Add TLS 1.3 cipher suites                    |
   |                                                 | -  ssl_cipher_info.py now attempts to enable    |
   |                                                 |    TLS 1.3                                      |
   |                                                 | -  Fix build issue in setup.py. python-nss can  |
   |                                                 |    now be build as Python wheel, e.g. \`pip     |
   |                                                 |    wheel -w dist .\`                            |
   |                                                 | -  The following constants were added:          |
   |                                                 |                                                 |
   |                                                 |    -  ssl.TLS_AES_128_GCM_SHA256                |
   |                                                 |    -  ssl.TLS_AES_256_GCM_SHA384                |
   |                                                 |    -  ssl.TLS_CHACHA20_POLY1305_SHA256          |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_1.0.0:

`Release 1.0.0 <#release_1.0.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2016-09-01                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         | PYNSS_RELEASE_1_0_0                             |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 | https://ftp.mozilla.org/pub/mozilla.org/securi  |
   |                                                 | ty/python-nss/releases/PYNSS_RELEASE_1_0_0/src/ |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | Official 1.0.0 release, only minor tweaks from  |
   |                                                 | the 1.0.0beta1 release.                         |
   |                                                 |                                                 |
   |                                                 | -  Allow custom include root in setup.py as     |
   |                                                 |    command line arg.                            |
   |                                                 | -  Add TLS chacha20 poly1305 constants.         |
   |                                                 | -  Remove checks for whether a socket is open   |
   |                                                 |    for reading. It's not possible for the       |
   |                                                 |    binding to know in all cases, especially if  |
   |                                                 |    the socket is created from an xternal socket |
   |                                                 |    passed in.                                   |
   |                                                 | -  The following module functions were added:   |
   |                                                 |                                                 |
   |                                                 |    -  nss.get_all_tokens                        |
   |                                                 |                                                 |
   |                                                 | -  The following constants were added:          |
   |                                                 |                                                 |
   |                                                 |    -                                            |
   |                                                 | ssl.TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 |
   |                                                 |    -  ss                                        |
   |                                                 | l.TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256 |
   |                                                 |    -                                            |
   |                                                 |   ssl.TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256 |
   |                                                 |    -                                            |
   |                                                 | ssl.TLS_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256 |
   |                                                 |    -                                            |
   |                                                 |   ssl.TLS_DHE_PSK_WITH_CHACHA20_POLY1305_SHA256 |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_1.0.0beta1:

`Release 1.0.0beta1 <#release_1.0.0beta1>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2016-02-16                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         | PYNSS_RELEASE_1_0_0beta1                        |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 | http                                            |
   |                                                 | s://ftp.mozilla.org/pub/mozilla.org/security/py |
   |                                                 | thon-nss/releases/PYNSS_RELEASE_1_0_0beta1/src/ |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | The primary enhancement in this version is      |
   |                                                 | support for Python3. A single code base         |
   |                                                 | supports both Py2 (minimum version 2.7) and Py3 |
   |                                                 |                                                 |
   |                                                 | -  When built for Py2:                          |
   |                                                 |                                                 |
   |                                                 |    -  text will be a Unicode object             |
   |                                                 |    -  binary data will be a str object          |
   |                                                 |    -  ints will be Python long object           |
   |                                                 |                                                 |
   |                                                 | -  When built for Py3:                          |
   |                                                 |                                                 |
   |                                                 |    -  text will be a str object                 |
   |                                                 |    -  binary data will be a bytes object        |
   |                                                 |    -  ints will be a Python int object          |
   |                                                 |                                                 |
   |                                                 | -  All pure Python tests and examples have been |
   |                                                 |    ported to Py3 syntax but should continue to  |
   |                                                 |    run under Py2.                               |
   |                                                 | -  The following class methods were added:      |
   |                                                 |                                                 |
   |                                                 |    -  PK11Slot.check_security_officer_passwd    |
   |                                                 |    -  PK11Slot.check_user_passwd                |
   |                                                 |    -  PK11Slot.change_passwd                    |
   |                                                 |    -  PK11Slot.init_pin                         |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.17.0:

`Release 0.17.0 <#release_0.17.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2014-11-07                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         | PYNSS_RELEASE_0_17_0                            |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 | https://ftp.mozilla.org/pub/mozilla.org/securit |
   |                                                 | y/python-nss/releases/PYNSS_RELEASE_0_17_0/src/ |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | The primary enhancement in this version is      |
   |                                                 | adding support for PBKDF2                       |
   |                                                 |                                                 |
   |                                                 | -  The following module functions were added:   |
   |                                                 |                                                 |
   |                                                 |    -  nss.create_pbev2_algorithm_id             |
   |                                                 |                                                 |
   |                                                 | -  The following class methods were added:      |
   |                                                 |                                                 |
   |                                                 |    -  nss.AlgorithmID.get_pbe_crypto_mechanism  |
   |                                                 |    -  nss.AlgorithmID.get_pbe_iv                |
   |                                                 |    -  nss.PK11Slot.pbe_key_gen                  |
   |                                                 |    -  nss.PK11Slot.format_lines                 |
   |                                                 |    -  nss.PK11Slot.format                       |
   |                                                 |    -  nss.Pk11SymKey.format_lines               |
   |                                                 |    -  nss.Pk11SymKey.format                     |
   |                                                 |    -  nss.SecItem.to_base64                     |
   |                                                 |    -  nss.SecItem.format_lines                  |
   |                                                 |    -  nss.SecItem.format                        |
   |                                                 |                                                 |
   |                                                 | -  The following files were added:              |
   |                                                 |                                                 |
   |                                                 |    -  doc/examples/pbkdf2_example.py            |
   |                                                 |                                                 |
   |                                                 | -  The SecItem constructor added 'ascii'        |
   |                                                 |    parameter to permit initialization from      |
   |                                                 |    base64 and/or PEM textual data.              |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.16.0:

`Release 0.16.0 <#release_0.16.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2014-10-29                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         | PYNSS_RELEASE_0_16_0                            |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 | https://ftp.mozilla.org/pub/mozilla.org/securit |
   |                                                 | y/python-nss/releases/PYNSS_RELEASE_0_16_0/src/ |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | The primary enhancements in this version is     |
   |                                                 | adding support for the setting trust attributes |
   |                                                 | on a Certificate, the SSL version range API,    |
   |                                                 | information on the SSL cipher suites and        |
   |                                                 | information on the SSL connection.              |
   |                                                 |                                                 |
   |                                                 | -  The following module functions were added:   |
   |                                                 |                                                 |
   |                                                 |    -  ssl.get_ssl_version_from_major_minor      |
   |                                                 |    -  ssl.get_default_ssl_version_range         |
   |                                                 |    -  ssl.get_supported_ssl_version_range       |
   |                                                 |    -  ssl.set_default_ssl_version_range         |
   |                                                 |    -  ssl.ssl_library_version_from_name         |
   |                                                 |    -  ssl.ssl_library_version_name              |
   |                                                 |    -  ssl.get_cipher_suite_info                 |
   |                                                 |    -  ssl.ssl_cipher_suite_name                 |
   |                                                 |    -  ssl.ssl_cipher_suite_from_name            |
   |                                                 |                                                 |
   |                                                 | -  The following deprecated module functions    |
   |                                                 |    were removed:                                |
   |                                                 |                                                 |
   |                                                 |    -  ssl.nssinit                               |
   |                                                 |    -  ssl.nss_ini                               |
   |                                                 |    -  ssl.nss_shutdown                          |
   |                                                 |                                                 |
   |                                                 | -  The following classes were added:            |
   |                                                 |                                                 |
   |                                                 |    -  SSLCipherSuiteInfo                        |
   |                                                 |    -  SSLChannelInfo                            |
   |                                                 |                                                 |
   |                                                 | -  The following class methods were added:      |
   |                                                 |                                                 |
   |                                                 |    -  Certificate.trust_flags                   |
   |                                                 |    -  Certificate.set_trust_attributes          |
   |                                                 |    -  SSLSocket.set_ssl_version_range           |
   |                                                 |    -  SSLSocket.get_ssl_version_range           |
   |                                                 |    -  SSLSocket.get_ssl_channel_info            |
   |                                                 |    -  SSLSocket.get_negotiated_host             |
   |                                                 |    -  SSLSocket.connection_info_format_lines    |
   |                                                 |    -  SSLSocket.connection_info_format          |
   |                                                 |    -  SSLSocket.connection_info_str             |
   |                                                 |    -  SSLCipherSuiteInfo.format_lines           |
   |                                                 |    -  SSLCipherSuiteInfo.format                 |
   |                                                 |    -  SSLChannelInfo.format_lines               |
   |                                                 |    -  SSLChannelInfo.format                     |
   |                                                 |                                                 |
   |                                                 | -  The following class properties were added:   |
   |                                                 |                                                 |
   |                                                 |    -  Certificate.ssl_trust_flags               |
   |                                                 |    -  Certificate.email_trust_flags             |
   |                                                 |    -  Certificate.signing_trust_flags           |
   |                                                 |    -  SSLCipherSuiteInfo.cipher_suite           |
   |                                                 |    -  SSLCipherSuiteInfo.cipher_suite_name      |
   |                                                 |    -  SSLCipherSuiteInfo.auth_algorithm         |
   |                                                 |    -  SSLCipherSuiteInfo.auth_algorithm_name    |
   |                                                 |    -  SSLCipherSuiteInfo.kea_type               |
   |                                                 |    -  SSLCipherSuiteInfo.kea_type_name          |
   |                                                 |    -  SSLCipherSuiteInfo.symmetric_cipher       |
   |                                                 |    -  SSLCipherSuiteInfo.symmetric_cipher_name  |
   |                                                 |    -  SSLCipherSuiteInfo.symmetric_key_bits     |
   |                                                 |    -  SSLCipherSuiteInfo.symmetric_key_space    |
   |                                                 |    -  SSLCipherSuiteInfo.effective_key_bits     |
   |                                                 |    -  SSLCipherSuiteInfo.mac_algorithm          |
   |                                                 |    -  SSLCipherSuiteInfo.mac_algorithm_name     |
   |                                                 |    -  SSLCipherSuiteInfo.mac_bits               |
   |                                                 |    -  SSLCipherSuiteInfo.is_fips                |
   |                                                 |    -  SSLCipherSuiteInfo.is_exportable          |
   |                                                 |    -  SSLCipherSuiteInfo.is_nonstandard         |
   |                                                 |    -  SSLChannelInfo.protocol_version           |
   |                                                 |    -  SSLChannelInfo.protocol_version_str       |
   |                                                 |    -  SSLChannelInfo.protocol_version_enum      |
   |                                                 |    -  SSLChannelInfo.major_protocol_version     |
   |                                                 |    -  SSLChannelInfo.minor_protocol_version     |
   |                                                 |    -  SSLChannelInfo.cipher_suite               |
   |                                                 |    -  SSLChannelInfo.auth_key_bits              |
   |                                                 |    -  SSLChannelInfo.kea_key_bits               |
   |                                                 |    -  SSLChannelInfo.creation_time              |
   |                                                 |    -  SSLChannelInfo.creation_time_utc          |
   |                                                 |    -  SSLChannelInfo.last_access_time           |
   |                                                 |    -  SSLChannelInfo.last_access_time_utc       |
   |                                                 |    -  SSLChannelInfo.expiration_time            |
   |                                                 |    -  SSLChannelInfo.expiration_time_utc        |
   |                                                 |    -  SSLChannelInfo.compression_method         |
   |                                                 |    -  SSLChannelInfo.compression_method_name    |
   |                                                 |    -  SSLChannelInfo.session_id                 |
   |                                                 |                                                 |
   |                                                 | -  The following files were added:              |
   |                                                 |                                                 |
   |                                                 |    -  doc/examples/cert_trust.py                |
   |                                                 |    -  doc/examples/ssl_version_range.py         |
   |                                                 |                                                 |
   |                                                 | -  The following constants were added:          |
   |                                                 |                                                 |
   |                                                 |    -  nss.CERTDB_TERMINAL_RECORD                |
   |                                                 |    -  nss.CERTDB_VALID_PEER                     |
   |                                                 |    -  nss.CERTDB_TRUSTED                        |
   |                                                 |    -  nss.CERTDB_SEND_WARN                      |
   |                                                 |    -  nss.CERTDB_VALID_CA                       |
   |                                                 |    -  nss.CERTDB_TRUSTED_CA                     |
   |                                                 |    -  nss.CERTDB_NS_TRUSTED_CA                  |
   |                                                 |    -  nss.CERTDB_USER                           |
   |                                                 |    -  nss.CERTDB_TRUSTED_CLIENT_CA              |
   |                                                 |    -  nss.CERTDB_GOVT_APPROVED_CA               |
   |                                                 |    -  ssl.SRTP_AES128_CM_HMAC_SHA1_32           |
   |                                                 |    -  ssl.SRTP_AES128_CM_HMAC_SHA1_80           |
   |                                                 |    -  ssl.SRTP_NULL_HMAC_SHA1_32                |
   |                                                 |    -  ssl.SRTP_NULL_HMAC_SHA1_80                |
   |                                                 |    -  ssl.SSL_CK_DES_192_EDE3_CBC_WITH_MD5      |
   |                                                 |    -  ssl.SSL_CK_DES_64_CBC_WITH_MD5            |
   |                                                 |    -  ssl.SSL_CK_IDEA_128_CBC_WITH_MD5          |
   |                                                 |    -  ssl.SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5  |
   |                                                 |    -  ssl.SSL_CK_RC2_128_CBC_WITH_MD5           |
   |                                                 |    -  ssl.SSL_CK_RC4_128_EXPORT40_WITH_MD5      |
   |                                                 |    -  ssl.SSL_CK_RC4_128_WITH_MD5               |
   |                                                 |                                                 |
   |                                                 |   -  ssl.SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA |
   |                                                 |    -  ssl.SSL_FORTEZZA_DMS_WITH_NULL_SHA        |
   |                                                 |    -  ssl.SSL_FORTEZZA_DMS_WITH_RC4_128_SHA     |
   |                                                 |    -  ssl.SSL_RSA_OLDFIPS_WITH_3DES_EDE_CBC_SHA |
   |                                                 |    -  ssl.SSL_RSA_OLDFIPS_WITH_DES_CBC_SHA      |
   |                                                 |    -  ssl.TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA |
   |                                                 |    -  ssl.TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA     |
   |                                                 |    -  ssl.TLS_DHE_DSS_WITH_AES_128_GCM_SHA256   |
   |                                                 |    -  ssl.TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA |
   |                                                 |    -  ssl.TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA |
   |                                                 |    -  ssl.TLS_DHE_DSS_WITH_DES_CBC_SHA          |
   |                                                 |    -  ssl.TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA |
   |                                                 |    -  ssl.TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA     |
   |                                                 |    -  ssl.TLS_DHE_RSA_WITH_AES_128_CBC_SHA256   |
   |                                                 |    -  ssl.TLS_DHE_RSA_WITH_AES_128_GCM_SHA256   |
   |                                                 |    -  ssl.TLS_DHE_RSA_WITH_AES_256_CBC_SHA256   |
   |                                                 |    -  ssl.TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA |
   |                                                 |    -  ssl.TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA |
   |                                                 |    -  ssl.TLS_DHE_RSA_WITH_DES_CBC_SHA          |
   |                                                 |    -  ssl.TLS_DH_ANON_WITH_CAMELLIA_128_CBC_SHA |
   |                                                 |    -  ssl.TLS_DH_ANON_WITH_CAMELLIA_256_CBC_SHA |
   |                                                 |    -  ssl.TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA  |
   |                                                 |    -  ssl.TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA      |
   |                                                 |    -  ssl.TLS_DH_DSS_WITH_CAMELLIA_128_CBC_SHA  |
   |                                                 |    -  ssl.TLS_DH_DSS_WITH_CAMELLIA_256_CBC_SHA  |
   |                                                 |    -  ssl.TLS_DH_DSS_WITH_DES_CBC_SHA           |
   |                                                 |    -  ssl.TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA  |
   |                                                 |    -  ssl.TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA      |
   |                                                 |    -  ssl.TLS_DH_RSA_WITH_CAMELLIA_128_CBC_SHA  |
   |                                                 |    -  ssl.TLS_DH_RSA_WITH_CAMELLIA_256_CBC_SHA  |
   |                                                 |    -  ssl.TLS_DH_RSA_WITH_DES_CBC_SHA           |
   |                                                 |    -  ssl.TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA |
   |                                                 |    -  ssl.TLS_DH_anon_EXPORT_WITH_RC4_40_MD5    |
   |                                                 |    -  ssl.TLS_DH_anon_WITH_3DES_EDE_CBC_SHA     |
   |                                                 |    -  ssl.TLS_DH_anon_WITH_AES_128_CBC_SHA      |
   |                                                 |    -  ssl.TLS_DH_anon_WITH_AES_256_CBC_SHA      |
   |                                                 |    -  ssl.TLS_DH_anon_WITH_CAMELLIA_128_CBC_SHA |
   |                                                 |    -  ssl.TLS_DH_anon_WITH_CAMELLIA_256_CBC_SHA |
   |                                                 |    -  ssl.TLS_DH_anon_WITH_DES_CBC_SHA          |
   |                                                 |    -  ssl.TLS_DH_anon_WITH_RC4_128_MD5          |
   |                                                 |                                                 |
   |                                                 |  -  ssl.TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 |
   |                                                 |                                                 |
   |                                                 |  -  ssl.TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 |
   |                                                 |    -  ssl.TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256 |
   |                                                 |    -  ssl.TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 |
   |                                                 |                                                 |
   |                                                 |   -  ssl.TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256 |
   |                                                 |    -  ssl.TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256  |
   |                                                 |    -  ssl.TLS_EMPTY_RENEGOTIATION_INFO_SCSV     |
   |                                                 |    -  ssl.TLS_FALLBACK_SCSV                     |
   |                                                 |    -  ssl.TLS_NULL_WITH_NULL_NULL               |
   |                                                 |    -  ssl.TLS_RSA_EXPORT_WITH_DES40_CBC_SHA     |
   |                                                 |    -  ssl.TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5    |
   |                                                 |    -  ssl.TLS_RSA_EXPORT_WITH_RC4_40_MD5        |
   |                                                 |    -  ssl.TLS_RSA_WITH_3DES_EDE_CBC_SHA         |
   |                                                 |    -  ssl.TLS_RSA_WITH_AES_128_CBC_SHA256       |
   |                                                 |    -  ssl.TLS_RSA_WITH_AES_128_GCM_SHA256       |
   |                                                 |    -  ssl.TLS_RSA_WITH_AES_256_CBC_SHA256       |
   |                                                 |    -  ssl.TLS_RSA_WITH_CAMELLIA_128_CBC_SHA     |
   |                                                 |    -  ssl.TLS_RSA_WITH_CAMELLIA_256_CBC_SHA     |
   |                                                 |    -  ssl.TLS_RSA_WITH_DES_CBC_SHA              |
   |                                                 |    -  ssl.TLS_RSA_WITH_IDEA_CBC_SHA             |
   |                                                 |    -  ssl.TLS_RSA_WITH_NULL_MD5                 |
   |                                                 |    -  ssl.TLS_RSA_WITH_NULL_SHA                 |
   |                                                 |    -  ssl.TLS_RSA_WITH_NULL_SHA256              |
   |                                                 |    -  ssl.TLS_RSA_WITH_RC4_128_MD5              |
   |                                                 |    -  ssl.TLS_RSA_WITH_RC4_128_SHA              |
   |                                                 |    -  ssl.TLS_RSA_WITH_SEED_CBC_SHA             |
   |                                                 |    -  ssl.SSL_VARIANT_DATAGRAM                  |
   |                                                 |    -  ssl.SSL_VARIANT_STREAM                    |
   |                                                 |    -  ssl.SSL_LIBRARY_VERSION_2                 |
   |                                                 |    -  ssl.SSL_LIBRARY_VERSION_3_0               |
   |                                                 |    -  ssl.SSL_LIBRARY_VERSION_TLS_1_0           |
   |                                                 |    -  ssl.SSL_LIBRARY_VERSION_TLS_1_1           |
   |                                                 |    -  ssl.SSL_LIBRARY_VERSION_TLS_1_2           |
   |                                                 |    -  ssl.SSL_LIBRARY_VERSION_TLS_1_3           |
   |                                                 |    -  ssl.ssl2                                  |
   |                                                 |    -  ssl.ssl3                                  |
   |                                                 |    -  ssl.tls1.0                                |
   |                                                 |    -  ssl.tls1.1                                |
   |                                                 |    -  ssl.tls1.2                                |
   |                                                 |    -  ssl.tls1.3                                |
   |                                                 |                                                 |
   |                                                 | -  The following methods were missing thread    |
   |                                                 |    locks, this has been fixed.                  |
   |                                                 |                                                 |
   |                                                 |    -  nss.nss_initialize                        |
   |                                                 |    -  nss.nss_init_context                      |
   |                                                 |    -  nss.nss_shutdown_context                  |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.15.0:

`Release 0.15.0 <#release_0.15.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2014-09-09                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         | PYNSS_RELEASE_0_15_0                            |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 | https://ftp.mozilla.org/pub/mozilla.org/securit |
   |                                                 | y/python-nss/releases/PYNSS_RELEASE_0_15_0/src/ |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | The primary enhancements in this version was    |
   |                                                 | fixing access to extensions in a                |
   |                                                 | CertificateRequest and giving access to         |
   |                                                 | CertificateRequest attributes.  There is a bug  |
   |                                                 | in NSS which hides the existence of extensions  |
   |                                                 | in a CSR if the extensions are not contained in |
   |                                                 | the first CSR  attribute. This was fixable in   |
   |                                                 | python-nss without requiring a patch  to NSS.   |
   |                                                 | Formerly python-nss did not provide access to   |
   |                                                 | the attributes in a CSR only the extensions,    |
   |                                                 | with this release all components of a  CSR can  |
   |                                                 | be accessed. See test/test_cert_request.py for  |
   |                                                 | examples.                                       |
   |                                                 |                                                 |
   |                                                 | -  Add ability to read PEM data from a string.  |
   |                                                 | -  Add more build instructions to README.       |
   |                                                 |    Source README into package long description. |
   |                                                 | -  A SecItem now converts almost all DER        |
   |                                                 |    encoded data to a string when it's str       |
   |                                                 |    method is invoked, formerly it was limited   |
   |                                                 |    to only a few objects.                       |
   |                                                 | -  The following classes were added:            |
   |                                                 |                                                 |
   |                                                 |    -  CERTAttribute                             |
   |                                                 |                                                 |
   |                                                 | -  The following class methods were added:      |
   |                                                 |                                                 |
   |                                                 |    -  CertAttribute.format_lines                |
   |                                                 |    -  CertAttribute.format                      |
   |                                                 |    -  nss.SecItem.get_integer                   |
   |                                                 |                                                 |
   |                                                 | -  The following class properties were added:   |
   |                                                 |                                                 |
   |                                                 |    -  CertificateRequest.attributes             |
   |                                                 |    -  CertAttribute.type_oid                    |
   |                                                 |    -  CertAttribute.type_tag                    |
   |                                                 |    -  CertAttribute.type_str                    |
   |                                                 |    -  CertAttribute.values                      |
   |                                                 |                                                 |
   |                                                 | -  The following module functions were added:   |
   |                                                 |                                                 |
   |                                                 |    -  base64_to_binary                          |
   |                                                 |                                                 |
   |                                                 | -  The following files were added:              |
   |                                                 |                                                 |
   |                                                 |    -  test_cert_request                         |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.14.1:

`Release 0.14.1 <#release_0.14.1>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2013-10-28                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         | PYNSS_RELEASE_0_14_1                            |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 | https://ftp.mozilla.org/pub/mozilla.org/securit |
   |                                                 | y/python-nss/releases/PYNSS_RELEASE_0_14_1/src/ |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | Release 0.14.1 contains only modifications to   |
   |                                                 | tests and examples, otherwise functionally it   |
   |                                                 | is the same as release 0.14.0                   |
   |                                                 |                                                 |
   |                                                 | -  Fix bug in ssl_example.py and                |
   |                                                 |    test_client_server.py where complete data    |
   |                                                 |    was not read from socket. The Beast CVE fix  |
   |                                                 |    in NSS causes only one octet to be sent in   |
   |                                                 |    the first socket packet and then the         |
   |                                                 |    remaining data is sent normally, this is     |
   |                                                 |    known as 1/n-1 record splitting. The example |
   |                                                 |    and test SSL code sent short messages and    |
   |                                                 |    then did a sock.recv(1024). We had always    |
   |                                                 |    received the entire message in one           |
   |                                                 |    sock.recv() call because it was so short.    |
   |                                                 |    But sock.recv() does not guarantee how much  |
   |                                                 |    data will be received, thus this was a       |
   |                                                 |    coding mistake. The solution is straight     |
   |                                                 |    forward, use newlines as a record separator  |
   |                                                 |    and call sock.readline() instead of          |
   |                                                 |    sock.recv(). sock.readline() calls           |
   |                                                 |    sock.recv() internally until a complete line |
   |                                                 |    is read or the socket is closed.             |
   |                                                 |                                                 |
   |                                                 | -  Rewrite setup_certs.py, it was written like  |
   |                                                 |    an expect script reacting to prompts read    |
   |                                                 |    from a pseudo terminal but it was fragile    |
   |                                                 |    and would hang on some systems. New version  |
   |                                                 |    uses temporary password file and writes      |
   |                                                 |    hardcoded responses to the stdin of certuil  |
   |                                                 |    and modutil.                                 |
   |                                                 |                                                 |
   |                                                 | -  setup_certs now creates a new sql sytle NSS  |
   |                                                 |    database (sql:pki)                           |
   |                                                 |                                                 |
   |                                                 | -  All tests and examples now load the sql:pki  |
   |                                                 |    database. Command line arg and variable      |
   |                                                 |    changed from dbdir to db_name to reflect the |
   |                                                 |    database specification is no longer just a   |
   |                                                 |    directory.                                   |
   |                                                 |                                                 |
   |                                                 | -  All command line process in test and         |
   |                                                 |    examples now uses modern argparse module     |
   |                                                 |    instead of deprecated getopt and optparse.   |
   |                                                 |    Some command line args were tweaked.         |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.14.0:

`Release 0.14.0 <#release_0.14.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Release Date

2013-05-10

SCM Tag

PYNSS_RELEASE_0_14_0

Source Download

https://ftp.mozilla.org/pub/mozilla.org/security/python-nss/releases/PYNSS_RELEASE_0_14_0/src/

Change Log

The primary enhancements in this version is support of certifcate validation, OCSP support, and
support for the certificate "Authority Information Access" extension.

Enhanced certifcate validation including CA certs can be done via Certificate.verify() or
Certificate.is_ca_cert(). When cert validation fails you can now obtain diagnostic information as to
why the cert failed to validate. This is encapsulated in the CertVerifyLog class which is a iterable
collection of CertVerifyLogNode objects. Most people will probablby just print the string
representation of the returned CertVerifyLog object. Cert validation logging is handled by the
Certificate.verify() method. Support has also been added for the various key usage and cert type
entities which feature prominently during cert validation.

-  Certificate() constructor signature changed from

   Certificate(data=None, der_is_signed=True)

   to

   Certificate(data, certdb=cert_get_default_certdb(), perm=False, nickname=None)

   This change was necessary because all certs should be added to the NSS temporary database when
   they are loaded, but earlier code failed to do that. It's is not likely that an previous code was
   failing to pass initialization data or the der_is_signed flag so this change should be backwards
   compatible.

-  Fix bug #922247, PKCS12Decoder.database_import() method. Importing into a NSS database would
   sometimes fail or segfault.

-  Error codes and descriptions were updated from upstream NSPR & NSS.

-  The password callback did not allow for breaking out of a password prompting loop, now if None is
   returned from the password callback the password prompting is terminated.

-  nss.nss_shutdown_context now called from InitContext destructor, this assures the context is
   shutdown even if the programmer forgot to. It's still best to explicitly shut it down, this is
   just failsafe.

-  Support was added for shutdown callbacks.

-  cert_dump.py extended to print NS_CERT_TYPE_EXTENSION

-  cert_usage_flags, nss_init_flags now support optional repr_kind parameter

-  The following classes were added:

   -  nss.CertVerifyLogNode
   -  nss.CertVerifyLog
   -  error.CertVerifyError (exception)
   -  nss.AuthorityInfoAccess
   -  nss.AuthorityInfoAccesses

-  The following class methods were added:

   -  nss.Certificate.is_ca_cert
   -  nss.Certificate.verify
   -  nss.Certificate.verify_with_log
   -  nss.Certificate.get_cert_chain
   -  nss.Certificate.check_ocsp_status
   -  nss.PK11Slot.list_certs
   -  nss.CertVerifyLogNode.format_lines
   -  nss.CertVerifyLog.format_lines
   -  nss.CRLDistributionPts.format_lines

-  The following class properties were added:

   -  nss.CertVerifyLogNode.certificate
   -  nss.CertVerifyLogNode.error
   -  nss.CertVerifyLogNode.depth
   -  nss.CertVerifyLog.count

-  The following module functions were added:

   -  nss.x509_cert_type
   -  nss.key_usage_flags
   -  nss.list_certs
   -  nss.find_certs_from_email_addr
   -  nss.find_certs_from_nickname
   -  nss.nss_get_version
   -  nss.nss_version_check
   -  nss.set_shutdown_callback
   -  nss.get_use_pkix_for_validation
   -  nss.set_use_pkix_for_validation
   -  nss.enable_ocsp_checking
   -  nss.disable_ocsp_checking
   -  nss.set_ocsp_cache_settings
   -  nss.set_ocsp_failure_mode
   -  nss.set_ocsp_timeout
   -  nss.clear_ocsp_cache
   -  nss.set_ocsp_default_responder
   -  nss.enable_ocsp_default_responder
   -  nss.disable_ocsp_default_responder

-  The following files were added:

   -  src/py_traceback.h
   -  doc/examples/verify_cert.py
   -  test/test_misc.py

-  The following constants were added:

   -  nss.KU_DIGITAL_SIGNATURE
   -  nss.KU_NON_REPUDIATION
   -  nss.KU_KEY_ENCIPHERMENT
   -  nss.KU_DATA_ENCIPHERMENT
   -  nss.KU_KEY_AGREEMENT
   -  nss.KU_KEY_CERT_SIGN
   -  nss.KU_CRL_SIGN
   -  nss.KU_ENCIPHER_ONLY
   -  nss.KU_ALL
   -  nss.KU_DIGITAL_SIGNATURE_OR_NON_REPUDIATION
   -  nss.KU_KEY_AGREEMENT_OR_ENCIPHERMENT
   -  nss.KU_NS_GOVT_APPROVED
   -  nss.PK11CertListUnique
   -  nss.PK11CertListUser
   -  nss.PK11CertListRootUnique
   -  nss.PK11CertListCA
   -  nss.PK11CertListCAUnique
   -  nss.PK11CertListUserUnique
   -  nss.PK11CertListAll
   -  nss.certUsageSSLClient
   -  nss.certUsageSSLServer
   -  nss.certUsageSSLServerWithStepUp
   -  nss.certUsageSSLCA
   -  nss.certUsageEmailSigner
   -  nss.certUsageEmailRecipient
   -  nss.certUsageObjectSigner
   -  nss.certUsageUserCertImport
   -  nss.certUsageVerifyCA
   -  nss.certUsageProtectedObjectSigner
   -  nss.certUsageStatusResponder
   -  nss.certUsageAnyCA
   -  nss.ocspMode_FailureIsVerificationFailure
   -  nss.ocspMode_FailureIsNotAVerificationFailure

Internal Changes

-  Reimplement exception handling

   -  NSPRError is now derived from StandardException instead of EnvironmentError. It was never
      correct to derive from EnvironmentError but was difficult to implement a new subclassed
      exception with it's own attributes, using EnvironmentError had been expedient.
   -  NSPRError now derived from StandardException, provides:

      -  errno (numeric error code)
      -  strerror (error description associated with error code)
      -  error_message (optional detailed message)
      -  error_code (alias for errno)
      -  error_desc (alias for strerror)

   -  CertVerifyError derived from NSPRError, extends with:

      -  usages (bitmask of returned usages)
      -  log (CertVerifyLog object)

-  Expose error lookup to sibling modules

-  Use macros for bitmask_to_list functions to reduce code duplication and centralize logic.

-  Add repr_kind parameter to cert_trust_flags_str()

-  Add support for repr_kind AsEnumName to bitstring table lookup.

-  Add cert_type_bitstr_to_tuple() lookup function

-  Add PRTimeConvert(), used to convert Python time values to PRTime, centralizes conversion logic,
   reduces duplication

-  Add UTF8OrNoneConvert to better handle unicode parameters which are optional.

-  Add Certificate_summary_format_lines() utility to generate concise certificate identification
   info for output.

-  Certificate_new_from_CERTCertificate now takes add_reference parameter to properly reference
   count certs, should fix shutdown busy problems.

-  Add print_traceback(), print_cert() debugging support.

.. _release_0.13.0:

`Release 0.13.0 <#release_0.13.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2012-10-09                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         | PYNSS_RELEASE_0_13_0                            |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 | https://ftp.mozilla.org/pub/mozilla.org/securit |
   |                                                 | y/python-nss/releases/PYNSS_RELEASE_0_13_0/src/ |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | -  Fix NSS SECITEM_CompareItem bug via          |
   |                                                 |    workaround.                                  |
   |                                                 | -  Fix incorrect format strings in              |
   |                                                 |    PyArg_ParseTuple\* for:                      |
   |                                                 |                                                 |
   |                                                 |    -  GeneralName                               |
   |                                                 |    -  BasicConstraints                          |
   |                                                 |    -  cert_x509_key_usage                       |
   |                                                 |                                                 |
   |                                                 | -  Fix bug when decoding certificate            |
   |                                                 |    BasicConstraints extension                   |
   |                                                 | -  Fix hang in setup_certs.                     |
   |                                                 | -  For NSS >= 3.13 support                      |
   |                                                 |    CERTDB_TERMINAL_RECORD                       |
   |                                                 | -  You can now query for a specific certificate |
   |                                                 |    extension Certficate.get_extension()         |
   |                                                 | -  The PublicKey formatting (i.e. format_lines) |
   |                                                 |    was augmented to format DSA keys (formerly   |
   |                                                 |    it only recognized RSA keys).                |
   |                                                 | -  Allow labels and values to be justified when |
   |                                                 |    printing objects                             |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following classes were added    |
   |                                                 |    :name: the_following_classes_were_added      |
   |                                                 |                                                 |
   |                                                 | -  RSAGenParams                                 |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following class methods were    |
   |                                                 |    added                                        |
   |                                                 |                                                 |
   |                                                 |   :name: the_following_class_methods_were_added |
   |                                                 |                                                 |
   |                                                 | -  nss.nss.Certificate.get_extension            |
   |                                                 | -  nss.nss.PK11Slot.generate_key_pair           |
   |                                                 | -  nss.nss.DSAPublicKey.format                  |
   |                                                 | -  nss.nss.DSAPublicKey.format_lines            |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following module functions were |
   |                                                 |    added                                        |
   |                                                 |    :                                            |
   |                                                 | name: the_following_module_functions_were_added |
   |                                                 |                                                 |
   |                                                 | -  nss.nss.pub_wrap_sym_key                     |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following internal utilities    |
   |                                                 |    were added                                   |
   |                                                 |    :na                                          |
   |                                                 | me: the_following_internal_utilities_were_added |
   |                                                 |                                                 |
   |                                                 | -  PyString_UTF8                                |
   |                                                 | -  SecItem_new_alloc()                          |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following class constructors    |
   |                                                 |    were modified to accept intialization        |
   |                                                 |    parameters                                   |
   |                                                 |    :name: the_following_class_constructors_w    |
   |                                                 | ere_modified_to_accept_intialization_parameters |
   |                                                 |                                                 |
   |                                                 | -  KEYPQGParams (DSA generation parameters)     |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following were deprecated       |
   |                                                 |    :name: the_following_were_deprecated         |
   |                                                 |                                                 |
   |                                                 | -  nss.nss.make_line_pairs (replaced by         |
   |                                                 |    nss.nss.make_line_fmt_tuples)                |
   |                                                 |                                                 |
   |                                                 | .. rubric:: Deprecated Functionality            |
   |                                                 |    :name: deprecated_functionality              |
   |                                                 |                                                 |
   |                                                 | make_line_pairs() has been replaced by          |
   |                                                 | make_line_fmt_tuples() because 2-valued tuples  |
   |                                                 | were not sufficently general. It is expected    |
   |                                                 | very few programs will have used this function, |
   |                                                 | it's mostly used internally but provided as a   |
   |                                                 | support utility.                                |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.12.0:

`Release 0.12.0 <#release_0.12.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2011-06-06                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         | PYNSS_RELEASE_0_12_0                            |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 | https://ftp.mozilla.org/pub/mozilla.org/securit |
   |                                                 | y/python-nss/releases/PYNSS_RELEASE_0_12_0/src/ |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | -  Major new enhancement is additon of PKCS12   |
   |                                                 |    support and AlgorithmID's.                   |
   |                                                 | -  setup.py build enhancements                  |
   |                                                 |                                                 |
   |                                                 |    -  Now searches for the NSS and NSPR header  |
   |                                                 |       files rather than hardcoding their        |
   |                                                 |       location. This makes building friendlier  |
   |                                                 |       on other systems (i.e. debian)            |
   |                                                 |    -  Now takes optional command line           |
   |                                                 |       arguments, -d or --debug will turn on     |
   |                                                 |       debug options during the build.           |
   |                                                 |                                                 |
   |                                                 | -  Fix reference counting bug in                |
   |                                                 |    PK11_password_callback() which contributed   |
   |                                                 |    to NSS not being able to shutdown due to     |
   |                                                 |    resources still in use.                      |
   |                                                 | -  Add UTF-8 support to                         |
   |                                                 |    ssl.config_server_session_id_cache()         |
   |                                                 | -  Added unit tests for cipher, digest,         |
   |                                                 |    client_server.                               |
   |                                                 | -  All unittests now run, added test/run_tests  |
   |                                                 |    to invoke full test suite.                   |
   |                                                 | -  Fix bug in test/setup_certs.py, hardcoded    |
   |                                                 |    full path to libnssckbi.so was causing       |
   |                                                 |    failures on 64-bit systems, just use the     |
   |                                                 |    libnssckbi.so basename, modutil will find it |
   |                                                 |    on the standard search path.                 |
   |                                                 | -  doc/examples/cert_dump.py uses new           |
   |                                                 |    AlgorithmID class to dump Signature          |
   |                                                 |    Algorithm                                    |
   |                                                 | -  doc/examples/ssl_example.py now can cleanly  |
   |                                                 |    shutdown NSS.                                |
   |                                                 | -  Exception error messages now include PR      |
   |                                                 |    error text if available.                     |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following classes were replaced |
   |                                                 |    :name: the_following_classes_were_replaced   |
   |                                                 |                                                 |
   |                                                 | -  SignatureAlgorithm replaced by new class     |
   |                                                 |    AlgorithmID                                  |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following classes were added    |
   |                                                 |    :name: the_following_classes_were_added_2    |
   |                                                 |                                                 |
   |                                                 | -  AlgorithmID                                  |
   |                                                 | -  PKCS12DecodeItem                             |
   |                                                 | -  PKCS12Decoder                                |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following class methods were    |
   |                                                 |    added                                        |
   |                                                 |                                                 |
   |                                                 | :name: the_following_class_methods_were_added_2 |
   |                                                 |                                                 |
   |                                                 | -  PK11Slot.authenticate()                      |
   |                                                 | -  PK11Slot.get_disabled_reason()               |
   |                                                 | -  PK11Slot.has_protected_authentication_path() |
   |                                                 | -  PK11Slot.has_root_certs()                    |
   |                                                 | -  PK11Slot.is_disabled()                       |
   |                                                 | -  PK11Slot.is_friendly()                       |
   |                                                 | -  PK11Slot.is_internal()                       |
   |                                                 | -  PK11Slot.is_logged_in()                      |
   |                                                 | -  PK11Slot.is_removable()                      |
   |                                                 | -  PK11Slot.logout()                            |
   |                                                 | -  PK11Slot.need_login()                        |
   |                                                 | -  PK11Slot.need_user_init()                    |
   |                                                 | -  PK11Slot.user_disable()                      |
   |                                                 | -  PK11Slot.user_enable()                       |
   |                                                 | -  PKCS12DecodeItem.format()                    |
   |                                                 | -  PKCS12DecodeItem.format_lines()              |
   |                                                 | -  PKCS12Decoder.database_import()              |
   |                                                 | -  PKCS12Decoder.format()                       |
   |                                                 | -  PKCS12Decoder.format_lines()                 |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following class properties were |
   |                                                 |    added                                        |
   |                                                 |    :                                            |
   |                                                 | name: the_following_class_properties_were_added |
   |                                                 |                                                 |
   |                                                 | -  AlgorithmID.id_oid                           |
   |                                                 | -  AlgorithmID.id_str                           |
   |                                                 | -  AlgorithmID.id_tag                           |
   |                                                 | -  AlgorithmID.parameters                       |
   |                                                 | -  PKCS12DecodeItem.certificate                 |
   |                                                 | -  PKCS12DecodeItem.friendly_name               |
   |                                                 | -  PKCS12DecodeItem.has_key                     |
   |                                                 | -  PKCS12DecodeItem.shroud_algorithm_id         |
   |                                                 | -  PKCS12DecodeItem.signed_cert_der             |
   |                                                 | -  PKCS12DecodeItem.type                        |
   |                                                 | -  SignedData.data                              |
   |                                                 | -  SignedData.der                               |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following module functions were |
   |                                                 |    added                                        |
   |                                                 |    :na                                          |
   |                                                 | me: the_following_module_functions_were_added_2 |
   |                                                 |                                                 |
   |                                                 | -  nss.nss.dump_certificate_cache_info()        |
   |                                                 | -  nss.nss.find_slot_by_name()                  |
   |                                                 | -  nss.nss.fingerprint_format_lines()           |
   |                                                 | -  nss.nss.get_internal_slot()                  |
   |                                                 | -  nss.nss.is_fips()                            |
   |                                                 | -  nss.nss.need_pw_init()                       |
   |                                                 | -  nss.nss.nss_init_read_write()                |
   |                                                 | -  nss.nss.pk11_disabled_reason_name()          |
   |                                                 | -  nss.nss.pk11_disabled_reason_str()           |
   |                                                 | -  nss.nss.pk11_logout_all()                    |
   |                                                 | -  nss.nss.pkcs12_cipher_from_name()            |
   |                                                 | -  nss.nss.pkcs12_cipher_name()                 |
   |                                                 | -  nss.nss.pkcs12_enable_all_ciphers()          |
   |                                                 | -  nss.nss.pkcs12_enable_cipher()               |
   |                                                 | -  nss.nss.pkcs12_export()                      |
   |                                                 | -  nss.nss.pkcs12_map_cipher()                  |
   |                                                 | -  n                                            |
   |                                                 | ss.nss.pkcs12_set_nickname_collision_callback() |
   |                                                 | -  nss.nss.pkcs12_set_preferred_cipher()        |
   |                                                 | -  nss.nss.token_exists()                       |
   |                                                 | -  nss.ssl.config_mp_server_sid_cache()         |
   |                                                 | -  ns                                           |
   |                                                 | s.ssl.config_server_session_id_cache_with_opt() |
   |                                                 | -  nss.ssl.get_max_server_cache_locks()         |
   |                                                 | -  nss.ssl.set_max_server_cache_locks()         |
   |                                                 | -  nss.ssl.shutdown_server_session_id_cache()   |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following constants were added  |
   |                                                 |    :name: the_following_constants_were_added    |
   |                                                 |                                                 |
   |                                                 | -  nss.nss.int.PK11_DIS_COULD_NOT_INIT_TOKEN    |
   |                                                 | -  nss.nss.int.PK11_DIS_NONE                    |
   |                                                 | -  nss.nss.int.PK11_DIS_TOKEN_NOT_PRESENT       |
   |                                                 | -  nss.nss.int.PK11_DIS_TOKEN_VERIFY_FAILED     |
   |                                                 | -  nss.nss.int.PK11_DIS_USER_SELECTED           |
   |                                                 | -  nss.nss.int.PKCS12_DES_56                    |
   |                                                 | -  nss.nss.int.PKCS12_DES_EDE3_168              |
   |                                                 | -  nss.nss.int.PKCS12_RC2_CBC_128               |
   |                                                 | -  nss.nss.int.PKCS12_RC2_CBC_40                |
   |                                                 | -  nss.nss.int.PKCS12_RC4_128                   |
   |                                                 | -  nss.nss.int.PKCS12_RC4_40                    |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following files were added      |
   |                                                 |    :name: the_following_files_were_added        |
   |                                                 |                                                 |
   |                                                 | -  test/run_tests                               |
   |                                                 | -  test/test_cipher.py (replaces                |
   |                                                 |    cipher_test.py)                              |
   |                                                 | -  test/test_client_server.py                   |
   |                                                 | -  test/test_digest.py (replaces                |
   |                                                 |    digest_test.py)                              |
   |                                                 | -  test/test_pkcs12.py                          |
   |                                                 |                                                 |
   |                                                 | .. rubric:: Deprecated Functionality            |
   |                                                 |    :name: deprecated_functionality_2            |
   |                                                 |                                                 |
   |                                                 | -  SignatureAlgorithm                           |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.11.0:

`Release 0.11.0 <#release_0.11.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2011-02-21                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         | PYNSS_RELEASE_0_11_0                            |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 | https://ftp.mozilla.org/pub/mozilla.org/securit |
   |                                                 | y/python-nss/releases/PYNSS_RELEASE_0_11_0/src/ |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | .. rubric:: External Changes                    |
   |                                                 |    :name: external_changes                      |
   |                                                 |                                                 |
   |                                                 | -  Add AddrInfo class to support IPv6 address   |
   |                                                 |    resolution. Supports iteration over it's set |
   |                                                 |    of NetworkAddress objects and provides       |
   |                                                 |    hostname, canonical_name object properties.  |
   |                                                 | -  Add PR_AI_\* constants.                      |
   |                                                 | -  NetworkAddress constructor and               |
   |                                                 |    NetworkAddress.set_from_string() added       |
   |                                                 |    optional family parameter. This is necessary |
   |                                                 |    for utilizing PR_GetAddrInfoByName().        |
   |                                                 | -  NetworkAddress initialized via a string      |
   |                                                 |    parameter are now initialized via            |
   |                                                 |    PR_GetAddrInfoByName using family.           |
   |                                                 | -  Add NetworkAddress.address property to       |
   |                                                 |    return the address sans the port as a        |
   |                                                 |    string. NetworkAddress.str() includes the    |
   |                                                 |    port. For IPv6 the a hex string must be      |
   |                                                 |    enclosed in brackets if a port is appended   |
   |                                                 |    to it, the bracketed hex address with        |
   |                                                 |    appended with a port is unappropriate in     |
   |                                                 |    some circumstances, hence the new address    |
   |                                                 |    property to permit either the address string |
   |                                                 |    with a port or without a port.               |
   |                                                 | -  Fix the implementation of the                |
   |                                                 |    NetworkAddress.family property, it was       |
   |                                                 |    returning bogus data due to wrong native     |
   |                                                 |    data size.                                   |
   |                                                 | -  HostEntry objects now support iteration and  |
   |                                                 |    indexing of their NetworkAddress members.    |
   |                                                 | -  Add io.addr_family_name() function to return |
   |                                                 |    string representation of PR_AF_\* constants. |
   |                                                 | -  Modify example and test code to utilize      |
   |                                                 |    AddrInfo instead of deprecated               |
   |                                                 |    NetworkAddress functionality. Add address    |
   |                                                 |    family command argument to ssl_example.      |
   |                                                 | -  Fix pty import statement in                  |
   |                                                 |    test/setup_certs.py                          |
   |                                                 |                                                 |
   |                                                 | .. rubric:: Deprecated Functionality            |
   |                                                 |    :name: deprecated_functionality_3            |
   |                                                 |                                                 |
   |                                                 | -  NetworkAddress initialized via a string      |
   |                                                 |    parameter is now deprecated. AddrInfo should |
   |                                                 |    be used instead.                             |
   |                                                 | -  NetworkAddress.set_from_string is now        |
   |                                                 |    deprecated. AddrInfo should be used instead. |
   |                                                 | -  NetworkAddress.hostentry is deprecated. It   |
   |                                                 |    was a bad idea, NetworkAddress objects can   |
   |                                                 |    support both IPv4 and IPv6, but a HostEntry  |
   |                                                 |    object can only support IPv4. Plus the       |
   |                                                 |    implementation depdended on being able to    |
   |                                                 |    perform a reverse DNS lookup which is not    |
   |                                                 |    always possible.                             |
   |                                                 | -  HostEntry.get_network_addresses() and        |
   |                                                 |    HostEntry.get_network_address() are now      |
   |                                                 |    deprecated. In addition their port parameter |
   |                                                 |    is now no longer respected. HostEntry        |
   |                                                 |    objects now support iteration and indexing   |
   |                                                 |    of their NetworkAddress and that should be   |
   |                                                 |    used to access their NetworkAddress objects  |
   |                                                 |    instead.                                     |
   |                                                 |                                                 |
   |                                                 | .. rubric:: Internal Changes                    |
   |                                                 |    :name: internal_changes                      |
   |                                                 |                                                 |
   |                                                 | -  Utilize PR_NetAddrFamily() access macro      |
   |                                                 |    instead of explict access.                   |
   |                                                 | -  Add PRNetAddr_port() utility to hide host    |
   |                                                 |    vs. network byte order requirements when     |
   |                                                 |    accessing the port inside a PRNetAddr and    |
   |                                                 |    simplify accessing the IPv4 vs. IPv6 port    |
   |                                                 |    variants.                                    |
   |                                                 | -  Replace the use of PR_InitializeNetAddr()    |
   |                                                 |    with PR_SetNetAddr(), the later properly     |
   |                                                 |    handles IPv6, the former did not.            |
   |                                                 | -  Rename NetworkAddress.addr to                |
   |                                                 |    NetworkAddress.pr_netaddr for naming         |
   |                                                 |    consistency.                                 |
   |                                                 | -  Update HostEntry documentation to indicate   |
   |                                                 |    it's deprecated status.                      |
   |                                                 | -  Remove redundant implementation of           |
   |                                                 |    NetworkAddress_new_from_PRNetAddr from       |
   |                                                 |    py_ssl.c and properly import the             |
   |                                                 |    implementation from py_nspr_io.c.            |
   |                                                 | -  The following other non-IPv6 fixes were also |
   |                                                 |    made because they were discovered while      |
   |                                                 |    doing the IPv6 work:                         |
   |                                                 | -  Move definition of TYPE_READY to             |
   |                                                 |    py_nspr_common.h so it can be shared. Update |
   |                                                 |    all modules to utilize it.                   |
   |                                                 | -  Replace incorrect use of free() with         |
   |                                                 |    PyMem_Free for string data returned by       |
   |                                                 |    Python's utf-8 encoder.                      |
   |                                                 | -  Add header dependency information to         |
   |                                                 |    setup.py so modules will be rebuilt when     |
   |                                                 |    header files change.                         |
   |                                                 | -  Add utility tuple_str() to convert a tuple   |
   |                                                 |    to a string representation by calling str()  |
   |                                                 |    on each object in the tuple. Tuple.str() in  |
   |                                                 |    CPython only calls repr() on each member.    |
   |                                                 | -  HostEntry objects now store their aliases    |
   |                                                 |    and NetworkAddress's in internal tuples.     |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.10.0:

`Release 0.10.0 <#release_0.10.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2010-07-25                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         | PYNSS_RELEASE_0_10_0                            |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 | https://ftp.mozilla.org/pub/mozilla.org/securit |
   |                                                 | y/python-nss/releases/PYNSS_RELEASE_0_10_0/src/ |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | .. rubric:: The following classes were added:   |
   |                                                 |    :name: the_following_classes_were_added_3    |
   |                                                 |                                                 |
   |                                                 | -  InitParameters                               |
   |                                                 | -  InitContext                                  |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following module functions were |
   |                                                 |    added:                                       |
   |                                                 |    :na                                          |
   |                                                 | me: the_following_module_functions_were_added_3 |
   |                                                 |                                                 |
   |                                                 | -  nss.nss.nss_initialize()                     |
   |                                                 | -  nss.nss.nss_init_context()                   |
   |                                                 | -  nss.nss.nss_shutdown_context()               |
   |                                                 | -  nss.nss.nss_init_flags()                     |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following constants were added: |
   |                                                 |    :name: the_following_constants_were_added_2  |
   |                                                 |                                                 |
   |                                                 | -  NSS_INIT_READONLY                            |
   |                                                 | -  NSS_INIT_NOCERTDB                            |
   |                                                 | -  NSS_INIT_NOMODDB                             |
   |                                                 | -  NSS_INIT_FORCEOPEN                           |
   |                                                 | -  NSS_INIT_NOROOTINIT                          |
   |                                                 | -  NSS_INIT_OPTIMIZESPACE                       |
   |                                                 | -  NSS_INIT_PK11THREADSAFE                      |
   |                                                 | -  NSS_INIT_PK11RELOAD                          |
   |                                                 | -  NSS_INIT_NOPK11FINALIZE                      |
   |                                                 | -  NSS_INIT_RESERVED                            |
   |                                                 | -  NSS_INIT_COOPERATE                           |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following file was added:       |
   |                                                 |    :name: the_following_file_was_added          |
   |                                                 |                                                 |
   |                                                 | -  test/setup_certs.py                          |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.9.0:

`Release 0.9.0 <#release_0.9.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2010-05-28                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         | PYNSS_RELEASE_0_9_0                             |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | .. rubric:: General Modifications:              |
   |                                                 |    :name: general_modifications                 |
   |                                                 |                                                 |
   |                                                 | -  Correct definciencies in                     |
   |                                                 |    auth_certificate_callback found in several   |
   |                                                 |    of the example files and documentation. If   |
   |                                                 |    you've copied that code you should merge     |
   |                                                 |    those changes in.                            |
   |                                                 | -  Unicode objects now accepted as well as str  |
   |                                                 |    objects for interfaces expecting a string.   |
   |                                                 | -  Sockets were enhanced thusly:                |
   |                                                 |                                                 |
   |                                                 |    -  Threads will now yield during blocking    |
   |                                                 |       IO.                                       |
   |                                                 |    -  Socket.makefile() reimplemented           |
   |                                                 |                                                 |
   |                                                 |       -  file object methods that had been      |
   |                                                 |          missing (readlines(), sendall(), and   |
   |                                                 |          iteration) were implemented            |
   |                                                 |       -  makefile now just returns the same     |
   |                                                 |       -  Socket object but increments an "open" |
   |                                                 |          ref count. Thus a Socket object        |
   |                                                 |          behaves like a file object and must be |
   |                                                 |          closed once for each makefile() call   |
   |                                                 |          before it's actually closed.           |
   |                                                 |                                                 |
   |                                                 |    -  Sockets now support the iter protocol     |
   |                                                 |    -  Added methods:                            |
   |                                                 |                                                 |
   |                                                 |       -  Socket.readlines()                     |
   |                                                 |       -  Socket.sendall()                       |
   |                                                 |                                                 |
   |                                                 | -  Apply patches from Miloslav Trmač            |
   |                                                 |    <mitr@redhat.com> for ref counting and       |
   |                                                 |    threading support. Thanks Miloslav!          |
   |                                                 | -  Review all ref counting, numerous ref        |
   |                                                 |    counting fixes                               |
   |                                                 | -  Implement cyclic garbage collection support  |
   |                                                 |    by adding object traversal and clear methods |
   |                                                 | -  Identify static variables, move to thread    |
   |                                                 |    local storage                                |
   |                                                 | -  Remove python-nss specific httplib.py, no    |
   |                                                 |    longer needed python-nss now compatible with |
   |                                                 |    standard library                             |
   |                                                 | -  Rewrite httplib_example.py to use standard   |
   |                                                 |    library and illustrate ssl, non-ssl,         |
   |                                                 |    connection class, http class usage           |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following classes were added:   |
   |                                                 |    :name: the_following_classes_were_added_4    |
   |                                                 |                                                 |
   |                                                 | -  AuthKeyID                                    |
   |                                                 | -  BasicConstraints                             |
   |                                                 | -  CRLDistributionPoint                         |
   |                                                 | -  CRLDistributionPts                           |
   |                                                 | -  CertificateExtension                         |
   |                                                 | -  GeneralName                                  |
   |                                                 | -  SignedCRL                                    |
   |                                                 | -  DN                                           |
   |                                                 | -  RDN                                          |
   |                                                 | -  AVA                                          |
   |                                                 | -  CertificateRequest                           |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following module functions were |
   |                                                 |    added:                                       |
   |                                                 |    :na                                          |
   |                                                 | me: the_following_module_functions_were_added_4 |
   |                                                 |                                                 |
   |                                                 | -  nss.nss.nss_is_initialized()                 |
   |                                                 | -  nss.nss.cert_crl_reason_from_name()          |
   |                                                 | -  nss.nss.cert_crl_reason_name()               |
   |                                                 | -  nss.nss.cert_general_name_type_from_name()   |
   |                                                 | -  nss.nss.cert_general_name_type_name()        |
   |                                                 | -  nss.nss.cert_usage_flags()                   |
   |                                                 | -  nss.nss.decode_der_crl()                     |
   |                                                 | -  nss.nss.der_universal_secitem_fmt_lines()    |
   |                                                 | -  nss.nss.import_crl()                         |
   |                                                 | -  nss.nss.make_line_pairs()                    |
   |                                                 | -  nss.nss.oid_dotted_decimal()                 |
   |                                                 | -  nss.nss.oid_str()                            |
   |                                                 | -  nss.nss.oid_tag()                            |
   |                                                 | -  nss.nss.oid_tag_name()                       |
   |                                                 | -  nss.nss.read_der_from_file()                 |
   |                                                 | -  nss.nss.x509_alt_name()                      |
   |                                                 | -  nss.nss.x509_ext_key_usage()                 |
   |                                                 | -  nss.nss.x509_key_usage()                     |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following class methods and     |
   |                                                 |    properties were added:                       |
   |                                                 |    :name: the_fo                                |
   |                                                 | llowing_class_methods_and_properties_were_added |
   |                                                 |                                                 |
   |                                                 | Note: it's a method if the name is suffixed     |
   |                                                 | with (), a propety otherwise                    |
   |                                                 |                                                 |
   |                                                 | -  Socket.next()                                |
   |                                                 | -  Socket.readlines()                           |
   |                                                 | -  Socket.sendall()                             |
   |                                                 | -  SSLSocket.next()                             |
   |                                                 | -  SSLSocket.readlines()                        |
   |                                                 | -  SSLSocket.sendall()                          |
   |                                                 | -  AuthKeyID.key_id                             |
   |                                                 | -  AuthKeyID.serial_number                      |
   |                                                 | -  AuthKeyID.get_general_names()                |
   |                                                 | -  CRLDistributionPoint.issuer                  |
   |                                                 | -  CRLDistributionPoint.get_general_names()     |
   |                                                 | -  CRLDistributionPoint.get_reasons()           |
   |                                                 | -  CertDB.find_crl_by_cert()                    |
   |                                                 | -  CertDB.find_crl_by_name()                    |
   |                                                 | -  Certificate.extensions                       |
   |                                                 | -  CertificateExtension.critical                |
   |                                                 | -  CertificateExtension.name                    |
   |                                                 | -  CertificateExtension.oid                     |
   |                                                 | -  CertificateExtension.oid_tag                 |
   |                                                 | -  CertificateExtension.value                   |
   |                                                 | -  GeneralName.type_enum                        |
   |                                                 | -  GeneralName.type_name                        |
   |                                                 | -  GeneralName.type_string                      |
   |                                                 | -  SecItem.der_to_hex()                         |
   |                                                 | -  SecItem.get_oid_sequence()                   |
   |                                                 | -  SecItem.to_hex()                             |
   |                                                 | -  SignedCRL.delete_permanently()               |
   |                                                 | -  AVA.oid                                      |
   |                                                 | -  AVA.oid_tag                                  |
   |                                                 | -  AVA.value                                    |
   |                                                 | -  AVA.value_str                                |
   |                                                 | -  DN.cert_uid                                  |
   |                                                 | -  DN.common_name                               |
   |                                                 | -  DN.country_name                              |
   |                                                 | -  DN.dc_name                                   |
   |                                                 | -  DN.email_address                             |
   |                                                 | -  DN.locality_name                             |
   |                                                 | -  DN.org_name                                  |
   |                                                 | -  DN.org_unit_name                             |
   |                                                 | -  DN.state_name                                |
   |                                                 | -  DN.add_rdn()                                 |
   |                                                 | -  DN.has_key()                                 |
   |                                                 | -  RDN.has_key()                                |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following module functions were |
   |                                                 |    removed:                                     |
   |                                                 |    :na                                          |
   |                                                 | me: the_following_module_functions_were_removed |
   |                                                 |                                                 |
   |                                                 | Note: use nss.nss.oid_tag() instead             |
   |                                                 |                                                 |
   |                                                 | -  nss.nss.sec_oid_tag_from_name()              |
   |                                                 | -  nss.nss.sec_oid_tag_name()                   |
   |                                                 | -  nss.nss.sec_oid_tag_str()                    |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following files were added:     |
   |                                                 |    :name: the_following_files_were_added_2      |
   |                                                 |                                                 |
   |                                                 | -  doc/examples/cert_dump.py                    |
   |                                                 | -  test/test_cert_components.py                 |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.8.0:

`Release 0.8.0 <#release_0.8.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2009-09-21                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         | PYNSS_RELEASE_0_8_0                             |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | .. rubric:: General Modifications:              |
   |                                                 |    :name: general_modifications_2               |
   |                                                 |                                                 |
   |                                                 | -  SecItem's now support indexing and slicing   |
   |                                                 |    on their data                                |
   |                                                 | -  Clean up parsing and parameter validation of |
   |                                                 |    variable arg functions                       |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following were added:           |
   |                                                 |    :name: the_following_were_added              |
   |                                                 |                                                 |
   |                                                 | -  SecItem.type SecItem.len                     |
   |                                                 | -  SecItem.data                                 |
   |                                                 | -  PK11SymKey.key_data                          |
   |                                                 | -  PK11SymKey.key_length                        |
   |                                                 | -  PK11SymKey.slot                              |
   |                                                 | -  create_context_by_sym_key                    |
   |                                                 | -  param_from_iv                                |
   |                                                 | -  generate_new_param                           |
   |                                                 | -  get_iv_length                                |
   |                                                 | -  get_block_size                               |
   |                                                 | -  get_pad_mechanism                            |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.7.0:

`Release 0.7.0 <#release_0.7.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2009-09-18                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | .. rubric:: General Modifications:              |
   |                                                 |    :name: general_modifications_3               |
   |                                                 |                                                 |
   |                                                 | -  add support for symmetric                    |
   |                                                 |    encryption/decryption                        |
   |                                                 | -  more support for digests (hashes)            |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following classes added:        |
   |                                                 |    :name: the_following_classes_added           |
   |                                                 |                                                 |
   |                                                 | -  PK11SymKey                                   |
   |                                                 | -  PK11Context                                  |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following methods and functions |
   |                                                 |    added:                                       |
   |                                                 |    :                                            |
   |                                                 | name: the_following_methods_and_functions_added |
   |                                                 |                                                 |
   |                                                 | -  get_best_wrap_mechanism                      |
   |                                                 | -  get_best_key_length                          |
   |                                                 | -  key_gen                                      |
   |                                                 | -  derive                                       |
   |                                                 | -  get_key_length                               |
   |                                                 | -  digest_key                                   |
   |                                                 | -  clone_context                                |
   |                                                 | -  digest_begin                                 |
   |                                                 | -  digest_op                                    |
   |                                                 | -  cipher_op                                    |
   |                                                 | -  finalize                                     |
   |                                                 | -  digest_final                                 |
   |                                                 | -  read_hex                                     |
   |                                                 | -  hash_buf                                     |
   |                                                 | -  sec_oid_tag_str                              |
   |                                                 | -  sec_oid_tag_name                             |
   |                                                 | -  sec_oid_tag_from_name                        |
   |                                                 | -  key_mechanism_type_name                      |
   |                                                 | -  key_mechanism_type_from_name                 |
   |                                                 | -  pk11_attribute_type_name                     |
   |                                                 | -  pk11_attribute_type_from_name                |
   |                                                 | -  get_best_slot                                |
   |                                                 | -  get_internal_key_slot                        |
   |                                                 | -  create_context_by_sym_key                    |
   |                                                 | -  import_sym_key                               |
   |                                                 | -  create_digest_context                        |
   |                                                 | -  param_from_iv                                |
   |                                                 | -  param_from_algid                             |
   |                                                 | -  generate_new_param                           |
   |                                                 | -  algtag_to_mechanism                          |
   |                                                 | -  mechanism_to_algtag                          |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following files added:          |
   |                                                 |    :name: the_following_files_added             |
   |                                                 |                                                 |
   |                                                 | -  test/cipher_test.py                          |
   |                                                 | -  test/digest_test.py                          |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.6.0:

`Release 0.6.0 <#release_0.6.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2009-07-08                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | .. rubric:: General Modifications:              |
   |                                                 |    :name: general_modifications_4               |
   |                                                 |                                                 |
   |                                                 | -  fix Red Hat bug #510343                      |
   |                                                 |    client_auth_data_callback seg faults if      |
   |                                                 |    False is returned from callback              |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.5.0:

`Release 0.5.0 <#release_0.5.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2009-07-01                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | .. rubric:: General Modifications:              |
   |                                                 |    :name: general_modifications_5               |
   |                                                 |                                                 |
   |                                                 | -  restore ssl.nss_init and ssl.nss_shutdown    |
   |                                                 |    but make them deprecated                     |
   |                                                 | -  add \__version_\_ string to nss module       |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.4.0:

`Release 0.4.0 <#release_0.4.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2009-06-30                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | .. rubric:: General Modifications:              |
   |                                                 |    :name: general_modifications_6               |
   |                                                 |                                                 |
   |                                                 | -  add binding for NSS_NoDB_Init(), Red Hat bug |
   |                                                 |    #509002                                      |
   |                                                 | -  move nss_init and nss_shutdown from ssl      |
   |                                                 |    module to nss module                         |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.3.0:

`Release 0.3.0 <#release_0.3.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2009-06-04                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | .. rubric:: General Modifications:              |
   |                                                 |    :name: general_modifications_7               |
   |                                                 |                                                 |
   |                                                 | -  import to Mozilla CVS, tweak directory       |
   |                                                 |    layout                                       |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.2.0:

`Release 0.2.0 <#release_0.2.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   +-------------------------------------------------+-------------------------------------------------+
   | Release Date                                    | 2009-05-21                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | SCM Tag                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | Source Download                                 |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | Change Log                                      | .. rubric:: General Modifications:              |
   |                                                 |    :name: general_modifications_8               |
   |                                                 |                                                 |
   |                                                 | -  apply patch from Red Hat bug #472805,        |
   |                                                 |    (Miloslav Trmač)                             |
   |                                                 | -  Don't allow closing a socket twice, that     |
   |                                                 |    causes crashes.                              |
   |                                                 | -  Fix return value creation in                 |
   |                                                 |    SSLSocket.get_security_status                |
   |                                                 | -  Convert licensing to MPL tri-license         |
   |                                                 |                                                 |
   |                                                 | .. rubric:: The following were added:           |
   |                                                 |    :name: the_following_were_added_2            |
   |                                                 |                                                 |
   |                                                 | -  nss.io.Socket.new_socket_pair()              |
   |                                                 | -  nss.io.Socket.poll()                         |
   |                                                 | -  nss.io.Socket.import_tcp_socket()            |
   |                                                 | -                                               |
   |                                                 |   nss.nss.Certificate.get_subject_common_name() |
   |                                                 | -  nss.nss.generate_random()                    |
   |                                                 | -  nss.ssl.SSLSocket.import_tcp_socket()        |
   +-------------------------------------------------+-------------------------------------------------+

.. _release_0.1.0:

`Release 0.1.0 <#release_0.1.0>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   =============== ===============
   Release Date    2008-07-09
   SCM Tag          
   Source Download  
   Change Log      Initial release
   =============== ===============