.. _mozilla_projects_nss_reference_nss_cryptographic_module_fips_mode_of_operation:

FIPS mode of operation
======================

.. _general-purpose_functions:

`General-purpose functions <#general-purpose_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  :ref:`mozilla_projects_nss_reference_fc_getfunctionlist`
   -  :ref:`mozilla_projects_nss_reference_fc_initialize`
   -  :ref:`mozilla_projects_nss_reference_fc_finalize`
   -  :ref:`mozilla_projects_nss_reference_fc_getinfo`

.. _slot_and_token_management_functions:

`Slot and token management functions <#slot_and_token_management_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  :ref:`mozilla_projects_nss_reference_fc_getslotlist`
   -  :ref:`mozilla_projects_nss_reference_fc_getslotinfo`
   -  :ref:`mozilla_projects_nss_reference_fc_gettokeninfo`
   -  :ref:`mozilla_projects_nss_reference_fc_waitforslotevent`
   -  :ref:`mozilla_projects_nss_reference_fc_getmechanismlist`
   -  :ref:`mozilla_projects_nss_reference_fc_getmechanisminfo`
   -  :ref:`mozilla_projects_nss_reference_fc_inittoken`
   -  :ref:`mozilla_projects_nss_reference_fc_initpin`
   -  :ref:`mozilla_projects_nss_reference_fc_setpin`

.. _session_management_functions:

`Session management functions <#session_management_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  :ref:`mozilla_projects_nss_reference_fc_opensession`
   -  :ref:`mozilla_projects_nss_reference_fc_closesession`
   -  :ref:`mozilla_projects_nss_reference_fc_closeallsessions`
   -  :ref:`mozilla_projects_nss_reference_fc_getsessioninfo`
   -  :ref:`mozilla_projects_nss_reference_fc_getoperationstate`
   -  :ref:`mozilla_projects_nss_reference_fc_setoperationstate`
   -  :ref:`mozilla_projects_nss_reference_fc_login`
   -  :ref:`mozilla_projects_nss_reference_fc_logout`

.. _object_management_functions:

`Object management functions <#object_management_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   These functions manage certificates and keys.

   -  :ref:`mozilla_projects_nss_reference_fc_createobject`
   -  :ref:`mozilla_projects_nss_reference_fc_copyobject`
   -  :ref:`mozilla_projects_nss_reference_fc_destroyobject`
   -  :ref:`mozilla_projects_nss_reference_fc_getobjectsize`
   -  :ref:`mozilla_projects_nss_reference_fc_getattributevalue`
   -  :ref:`mozilla_projects_nss_reference_fc_setattributevalue`
   -  :ref:`mozilla_projects_nss_reference_fc_findobjectsinit`
   -  :ref:`mozilla_projects_nss_reference_fc_findobjects`
   -  :ref:`mozilla_projects_nss_reference_fc_findobjectsfinal`

.. _encryption_functions:

`Encryption functions <#encryption_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   These functions support Triple DES and AES in ECB and CBC modes.

   -  :ref:`mozilla_projects_nss_reference_fc_encryptinit`
   -  :ref:`mozilla_projects_nss_reference_fc_encrypt`
   -  :ref:`mozilla_projects_nss_reference_fc_encryptupdate`
   -  :ref:`mozilla_projects_nss_reference_fc_encryptfinal`

.. _decryption_functions:

`Decryption functions <#decryption_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   These functions support Triple DES and AES in ECB and CBC modes.

   -  :ref:`mozilla_projects_nss_reference_fc_decryptinit`
   -  :ref:`mozilla_projects_nss_reference_fc_decrypt`
   -  :ref:`mozilla_projects_nss_reference_fc_decryptupdate`
   -  :ref:`mozilla_projects_nss_reference_fc_decryptfinal`

.. _message_digesting_functions:

`Message digesting functions <#message_digesting_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   These functions support SHA-1, SHA-256, SHA-384, and SHA-512.

   -  :ref:`mozilla_projects_nss_reference_fc_digestinit`
   -  :ref:`mozilla_projects_nss_reference_fc_digest`
   -  :ref:`mozilla_projects_nss_reference_fc_digestupdate`
   -  :ref:`mozilla_projects_nss_reference_fc_digestkey`
   -  :ref:`mozilla_projects_nss_reference_fc_digestfinal`

.. _signature_and_mac_generation_functions:

`Signature and MAC generation functions <#signature_and_mac_generation_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   These functions support DSA, RSA, ECDSA, and HMAC.

   -  :ref:`mozilla_projects_nss_reference_fc_signinit`
   -  :ref:`mozilla_projects_nss_reference_fc_sign`
   -  :ref:`mozilla_projects_nss_reference_fc_signupdate`
   -  :ref:`mozilla_projects_nss_reference_fc_signfinal`
   -  :ref:`mozilla_projects_nss_reference_fc_signrecoverinit`
   -  :ref:`mozilla_projects_nss_reference_fc_signrecover`

.. _signature_and_mac_verification_functions:

`Signature and MAC verification functions <#signature_and_mac_verification_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   These functions support DSA, RSA, ECDSA, and HMAC.

   -  :ref:`mozilla_projects_nss_reference_fc_verifyinit`
   -  :ref:`mozilla_projects_nss_reference_fc_verify`
   -  :ref:`mozilla_projects_nss_reference_fc_verifyupdate`
   -  :ref:`mozilla_projects_nss_reference_fc_verifyfinal`
   -  :ref:`mozilla_projects_nss_reference_fc_verifyrecoverinit`
   -  :ref:`mozilla_projects_nss_reference_fc_verifyrecover`

.. _dual-function_cryptographic_functions:

`Dual-function cryptographic functions <#dual-function_cryptographic_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  :ref:`mozilla_projects_nss_reference_fc_digestencryptupdate`
   -  :ref:`mozilla_projects_nss_reference_fc_decryptdigestupdate`
   -  :ref:`mozilla_projects_nss_reference_fc_signencryptupdate`
   -  :ref:`mozilla_projects_nss_reference_fc_decryptverifyupdate`

.. _key_management_functions:

`Key management functions <#key_management_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  :ref:`mozilla_projects_nss_reference_fc_generatekey`: DSA domain parameters (PQG)
   -  :ref:`mozilla_projects_nss_reference_fc_generatekeypair`: DSA, RSA, and ECDSA. Performs
      pair-wise consistency test.
   -  :ref:`mozilla_projects_nss_reference_fc_wrapkey`: RSA Key Wrapping
   -  :ref:`mozilla_projects_nss_reference_fc_unwrapkey`: RSA Key Wrapping
   -  :ref:`mozilla_projects_nss_reference_fc_derivekey`: Diffie-Hellman, EC Diffie-Hellman

.. _random_number_generation_functions:

`Random number generation functions <#random_number_generation_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  :ref:`mozilla_projects_nss_reference_fc_seedrandom`
   -  :ref:`mozilla_projects_nss_reference_fc_generaterandom`: Performs continuous random number
      generator test.

.. _parallel_function_management_functions:

`Parallel function management functions <#parallel_function_management_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  :ref:`mozilla_projects_nss_reference_fc_getfunctionstatus`
   -  :ref:`mozilla_projects_nss_reference_fc_cancelfunction`