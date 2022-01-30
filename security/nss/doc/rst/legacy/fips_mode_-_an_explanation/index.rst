.. _mozilla_projects_nss_fips_mode_-_an_explanation:

FIPS Mode - an explanation
==========================

.. container::

   NSS has a "FIPS Mode" that can be enabled when NSS is compiled in a specific way. (Note: Mozilla
   does not distribute a "FIPS Mode"-ready NSS with Firefox.) This page attempts to provide an
   informal explanation of what it is, who would use it, and why. 

.. _what's_a_fips:

`What's a FIPS? <#what's_a_fips>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The United States government defines many (several hundred) "Federal Information Processing
   Standard" (FIPS) documents.  (FIPS sounds plural, but is singular; one FIPS document is a FIPS,
   not a FIP.)  FIPS documents define rules, regulations, and standards for many aspects of handling
   of information by computers and by people.  They apply to all US government employees and
   personnel, including soldiers in the armed forces.  Generally speaking, any use of a computer by
   US government personnel must conform to all the relevant FIPS regulations.  If you're a
   US government worker, and you want to use a Mozilla software product such as Firefox, or any
   product that uses NSS, you will want to use it in a way that is fully conformant with all the
   relevant FIPS regulations.  Some other governments have also adopted many of the FIPS
   regulations, so their applicability is somewhat wider than just the US government's personnel.

.. _what_is_fips_mode:

`What is "FIPS Mode"? <#what_is_fips_mode>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   One of the FIPS regulations, FIPS 140, governs the use of encryption and cryptographic services. 
   It requires that ALL cryptography done by US government personnel MUST be done in "devices" that
   have been independently tested, and certified by NIST, to meet the extensive requirements of that
   document.  These devices may be hardware or software, but either way, they must function and
   behave as prescribed.  So, in order for Mozilla Firefox and Thunderbird to be usable by people
   who are subject to the FIPS regulations, Mozilla's cryptographic software must be able to operate
   in a mode that is fully compliant with FIPS 140.  To that end, Mozilla products can function in a
   "FIPS Mode", which is really "FIPS 140 Mode", when paired with a compliant copy of NSS.  (Note,
   the current version of FIPS 140 is revision 2, a.k.a. FIPS 140-2.  FIPS 140-3 is being devised by
   NIST now for adoption in the future.)  Users who are subject to the FIPS regulations must ensure
   that they have Mozilla's FIPS Mode enabled when they use Mozilla software, in order to be fully
   conformant.  Instructions for how to configure Firefox into FIPS mode may be found on
   `support.mozilla.com <https://support.mozilla.com/en-US/kb/Configuring+Firefox+for+FIPS+140-2>`__.

.. _is_nss_fips-140_compliant:

`Is NSS FIPS-140 compliant? <#is_nss_fips-140_compliant>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Mozilla's NSS cryptographic software has been tested by government-approved independent testing
   labs and certified by NIST as being FIPS 140 compliant *when operated in FIPS mode* on 4 previous
   occasions.  As of this writing, NSS is now being retested to be recertified for the fifth time. 
   NSS was the first open source cryptographic library to be FIPS certified.  

.. _what_is_fips_mode_all_about:

`What is FIPS Mode all about?  <#what_is_fips_mode_all_about>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   A FIPS-140 compliant application must do ALL of its cryptography in a FIPS-140 certified
   "device".  Whether it is hardware or software, that device will have all the cryptographic
   engines in it, and also will stores keys and perhaps certificates inside.  The device must have a
   way for users to authenticate to it (to "login" to it), to prove to it that they are authorized
   to use the cryptographic engines and keys it contains.  It may not do ANY cryptographic
   operations that involve the use of cryptographic keys, nor allow ANY of the keys or certificates
   it holds to be seen or used, except when a user has successfully authenticated to it.  If users
   authenticate to it with a password, it must ensure that their passwords are strong passwords.  It
   must implement the US government standard algorithms (also specified in other FIPS documents)
   such as AES, triple-DES, SHA-1 and SHA-256, that are needed to do whatever job the application
   wants it to perform.  It must generate or derive cryptographic keys and store them internally. 
   Except for "public keys", it must not allow any keys to leave it (to get outside of it) unless
   they are encrypted ("wrapped") in a special way.  This makes it difficult to move keys from one
   device to another, and consequently, all crypto engines and key storage must be in a single
   device rather than being split up into several devices.

.. _how_does_this_affect_firefox_users:

`How does this affect Firefox users? <#how_does_this_affect_firefox_users>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   These requirements have several implications for users.  In FIPS Mode, every user must have a
   good strong "master password", and must enter it each time they start or restart Firefox before
   they can visit any web sites that use cryptography (https).  Firefox can only use the latest
   version of SSL, known as "TLS", and not the older SSL 2 or SSL 3.0 protocols, and Firefox can
   only talk to those servers that use FIPS standard encryption algorithms such as AES or
   triple-DES.  Servers that can only use non-FIPS-approved encryption, such as RC4, cannot be used
   in FIPS mode.  

.. _how_is_fips_mode_different_from_normal_non-fips_mode:

`How is FIPS Mode different from normal non-FIPS Mode? <#how_is_fips_mode_different_from_normal_non-fips_mode>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   In normal non-FIPS Mode, the "master password" is optional and is allowed to be a weak short
   password.  The user is only required to enter his master password to use his own private keys (if
   he has any) or to access his stored web-site passwords.  The user is not required to enter the
   master password to visit ordinary https servers, nor to view certificates he has previously
   stored.  In non-FIPS mode, NSS is willing and able to use popular non-FIPS approved cryptographic
   algorithms, such as RC4 and MD5, to communicate with older https servers.  NSS divides its
   operations up into two "devices" rather than just one.  One device does all the operations that
   may be done without needing to authenticate, and the other device stores the user's certificates
   and private keys and performs operations that use those private keys.

.. _how_do_i_put_firefox_into_fips_mode:

`How do I put Firefox into FIPS Mode? <#how_do_i_put_firefox_into_fips_mode>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Instructions for how to configure Firefox into FIPS mode may be found on
   `support.mozilla.com <https://support.mozilla.com/en-US/kb/Configuring+Firefox+for+FIPS+140-2>`__.
   Some third-parties distribute Firefox ready for FIPS mode, `a partial list can be found at the
   NSS
   wiki <https://wiki.mozilla.org/FIPS_Validation#Products_Implementing_FIPS_140-2_Validated_NSS>`__.