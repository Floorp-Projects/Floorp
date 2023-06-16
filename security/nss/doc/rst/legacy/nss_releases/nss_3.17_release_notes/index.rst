.. _mozilla_projects_nss_nss_3_17_release_notes:

NSS 3.17 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.17, which is a minor release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_17_RTM. NSS 3.17 requires NSPR 4.10.7 or newer.

   NSS 3.17 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_17_RTM/src/

.. _new_in_nss_3.17:

`New in NSS 3.17 <#new_in_nss_3.17>`__
--------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  When using ECDHE, the TLS server code may be configured to generate a fresh ephemeral ECDH key
      for each handshake, by setting the SSL_REUSE_SERVER_ECDHE_KEY socket option to PR_FALSE. The
      SSL_REUSE_SERVER_ECDHE_KEY option defaults to PR_TRUE, which means the server's ephemeral ECDH
      key is reused for multiple handshakes. This option does not affect the TLS client code, which
      always generates a fresh ephemeral ECDH key for each handshake.

   New Macros

   -  *in ssl.h*

      -  **SSL_REUSE_SERVER_ECDHE_KEY**

.. _notable_changes_in_nss_3.17:

`Notable Changes in NSS 3.17 <#notable_changes_in_nss_3.17>`__
--------------------------------------------------------------

.. container::

   -  The manual pages for the certutil and pp tools have been updated to document the new
      parameters that had been added in NSS 3.16.2.
   -  On Windows, the new build variable USE_STATIC_RTL can be used to specify the static C runtime
      library should be used. By default the dynamic C runtime library is used.

.. _bugs_fixed_in_nss_3.17:

`Bugs fixed in NSS 3.17 <#bugs_fixed_in_nss_3.17>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.17:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.17