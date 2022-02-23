.. _mozilla_projects_nss_pkcs11_faq:

PKCS11 FAQ
==========

.. _pkcs11_faq:

`PKCS11 FAQ <#pkcs11_faq>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: QUESTIONS AND ANSWERS
      :name: questions_and_answers

   .. rubric:: GENERAL QUESTIONS
      :name: general_questions

   .. rubric:: After plugging in an external PKCS #11 module, how do you use the certificate
      available on the token? Does the certificate need to be imported into NSS's internal
      certificate database? If so, is there a way to get the certificate from an external token into
      NSS's internal certificate database?
      :name: after_plugging_in_an_external_pkcs_.2311_module.2c_how_do_you_use_the_certificate_available_on_the_token.3f_does_the_certificate_need_to_be_imported_into_nss.27s_internal_certificate_database.3f_if_so.2c_is_there_a_way_to_get_the_certificate_from_an_external_token_into_nss.27s_internal_certificate_database.3f

   NSS searches all the installed PKCS #11 modules when looking for certificates. Once you've
   installed the module, the module's certificates simply appear in the list of certificates
   displayed in the Certificate window.

   .. rubric:: What version of PKCS #11 does NSS support?
      :name: what_version_of_pkcs_.2311_does_nss_support.3f

   NSS requires at least PKCS #11 version 2.0, but can support some features of later versions of
   NSS, including NSS 2.20. NSS does not use all the features of later versions of PKCS #11.

   .. rubric:: What are the expectations in terms of session manipulation? Will NSS potentially open
      more than one session at a time? Read-only sessions, read/write sessions, serial, parallel?
      :name: what_are_the_expectations_in_terms_of_session_manipulation.3f_will_nss_potentially_open_more_than_one_session_at_a_time.3f_read-only_sessions.2c_read.2fwrite_sessions.2c_serial.2c_parallel.3f

   NSS typically holds one session read-only session per slot, in which some of the non-multipart
   functions are handled. Multipart functions, such as bulk encryption, hashing, and mac functions
   (for example, C_Digest and C_Sign) and those that require overlapped operation (C_Unwrap,
   C_Decrypt) are handled by creating new sessions. If no new sessions are available, the one
   read-only session is used, and the state is saved and restored after each multipart operation.

   NSS never uses Parallel mode.

   NSS opens new read/write sessions for key generation, some password management, and storage of
   new certificates.

   If your token is read/write and has only one session, NSS will open that one initial session
   read/write.

   .. rubric:: What permanent PKCS #11 objects are used by NSS or read from the token? Example: RSA
      private key, CA certificate, user's own certificate, user's name.
      :name: what_permanent_pkcs_.2311_objects_are_used_by_nss_or_read_from_the_token.3f_example:_rsa_private_key.2c_ca_certificate.2c_user.27s_own_certificate.2c_user.27s_name.

   Private keys (RSA and DSA) and the corresponding certificates are read from the token. Other
   certificates on the token are also loaded (to allow building certificate chains), but it's not
   necessary to include the full chain, as long as the full chain is available in the regular
   certificate database. For the sake of completeness, it's also a good idea to expose public key
   objects. NSS falls back to looking for the existance of public keys to determine if the token may
   have the corresponding private key while the token is not logged in.

   .. rubric:: How are permanent PKCS #11 objects found by NSS? That is, which PKCS #11 attributes
      are used in the object searches? Labels? Key IDs? Key types?
      :name: how_are_permanent_pkcs_.2311_objects_found_by_nss.3f_that_is.2c_which_pkcs_.2311_attributes_are_used_in_the_object_searches.3f_labels.3f_key_ids.3f_key_types.3f

   These are the general guidelines:

   -  User certificates are identified by their labels.
   -  Certificates and keys are often looked up by the following methods:

      -  By looking up all private keys.
      -  By looking up all certificates.
      -  Certificates may be looked up by label. By convention, all certificates making up a single
         personality should have the same label (that is, a pair of certificates, one for signing
         and one for key exchange, should have the same label).
      -  S/MIME-capable certificates are also looked up by issuer/serial number.
      -  Certificates may be looked up by their DER value.
      -  Certificates may also be looked up by subject. More than one certificate can match, but
         each certificate with the same subject should be part of the same personality.
      -  NSS may enumerate all the permanment certificates in a token (CKA_TOKEN set to true).
      -  Private keys must have the same CKA_ID value as their corresponding certificate, and this
         value must be unique on the token.
      -  Orphaned keys have a CKA_ID generated from some part of the public key. This value is set
         when the key is generated, so that NSS will be able to find the key when the certificate
         for that key is loaded. This case is interesting only for read/write tokens.

   .. rubric:: What labels does NSS use to identify certificates?
      :name: what_labels_does_nss_use_to_identify_certificates.3f

   NSS can use the CKA_LABEL attribute to identify user certificates (see previous question) and
   presents this label to the user. Therefore, each user certificate must have some label associated
   with it. The label for a token certificate is presented to the user as follows:*token label*
   **:**\ *certificate label* . This implies that each\ *token label* should be unique and
   meaningful to the user, and that each\ *certificate label* should be unique to the token.

   NSS gets the value of the CKA_LABEL attribute from the token. Labels should not have any trailing
   blank characters.

   .. rubric:: Will NSS use the random number generation features of PKCS #11?
      :name: will_nss_use_the_random_number_generation_features_of__pkcs_.2311.3f

   Only if you identify your token as the default random number generator. If you do, your token
   must be able to generate random numbers even when it is not logged in. NSS uses installed random
   number generators if PKCS11_MECH_RANDOM_FLAG is set in the installer script. For information on
   how to do this, see Using the JAR Installation Manager to Install a PKCS #11 Cryptographic
   Module.

   .. rubric:: Can Mozilla provide a list of all PKCS #11 functions that NSS will use?
      :name: can_mozilla_provide_a_list_of_all_pkcs_.2311_functions_that_nss_will_use.3f

   Your token should expect to implement all the PKCS #11 functions that make sense for your token.
   NSS continues to evolve, and periodically enhances it's functionality by using a more complete
   list of PKCS #11 functions. You should have implementations for all the functions specified in
   the version of the PKCS #11 spec your token implements. If you do not actually do the operation
   specified by that function, you can return CKR_FUNCTION_NOT_SUPPORTED.

   .. rubric:: Will NSS get the user's CA certificate via PKCS #11 and push it into the CA
      certificate database or is the CA certificate database expected to obtain the CA certificate
      by some other means?
      :name: will_nss_get_the_user.27s_ca_certificate_via_pkcs_.2311_and_push_it_into_the_ca_certificate_database_or_is_the_ca_certificate_database_expected_to_obtain_the_ca_certificate_by_some_other_means.3f

   PKCS #11 certificates that have private keys associated with them are loaded into the temporary
   database (in memory) and marked as user certificates. All other certificates in the module are
   loaded into the temporary database with no special trust bits associated with them. NSS is
   perfectly capable of using token certificates in place.

   .. rubric:: Which function does NSS use to get login state information?
      :name: which_function_does_nss_use_to_get_login_state_information.3f

   NSS calls C_GetSessionInfo to get the login/logout state. NSS never attempts to cache this
   information, because login state can change instantly without NSS knowing about it (for example,
   when the user removes the card). You must update all sessions correctly when the state changes.
   Not doing so is a common source of problems.

   .. rubric:: I have noticed that NSS sometimes use a session handle value of 0. Is this an invalid
      session handle?
      :name: i_have_noticed_that_nss_sometimes_use__a_session_handle_value_of_0._is_this_an_invalid_session_handle.3f

   A session handle of 0 is indeed invalid. In the past, NSS uses the invalid session handle to mark
   problems with acquiring or using a session. There have been cases where NSS would then use this
   handle to try to do some operation. PKCS #11 modules should fail with CKR_INVALID_SESSION. We are
   working to remove these cases as we find them.

   .. rubric:: What are "Generic Crypto Svcs" (the first item listed when you click the View/Edit
      button for the NSS Internal PKCS #11 Module under Security Devices under Options/Security in
      Firefox)?
      :name: what_are_.22generic_crypto_svcs.22_.28the_first_item_listed_when_you_click_the_view.2fedit_button_for_the_nss_internal_pkcs_.2311_module__under_security_devices_under_options.2fsecurity_in_firefox.29.3f

   Generic Crypto Svcs are the services that NSS uses to do its basic cryptography (RSA encryption
   with public keys, hashing, AES, DES, RC4, RC2, and so on).Other PKCS #11 modules can supply
   implementations of these functions, and NSS uses those versions under certain conditions.
   However, these are not the services NSS calls to get to other PKCS #11 modules, which show up
   separately under Cryptographic Modules.

   .. rubric:: Our plugin provides several slots with different capabilities. For example, one does
      all the hashing/symmetric operations, while another does only asymmetric RSA operations. Can
      this kind of division lead to problems?
      :name: our_plugin_provides_several_slots_with_different_capabilities._for_example.2c_one_does_all_the_hashing.2fsymmetric_operations.2c_while_another_does_only_asymmetric_rsa_operations._can_this_kind_of_division_lead_to_problems.3f

   The only issue is dealing with keys. For example, if the RSA slot unwraps a key, NSS needs to
   move that key to a slot that can do the symmetric operations. NSS itself uses two tokens
   internally--one that provides generic cryptographic services without authentication, and one that
   provides operations based on the keys stored in the user's database and do need authentication.
   NSS does this to avoid having to prompt for a password when performing an RSA verify operation,
   DES encryption, and so on. Therefore, NSS can move keys around when necessary and possible. When
   operating in FIPS mode, moving keys is significantly harder. In this case NSS uses a single token
   to handle both key and cert storage and crypto operations.

   In general, you not should use different slots unless you have a good reason. Much of NSS's token
   selection is based on where the key involved is currently stored. If the token that has your
   private keys doesn't also do symmetric operations, for example, it's likely that the internal
   token will end up doing the symmetric operations.

   .. rubric:: Is the PKCS #11 module supplied with NSS accessible through a shared library?
      :name: is_the_pkcs_.2311_module_supplied_with_nss_accessible_through_a_shared_library.3f

   Yes, the token is call softokn3 (softokn3.dll on windows, libsoftokn3.so on most unix platforms).
   The NSS softokn3 is not a complete PKCS #11 module, it was implemented only to support NSS,
   though other products have managed to get it to work in their environment. There are a number of
   bugs against softoken's non-compliance, but these bugs have lower priority than fixing NSS's
   non-complient uses of PKCS #11 or adding new features to NSS.

   .. rubric:: If multiple PKCS #11 modules are loaded, how does NSS determine which ones to use for
      the mechanisms required by SSL?
      :name: if_multiple_pkcs_.2311_modules_are_loaded.2c_how_does_nss_determine_which_ones_to_use_for_the_mechanisms_required_by_ssl.3f

   NSS uses the first slot it finds that can perform all the required operations. On servers, it's
   almost always the slot that contains the server's private key.

   .. rubric:: Does NSS support the use of PKCS #11 callbacks specified in the pNotify and
      pApplication parameters for C_OpenSession?
      :name: does_nss_support_the_use_of_pkcs_.2311_callbacks_specified_in_the_pnotify_and_papplication_parameters_for_c_opensession.3f

   NSS does not currently use any of the callbacks.

   NSS applications detect card insertion and deletion by means of polling to determine whether the
   card is still in the slot and whether the open session associated with that card is still valid,
   or by waiting on the C_WaitForSlotEvent call.

   .. rubric:: What must an X.509 certificate include to allow it to be recognized as an email
      certificate for use with S/MIME?
      :name: what_must_an_x.509_certificate_include_to_allow_it_to_be_recognized_as_an_email_certificate_for_use_with_s.2fmime.3f

   An email address must be included in the attribute of the subject DN or the mail attribute of the
   subject DN. If the subject DN does not include an email address, the certificate extension
   subjectAltName must include an email address. The subjectAltName extension is part of the X.509
   v3 and PKIX specifications.

   .. rubric:: If I have a multipurpose token that supports all required PKCS #11 functions and
      provides RSA_PKCS and DSA mechanisms but not AES, DES or RC4, will NSS use the token for the
      RSA_PKCS mechanisms and the NSS Internal PKCS #11 module for AES, DES or RC4 when making an
      SSL connection?
      :name: if_i_have_a_multipurpose_token_that_supports_all_required_pkcs_.2311_functions_and_provides_rsa_pkcs_and_dsa_mechanisms_but_but_not_aes.2c_des_or_rc4.2c_will_nss_use_the_token_for_the_rsa_pkcs_mechanisms_and_the_nss_internal_pkcs_.2311_module_for_aes.2c_des_or_rc4_when_making_an_ssl_connection.3f

   Once NSS starts using a token for a given operation (like S/MIME or SSL), it works hard to keep
   using that same token (so keys don't get moved around). Symmetric operations supported by NSS
   include the following: CKM_AES_XXX, CKM_DES3_XXX, CKM_DES_XXX, CKM_RC2_XXX, and CKM_RC4_XXX. NSS
   knows about all the mechanisms defined in PKCS #11 version 2.01, but will not perform those that
   aren't defined by NSS's policy mechanism.

   .. rubric:: When do NSS Applications spawn threads off the main thread, which in turn opens up a
      new PKCS #11 session?
      :name: when_do_nss_applications_spawn_threads_off_the_main_thread.2c_which_in_turn_opens_up_a_new_pkcs_.2311_session.3f

   This depends on the application. PKCS #11 sessions are cryptographic session states, independent
   of threads. In NSS based servers, multiple threads may call the same session, but two threads
   will not call the same session at the same time.

   .. rubric:: QUESTIONS ABOUT KEYS AND TOKENS
      :name: questions_about_keys_and_tokens

   .. rubric:: Is the PKCS #11 token treated in a read-only manner? That is, no token init, no key
      gens, no data puts, no cert puts, etc.?
      :name: is_the_pkcs_.2311_token_treated_in_a_read-only_manner.3f_that_is.2c_no_token_init.2c_no_key_gens.2c_no_data_puts.2c_no_cert_puts.2c_etc..3f

   If the token is marked read-only, then it will be treated as such. If the token is marked
   read/write and advertises that it can generate keys, NSS uses the token (through PKCS #11) to
   generate the key and loads the user's certificate into the token. If the token is marked
   read/write and does not advertise that it can generate keys, NSS generates the keys and loads
   them into the token.

   .. rubric:: How is private key handled when an external PKCS #11 module is loaded? Is it picked
      up from the token when securing, or does NSS expect it to be added in its private key database
      to use it?
      :name: how_is_private_key_handled_when_an_external_pkcs_.2311_module_is_loaded.3f_is_it_picked_up_from_the_token_when_securing.2c_or_does_nss_expect_it_to_be_added_in_its_private_key_database_to_use_it.3f

   While certificates may be read into the temporary database, private keys are never extracted from
   the PKCS #11 module unless the user is trying to back up the key. NSS represents each private key
   and a pointer to its PKCS #11 slot as a CK_OBJECT_HANDLE. When NSS needs to do anything with a
   private key, it calls the PCKS #11 module that holds the key.

   .. rubric:: If a PKCS #11 library reports that, for example, it does not support RSA signing
      operations, does NSS expect to be able to pull an RSA private key off the token using the
      C_GetAttributeValue call and then do the operation in software?
      :name: if_a_pkcs_.2311_library_reports_that.2c_for_example.2c_it_does_not_support_rsa_signing_operations.2c_does_nss_expect_to_be_able_to_pull_an_rsa_private_key_off_the_token_using_the_c_getattributevalue_call_and_then_do_the_operation_in_software.3f

   No. NSS will never try to pull private keys out of tokens (except as wrapped objects for PKCS
   #12). Operations the token does not support are considered impossible for the key to support.

   NSS may try to pull and load symmetric keys, usually if the key exchange happens in a token that
   does not support the symmetric algorithm. NSS works very hard not to have to pull any key out of
   a token (since that operation does not always work on all tokens).

   .. rubric:: If so, by what means does NSS attempt to retrieve the data? By searching for some
      fixed label attribute? Must the token store any temporary (session) objects?
      :name: if_so.2c_by_what_means_does_nss_attempt_to_retrieve_the_data.3f_by_searching_for_some_fixed_label_attribute.3f_must_the_token_store_any_temporary_.28session.29_objects.3f

   In general, yes, the token should store temporary session objects. This may not be necessary for
   "private key op only" tokens, but this is not guaranteed. You should be prepared to handle
   temporary objects. (Many NSS based server products will use temporary session objects, even for
   "private key op only" tokens.)

   .. rubric:: If a session key is unwrapped and stays on a hardware token, is it sufficient to
      support just the usual decryption mechanisms for it, or is it assumed that such a symmetric
      key will always be extractable from the token into the browser? The motivation for this is
      that some hardware tokens will prevent extraction of symmetric keys by design.
      :name: if_a_session_key_is_unwrapped_and_stays_on_a_hardware_token.2c_is_it_sufficient_to_support_just_the_usual_decryption_mechanisms_for_it.2c_or_is_it_assumed_that_such_a_symmetric_key_will_always_be_extractable_from_the_token_into_the_browser.3f_the_motivation_for_this_is_that_some_hardware_tokens_will_prevent_extraction_of_symmetric_keys_by_design.

   NSS attempts to extract an unwrapped key from a token only if the token cannot provide the
   necessary service with that key. For instance if you are decrypting an S/MIME message and you
   have unwrapped the DES key with the private key provided by a given token, NSS attempts to use
   that token to provide the DES encryption. Only if that token cannot do DES will NSS try to
   extract the key.

   .. rubric:: If the smartcard can't do key generation, will NSS do the key generation
      automatically?
      :name: if_the_smartcard_can.27t_do_key_generation.2c_will_nss_do_the_key_generation_automatically.3f

   Yes. If your token can do CKM_RSA_PKCS, and is writable, NSS displays it as one of the options to
   do key generation with. If the token cannot do CKM_RSA_PKCS_GEN_KEYPAIR, NSS uses its software
   key generation code and writes the private and public keys into the token using C_CreateObject.
   The RSA private key will contain all the attributes specified by PKCS #11 version 2.0. This is
   also true for CKM_DSA and CKM_DSA_GEN_KEYPAIR.

   .. rubric:: What is the C_GenerateKeyPair process? For example, what happens when an application
      in the a server asks an NSS based client to do a keypair generation while a smartCard is
      attached? How is the private key stored to the smartCard, and how is the public key sent to
      the server (with wrapping?).
      :name: what_is_the_c_generatekeypair_process.3f_for_example.2c_what_happens_when_an_application_in_the_a_server_asks_an_nss_based_client_to_do_a_keypair_generation_while_a_smartcard_is_attached.3f_how_is_the_private_key_stored_to_the_smartcard.2c_and_how_is_the_public_key_sent_to_the_server_.28with_wrapping.3f.29.

   The private key is created using C_GenerateKeyPair or stored using C_CreateObject (depending on
   who generates the key). NSS does not keep a copy of the generated key if it generates the key
   itself. Key generation in Mozilla clients is triggered either by the standard <KEYGEN> tag, or by
   the keygen functions off the window.crypto object. This is the same method used for generating
   software keys and certificates and is used by certificate authorities like VeriSign and Thawte.
   (Red Hat Certificate Server also uses this method). The public key is sent to the server
   base-64-DER-encoded with an (optional) signed challenge.

   .. rubric:: Are persistent objects that are stored on the token, such as private keys and
      certificates, created by the PKCS #11 module? Is it safe to assume that NSS never calls
      C_CreateObject for those persistent objects?
      :name: are_persistent_objects_that_are_stored_on_the_token.2c_such_as_private_keys_and_certificates.2c_created_by_the_pkcs_.2311_module.3f_is_it_safe_to_assume_that_nss_never_calls_c_createobject_for_those_persistent_objects.3f

   No. As stated in the answer to the preceding question, when NSS does a keygen it uses
   C_GenerateKeyPair if the token supports the keygen method. If the token does not support keygen,
   NSS generates the key internally and uses C_CreateObject to load the private key into the token.
   When the certificate is received after the keygen, NSS loads it into the token with
   C_CreateObject. NSS also does a similar operation for importing private keys and certificates
   through pkcs12.

   The above statement is true for read-write tokens only.

   .. rubric:: When and how does NSS generate private keys on the token?
      :name: when_and_how_does_nss_generate_private_keys_on_the_token.3f

   As stated above, NSS uses C_GenerateKeyPair if the token supports the keygen method. If an RSA
   key is being generated, the NSS application will present a list of all writable RSA devices asks
   the user to select which one to use, if a DSA key is being generated, it will present a list of
   all the writable DSA devices, if an EC key is being generated, it will present a list of all
   writable EC devices.

   .. rubric:: Does NSS ever use C_CopyObject to copy symmetric keys if it needs to reference the
      same key for different sessions?
      :name: does_nss_ever_use_c_copyobject_to_copy_symmetric_keys_if_it_needs_to_reference_the_same_key_for_different_sessions.3f

   No. This is never necessary. The PKCS #11 specification explicitly requires that symmetric keys
   must be visible to all sessions of the same application. NSS explicitly depends on this semantic
   without the use of C_CopyObject. If your module does not support this semantic, it will not work
   with NSS.

   .. rubric:: QUESTIONS ABOUT PINS
      :name: questions_about_pins

   .. rubric:: Will a password change ever be done on the token?
      :name: will_a_password_change_ever_be_done_on_the_token.3f

   Yes, NSS attempts to change the password in user mode only. (It goes to SSO mode only if your
   token identifies itself as CKF_LOGIN_REQUIRED, but not CKF_USER_INITIALIZED).

   It's perfectly valid to reject the password change request with a return value such as
   CKR_FUNCTION_NOT_SUPPORTED. If you do this, NSS applications display an appropriate error message
   for the user.

   .. rubric:: If I have my smart card which has initial PIN set at '9999', I insert it into my
      reader and download with my certificate (keygen completed), can I issue 'Change Password' from
      the Firefox to set a new PIN to the smart card? Any scenario that you can give me similar to
      this process (a way to issue a certificate on an initialized new card)?
      :name: if_i_have_my_smart_card_which_has_initial_pin_set_at__.279999.27.2c_i_insert_it_into_my_reader_and_download_with_my_certificate_.28keygen_completed.29.2c_can_i_issue_.27change_password.27_from_the_firefox_to_set_a_new_pin_to_the_smart_card.3f_any_scenario_that_you_can_give_me_similar_to_this_process_.28a_way_to_issue_a_certificate_on_an_initialized_new_card.29.3f

   Yes. First open the Tools/Options/Advanced/Security window in Mozilla and click Security Devices.
   Then select your PKCS #11 module, click View/Edit, select the token, and click Change Password.
   For this to work, you must supply a C_SetPIN function that operates as CKU_USER. Mozilla,
   Thunderbird, and Netscape products that use NSS have different UI to get the Security Devices
   dialog.

   To get a key into an initialized token, go to your local Certificate Authority and initiate a
   certificate request. Somewhere along the way you will be prompted with a keygen dialog. Normally
   this dialog does not have any options and just provides information; however, if you have more
   than one token that can be used in this key generation process (for example, your smartcard and
   the NSS internal PKCS#11 module), you will see a selection of "cards and databases" that can be
   used to generate your new key info.

   In the key generation process, NSS arranges for the key to have it's CKA_ID set to some value
   derived from the public key, and the public key will be extracted using C_GetAttributes. This key
   will be sent to the CA.

   At some later point, the CA presents the certificate to you (as part of this keygen, or in an
   e-mail, or you go back and fetch it from a web page once the CA notifies you of the arrival of
   the new certificate). NSS uses the public key to search all its tokens for the private key that
   matches that certificate. The certificate is then written to the token where that private key
   resides, and the certificate's CKA_ID is set to match the private key.

   .. rubric:: Why does Firefox require users to authenticate themselves by entering a PIN at the
      keyboard? Why not use a PIN pad or a fingerprint reader located on the token or reader?
      :name: why_does_firefox_require_users_to_authenticate_themselves_by_entering_a_pin_at_the_keyboard.3f_why_not_use_a_pin_pad_or_a_fingerprint_reader_located_on_the_token_or_reader.3f

   PKCS #11 defines how these kinds of devices work. There is an outstanding bug in Firefox to
   implement this support.