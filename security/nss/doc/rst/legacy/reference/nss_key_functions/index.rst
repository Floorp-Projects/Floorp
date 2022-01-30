.. _mozilla_projects_nss_reference_nss_key_functions:

NSS Key Functions
=================

.. container::

   This chapter describes two functions used to manipulate private keys and key databases such as
   the key3.db database provided with NSS. This was converted from `"Chapter 6: Key
   Functions" <https://developer.mozilla.org/en-US/docs/NSS/SSL_functions/sslkey.html>`__.

   -  :ref:`mozilla_projects_nss_reference`
   -  `SECKEY_GetDefaultKeyDB <#seckey_getdefaultkeydb>`__
   -  `SECKEY_DestroyPrivateKey <#seckey_destroyprivatekey>`__

   .. rubric:: SECKEY_GetDefaultKeyDB
      :name: seckey_getdefaultkeydb

   Returns a handle to the default key database opened by NSS_Init.

   Syntax

   #. include <key.h>
   #. include <keyt.h>

   SECKEYKeyDBHandle \*SECKEY_GetDefaultKeyDB(void);

   Returns The function returns a handle of type SECKEYKeyDBHandle.

   Description NSS_Init opens the certificate, key, and security module databases that you specify
   for use with NSS. SECKEYKeyDBHandle returns a handle to the key database opened by NSS_Init.

   .. rubric:: SECKEY_DestroyPrivateKey
      :name: seckey_destroyprivatekey

   Destroys a private key structure.

   Syntax

   #. include <key.h>
   #. include <keyt.h>

   void SECKEY_DestroyPrivateKey(SECKEYPrivateKey \*key);

   Parameter This function has the following parameter:

   key

   A pointer to the private key structure to destroy.

   Description Certificate and key structures are shared objects. When an application makes a copy
   of a particular certificate or key structure that already exists in memory, SSL makes a shallow
   copy--that is, it increments the reference count for that object rather than making a whole new
   copy. When you call CERT_DestroyCertificate or SECKEY_DestroyPrivateKey, the function decrements
   the reference count and, if the reference count reaches zero as a result, both frees the memory
   and sets all the bits to zero. The use of the word "destroy" in function names or in the
   description of a function implies reference counting.

   Never alter the contents of a certificate or key structure. If you attempt to do so, the change
   affects all the shallow copies of that structure and can cause severe problems.