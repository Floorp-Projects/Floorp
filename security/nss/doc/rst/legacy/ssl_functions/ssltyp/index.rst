.. _mozilla_projects_nss_ssl_functions_ssltyp:

ssltyp
======

.. container::

   .. note::

      -  This page is part of the :ref:`mozilla_projects_nss_ssl_functions_old_ssl_reference` that
         we are migrating into the format described in the `MDN Style
         Guide <https://developer.mozilla.org/en-US/docs/Project:MDC_style_guide>`__. If you are
         inclined to help with this migration, your help would be very much appreciated.

      -  Upgraded documentation may be found in the :ref:`mozilla_projects_nss_reference`

   .. rubric:: Selected SSL Types and Structures
      :name: Selected_SSL_Types_and_Structures

   --------------

.. _chapter_3_selected_ssl_types_and_structures:

`Chapter 3
 <#chapter_3_selected_ssl_types_and_structures>`__ Selected SSL Types and Structures
------------------------------------------------------------------------------------

.. container::

   This chapter describes some of the most important types and structures used with the functions
   described in the rest of this document, and how to manage the memory used for them. Additional
   types are described with the functions that use them or in the header files.

   |  `Types and Structures <#1030559>`__
   | `Managing SECItem Memory <#1029645>`__

.. _types_and_structures:

`Types and Structures <#types_and_structures>`__
------------------------------------------------

.. container::

   These types and structures are described here:

   |  ```CERTCertDBHandle`` <#1028465>`__
   | ```CERTCertificate`` <#1027387>`__
   | ```PK11SlotInfo`` <#1028593>`__
   | ```SECItem`` <#1026076>`__
   | ```SECKEYPrivateKey`` <#1026727>`__
   | ```SECStatus`` <#1026722>`__

   Additional types used by a single function only are described with the function's entry in each
   chapter. Some of these functions also use types defined by NSPR and described in the `NSPR
   Reference <https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSPR/Reference>`__.

   <a id="> Many of the structures presented here (```CERTCertDBHandle`` <#1028465>`__,
   ```CERTCertificate`` <#1027387>`__, ```PK11SlotInfo`` <#1028593>`__, and
   ```SECKEYPrivateKey`` <#1026727>`__) are opaque--that is, they are types defined as structures
   (for example, ``CERTCertDBHandleStr``) that may change in future releases of Network Security
   Services. As long as you use the form shown here, your code will not need revision.

   .. rubric:: CERTCertDBHandle
      :name: certcertdbhandle

   An opaque handle structure for open certificate databases.

   .. rubric:: Syntax
      :name: syntax

   .. code:: notranslate

      #include <certt.h>

   .. code:: notranslate

      typedef struct CERTCertDBHandleStr CERTCertDBHandle;

   .. rubric:: CERTCertificate
      :name: certcertificate

   An opaque X.509 certificate object.

   .. rubric:: Syntax
      :name: syntax_2

   .. code:: notranslate

      #include <certt.h>

   .. code:: notranslate

      typedef struct CERTCertificateStr CERTCertificate;

   .. rubric:: Description
      :name: description

   Certificate structures are shared objects. When an application makes a copy of a particular
   certificate structure that already exists in memory, SSL makes a *shallow* copy--that is, it
   increments the reference count for that object rather than making a whole new copy. When you call
   ```CERT_DestroyCertificate`` <sslcrt.html#1050532>`__, the function decrements the reference
   count and, if the reference count reaches zero as a result, frees the memory. The use of the word
   "destroy" in function names or in the description of a function often implies reference counting.

   Never alter the contents of a certificate structure. If you attempt to do so, the change affects
   all the shallow copies of that structure and can cause severe problems.

   .. rubric:: PK11SlotInfo
      :name: pk11slotinfo

   An opaque structure representing a physical or logical PKCS #11 slot.

   .. rubric:: Syntax
      :name: syntax_3

   .. code:: notranslate

      #include <pk11expt.h>

   ``typedef struct PK11SlotInfo``\ Str ``PK11SlotInfo``;

   .. rubric:: SECItem
      :name: secitem

   A structure that points to other structures.

   .. rubric:: Syntax
      :name: syntax_4

   .. code:: notranslate

      #include <seccomon.h>
      #include <prtypes.h>
      #include <secport.h>

   .. code:: notranslate

      typedef enum {
          siBuffer,
          siClearDataBuffer,
          siCipherDataBuffer,
          siDERCertBuffer,
          siEncodedCertBuffer,
          siDERNameBuffer,
          siEncodedNameBuffer,
          siAsciiNameString,
          siAsciiString,
          siDEROID
      } SECItemType;

   .. code:: notranslate

      typedef struct SECItemStr SECItem;

   .. code:: notranslate

      struct SECItemStr {
          SECItemType type;
          unsigned char *data;
          unsigned int len;
      };

   .. rubric:: Description
      :name: description_2

   A ``SECItem`` structure can be used to associate your own data with an SSL socket.

   To free a structure pointed to by a ``SECItem``, and, if desired, the ``SECItem`` structure
   itself, use one the functions ```SECItem_FreeItem`` <#1030620>`__ or
   ```SECItem_ZfreeItem`` <#1030773>`__.

   .. rubric:: SECKEYPrivateKey
      :name: seckeyprivatekey

   An opaque, generic key structure.

   .. rubric:: Syntax
      :name: syntax_5

   .. code:: notranslate

      #include <keyt.h>

   .. code:: notranslate

      typedef struct SECKEYPrivateKeyStr SECKEYPrivateKey;

   .. rubric:: Description
      :name: description_3

   Key structures are not shared objects. When an application makes a copy of a particular key
   structure that already exists in memory, SSL makes a *deep* copy--that is, it makes a whole new
   copy of that object. When you call ```SECKEY_DestroyPrivateKey`` <sslkey.html#1051017>`__, the
   function both frees the memory and sets all the bits to zero.

   Never alter the contents of a key structure. Treat the structure as read only.

   .. rubric:: SECStatus
      :name: secstatus

   The return value for many SSL functions.

   .. rubric:: Syntax
      :name: syntax_6

   .. code:: notranslate

      #include <seccomon.h>

   .. code:: notranslate

      typedef enum {
          SECWouldBlock = -2,
          SECFailure = -1,
          SECSuccess = 0
      } SECStatus;

   .. rubric:: Enumerators
      :name: enumerators

   The enum includes the following enumerators:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | Reserved for internal use.                      |
   |                                                 |                                                 |
   |    SECWouldBlock                                |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | The operation failed. To find out why, call     |
   |                                                 | ``PR_GetError``.                                |
   |    SECFailure                                   |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | The operation succeeded. In this case the value |
   |                                                 | returned by ``PR_GetError`` is meaningless.     |
   |    SECSuccess                                   |                                                 |
   +-------------------------------------------------+-------------------------------------------------+

