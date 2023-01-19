.. _mozilla_projects_nss_ssl_functions_sslkey:

sslkey
======

.. container::

   .. note::

      -  This page is part of the :ref:`mozilla_projects_nss_ssl_functions_old_ssl_reference` that
         we are migrating into the format described in the `MDN Style
         Guide <https://developer.mozilla.org/en-US/docs/MDN/Guidelines>`__. If you are inclined to
         help with this migration, your help would be very much appreciated.

      -  Upgraded documentation may be found in the :ref:`mozilla_projects_nss_reference`

   .. rubric:: Key Functions
      :name: Key_Functions

   --------------

.. _chapter_6_key_functions:

`Chapter 6
 <#chapter_6_key_functions>`__ Key Functions
--------------------------------------------

.. container::

   This chapter describes two functions used to manipulate private keys and key databases such as
   the ``key3.db`` database provided with Communicator.

   |  ```SECKEY_GetDefaultKeyDB`` <#1051479>`__
   | ```SECKEY_DestroyPrivateKey`` <#1051017>`__

   .. rubric:: SECKEY_GetDefaultKeyDB
      :name: seckey_getdefaultkeydb

   Returns a handle to the default key database opened by
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1067601`.

   .. rubric:: Syntax
      :name: syntax

   .. code::

      #include <key.h>
      #include <keyt.h>

   .. code::

      SECKEYKeyDBHandle *SECKEY_GetDefaultKeyDB(void);

   .. rubric:: Returns
      :name: returns

   The function returns a handle of type ``SECKEYKeyDBHandle``.

   .. rubric:: Description
      :name: description

   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1067601` opens the certificate, key, and security
   module databases that you specify for use with NSS. ``SECKEYKeyDBHandle`` returns a handle to the
   key database opened by ``NSS_Init``.

   .. rubric:: SECKEY_DestroyPrivateKey
      :name: seckey_destroyprivatekey

   Destroys a private key structure.

   .. rubric:: Syntax
      :name: syntax_2

   .. code::

      #include <key.h>
      #include <keyt.h>

   .. code::

      void SECKEY_DestroyPrivateKey(SECKEYPrivateKey *key);

   .. rubric:: Parameter
      :name: parameter

   This function has the following parameter:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code::                           | A pointer to the private key structure to       |
   |                                                 | destroy.                                        |
   |    key                                          |                                                 |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Description
      :name: description_2

   Certificate and key structures are shared objects. When an application makes a copy of a
   particular certificate or key structure that already exists in memory, SSL makes a *shallow*
   copy--that is, it increments the reference count for that object rather than making a whole new
   copy. When you call ```CERT_DestroyCertificate`` <sslcrt.html#1050532>`__ or
   ```SECKEY_DestroyPrivateKey`` <#1051017>`__, the function decrements the reference count and, if
   the reference count reaches zero as a result, both frees the memory and sets all the bits to
   zero. The use of the word "destroy" in function names or in the description of a function implies
   reference counting.

   Never alter the contents of a certificate or key structure. If you attempt to do so, the change
   affects all the shallow copies of that structure and can cause severe problems.