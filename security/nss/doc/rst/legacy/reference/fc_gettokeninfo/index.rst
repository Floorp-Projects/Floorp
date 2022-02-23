.. _mozilla_projects_nss_reference_fc_gettokeninfo:

FC_GetTokenInfo
===============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_GetTokenInfo - obtain information about a particular token in the system.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_GetTokenInfo(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo);

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetTokenInfo`` has two parameters:

   ``slotID``
      the ID of the token's slot
   ``pInfo``
      points to a `CK_TOKEN_INFO </en-US/CK_TOKEN_INFO>`__ structure

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetTokenInfo`` returns information about the token in the specified slot. On return, the
   ``CK_TOKEN_INFO`` structure that ``pInfo`` points to has the following information:

   -  ``label``: the label of the token, assigned during token initialization, padded with spaces to
      32 bytes and not null-terminated.
   -  ``manufacturerID``: ID of the device manufacturer, "Mozilla Foundation", padded with spaces to
      32 characters and not null-terminated.
   -  ``model``: model of the device, "NSS 3", padded with spaces to 16 characters and not
      null-terminated.
   -  ``serialNumber``: the device's serial number as a string, "0000000000000000", 16 characters
      and not null-terminated.
   -  ``flags``: bit flags indicating capabilities and status of the device.

      -  ``CKF_RNG (0x00000001)``: this device has a random number generator
      -  ``CKF_WRITE_PROTECTED (0x00000002)``: this device is read-only
      -  ``CKF_LOGIN_REQUIRED (0x00000004)``: this device requires the user to log in to use some of
         its services
      -  ``CKF_USER_PIN_INITIALIZED (0x00000008)``: the user's password has been initialized
      -  ``CKF_DUAL_CRYPTO_OPERATIONS (0x00000200)``: a single session with the token can perform
         dual cryptographic operations
      -  ``CKF_TOKEN_INITIALIZED (0x00000400)``: the token has been initialized. If login is
         required (which is true for the FIPS mode of operation), this flag means the user's
         password has been initialized.

   -  ``ulSessionCount``: number of sessions that this application currently has open with the token
   -  ``ulRwSessionCount``: number of read/write sessions that this application currently has open
      with the token
   -  ``hardwareVersion``: hardware version number, for example, 8.3 (``major=0x08, minor=0x03``),
      which are the version numbers of the certificate and key databases, respectively.
   -  ``firmwareVersion``: firmware version number, 0.0 (``major=0x00, minor=0x00``).

   A user may call ``FC_GetTokenInfo`` without logging into the token (to assume the NSS User role).

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``CKR_OK``
      Token information was successfully copied.
   ``CKR_CRYPTOKI_NOT_INITIALIZED``
      The PKCS #11 module library is not initialized.
   ``CKR_SLOT_ID_INVALID``
      The specified slot number is out of the defined range of values.

   .. note::

      FC_GetTokenInfo should return CKR_ARGUMENTS_BAD if pInfo is NULL.

`Examples <#examples>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Note the use of the ``%.32s`` format string to print the ``label`` and ``manufacturerID`` members
   of the ``CK_TOKEN_INFO`` structure.

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  :ref:`mozilla_projects_nss_reference_fc_getslotinfo`,
      `NSC_GetTokenInfo </en-US/NSC_GetTokenInfo>`__