.. _managing_secitem_memory:

`Managing SECItem Memory <#managing_secitem_memory>`__
------------------------------------------------------

.. container::

   These functions are available for managing the memory associated with ``SECItem`` structures and
   the structures to which they point.

   |  ```SECItem_FreeItem`` <#1030620>`__
   | ```SECItem_ZfreeItem`` <#1030773>`__

   .. rubric:: SECItem_FreeItem
      :name: secitem_freeitem

   Frees the memory associated with a ``SECItem`` structure.

   .. rubric:: Syntax
      :name: syntax_7

   .. code:: notranslate

      #include <prtypes.h> 

   .. code:: notranslate

      SECStatus SECItem_FreeItem (
         SECItem *item,
         PRBool freeItem)

   .. rubric:: Parameter
      :name: parameter

   This function has the following parameter:

   +----------+--------------------------------------------------------------------------------------+
   | ``item`` | A pointer to a ``SECItem`` structure.                                                |
   +----------+--------------------------------------------------------------------------------------+
   | freeItem | When ``PR_FALSE``, free only the structure pointed to. Otherwise, free both the      |
   |          | structure pointed to and the ``SECItem`` structure itself.                           |
   +----------+--------------------------------------------------------------------------------------+

   .. rubric:: Returns
      :name: returns

   The function returns one of these value\ ``s``:

   -  If successful, ``SECSuccess``.
   -  If unsuccessful, ``SECFailure``. Use
      `PR_GetError <../../../../../nspr/reference/html/prerr.html#26127>`__ to retrieve the error
      code.

   .. rubric:: Description
      :name: description_4

   This function frees the memory associated with the structure to which the specified item points,
   when that structure is no longer used. When ``freeItem`` is not ``PR_FALSE``, also frees the item
   structure itself.

   .. rubric:: SECItem_ZfreeItem
      :name: secitem_zfreeitem

   Zeroes and frees the memory associated with a ``SECItem`` structure.

   .. rubric:: Syntax
      :name: syntax_8

   .. code:: notranslate

      #include <prtypes.h> 

   .. code:: notranslate

      SECStatus SECItem_ZfreeItem (
         SECItem *item,
         PRBool freeItem)

   .. rubric:: Parameter
      :name: parameter_2

   This function has the following parameter:

   +----------+--------------------------------------------------------------------------------------+
   | ``item`` | A pointer to a ``SECItem`` structure.                                                |
   +----------+--------------------------------------------------------------------------------------+
   | freeItem | When ``PR_FALSE``, free only the structure pointed to. Otherwise, free both the      |
   |          | structure pointed to and the ``SECItem`` structure itself.                           |
   +----------+--------------------------------------------------------------------------------------+

   .. rubric:: Returns
      :name: returns_2

   The function returns one of these value\ ``s``:

   -  If successful, ``SECSuccess``.
   -  If unsuccessful, ``SECFailure``. Use
      `PR_GetError <../../../../../nspr/reference/html/prerr.html#26127>`__ to retrieve the error
      code.

   .. rubric:: Description
      :name: description_5

   This function is similar to ```SECItem_FreeItem`` <#1030620>`__, except that it overwrites the
   structures to be freed with zeroes before it frees them. Zeros and frees the memory associated
   with the structure to which the specified item points, when that structure is no longer used.
   When ``freeItem`` is not ``PR_FALSE``, also zeroes and frees the item structure itself.