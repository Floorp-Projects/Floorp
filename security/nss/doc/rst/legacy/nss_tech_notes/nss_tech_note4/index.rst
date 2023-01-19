.. _mozilla_projects_nss_nss_tech_notes_nss_tech_note4:

nss tech note4
==============

.. _pulling_certificate_extension_information_out_of_ssl_certificates:

`Pulling certificate extension information out of SSL certificates <#pulling_certificate_extension_information_out_of_ssl_certificates>`__
------------------------------------------------------------------------------------------------------------------------------------------

.. container::

.. _nss_technical_note_4:

`NSS Technical Note: 4 <#nss_technical_note_4>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   **Note:** This document contains code snippets that focus on essential aspects of the task and
   often do not illustrate all the cleanup that needs to be done. Also, this document does not
   attempt to be an exhaustive survey of all possible ways to do a certain task; it merely tries to
   show a certain way.
   .. rubric:: Include these files
      :name: include_these_files

   #include "ssl.h"
   #include "cert.h"
   .. rubric:: Get the handle of the cert associated with an SSL connection
      :name: get_the_handle_of_the_cert_associated_with_an_ssl_connection

   *CERTCertificate\*  cert =  SSL_PeerCertificate(PRFileDesc \*fd);*
           If SSL client, this will get you the server's cert handle;
           If SSL server, this will get you the client's cert handle IF client auth is enabled
   *CERTCertificate\* cert = SSL_LocalCertificate(PRFileDesc \*fd);*
           If SSL client, this will get you the client cert's handle, IF client auth happened
           If SSL server, this will get you the server's cert handle
   .. rubric:: Don't forget to clean up the cert handle when you're done with it
      :name: don't_forget_to_clean_up_the_cert_handle_when_you're_done_with_it

   *void CERT_DestroyCertificate(CERTCertificate \*cert);*
   .. rubric:: Some info is readily available 
      :name: some_info_is_readily_available

   cert->subjectName (char*)
   cert->issuerName (char*)
   cert->emailAddr (char*)
        OR char \*CERT_GetCertificateEmailAddress(CERTCertificate \*cert);
   cert->keyUsage (unsigned int)
   .. rubric:: To break the issuer and subject names into components
      :name: to_break_the_issuer_and_subject_names_into_components

   Pass  &(cert->issuer) or &(cert->subject) to the following functions
      *char \*CERT_GetCommonName(CERTName \*name);
      char \*CERT_GetCertEmailAddress(CERTName \*name);
      char \*CERT_GetCountryName(CERTName \*name);
      char \*CERT_GetLocalityName(CERTName \*name);
      char \*CERT_GetStateName(CERTName \*name);
      char \*CERT_GetOrgName(CERTName \*name);
      char \*CERT_GetOrgUnitName(CERTName \*name);
      char \*CERT_GetDomainComponentName(CERTName \*name);
      char \*CERT_GetCertUid(CERTName \*name);*

   Example code to illustrate access to the info is given below.
   .. rubric:: Background on cert extensions
      :name: background_on_cert_extensions

   An extension has the following attributes

   -  Object Id (OID) : A unique OID represents an algorithm, a mechanism, a piece of information,
      etc. Examples: X500 RSA Encryption,  Certificate Basic Constraints, PKCS#7 Digested Data, etc.
      There is a long list of pre-defined OIDs, and new ones can be *added dynamically by an
      application.*
      The OID data structure contains an array of identifier bytes (each byte is a "level" in a
      hierarchical namespace), a text description, and some other things.
   -  Critical : indicates whether the extension is critical
   -  Value : The value of the extension

   .. rubric:: Looping through all extensions
      :name: looping_through_all_extensions

      *CERTCertExtension*\* extensions =cert->extensions;*
      *if (extensions)*
      *{*
      *    while (*extensions)*
      *    {*
      *        SECItem \*ext_oid = &(*extensions)->id;*
      *        SECItem \*ext_critical = &(*extensions)->critical;*
      *        SECItem \*ext_value = &(*extensions)->value;*
      *        /\* id attribute of the extension \*/*
      *        SECOidData \*oiddata = SECOID_FindOID(ext_oid);*
      *        if (oiddata == NULL)*
      *        {*
      */\* OID not found \*/*
      */\* SECItem ext_oid has type (SECItemType), data (unsigned char \*) and len (unsigned int)
      fields*
      *   - the application interprets these \*/*
      *.......*
      *        }*
      *        else*
      *        {*
      *char \*name = oiddata->desc; /\* name of the extension \*/*
      *.......*
      *        }*
      *        /\* critical attribute of the extension \*/*
      *        if (ext_critical->len > 0)*
      *        {*
      *if (ext_critical->data[0])*
      *    /\* the extension is critical \*/*
      *else*
      *    /\* the extension is not critical \*/*
      *        }*
      *        /\* value attribute of the extension \*/*
      *        /\* SECItem ext_value has type (SECItemType), data (unsigned char \*) and len
      (unsigned int) fields*
      *- the application interprets these \*/*
      *        SECOidTag oidtag = SECOID_FindOIDTag(ext_oid);*
      *        switch (oidtag)*
      *        {*
      *case a_tag_that_app_recognizes:*
      *    .....*
      *case .....*
      *    ......*
      *        }*
      *        extensions++;*
      *    }*
      *}*

   .. rubric:: An example custom cert extension
      :name: an_example_custom_cert_extension

      *struct \_myCertExtData*
      *{*
      *    SECItem version;*
      *    SECItem streetaddress;*
      *    SECItem phonenum;*
      *    SECItem rfc822name;*
      *    SECItem id;*
      *    SECItem maxusers;*
      *};*
      *typedef struct \_myCertExtData myCertExtData;*
      */\* template used for decoding the extension \*/*
      *const SEC_ASN1Template myCertExtTemplate[] = {*
      *    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof( myCertExtData ) },*
      *    { SEC_ASN1_INTEGER, offsetof(myCertExtData, version) },*
      *    { SEC_ASN1_OCTET_STRING, offsetof( myCertExtData, streetaddress ) },*
      *    { SEC_ASN1_OCTET_STRING, offsetof( myCertExtData, phonenum ) },*
      *    { SEC_ASN1_OCTET_STRING, offsetof( myCertExtData, rfc822name ) },*
      *    { SEC_ASN1_OCTET_STRING, offsetof( myCertExtData, id ) },*
      *    { SEC_ASN1_INTEGER, offsetof(myCertExtData, maxusers ) },*
      *    { 0 }*
      *};*
      */\* OID for my cert extension - replace 0xff with appropriate values*/*
      *static const unsigned char myoid[] = { 0xff, 0xff, 0xff, 0xff, .... };*
      *static const SECItem myoidItem = { (SECItemType) 0, (unsigned char \*)myoid, sizeof(myoid)
      };*
      *SECItem myextvalue;
      myCertExtData data;*
      *SECStatus rv = CERT_FindCertExtensionByOID(cert, &myoidItem, &myextvalue);
      if (rv == SECSuccess)
      {
          SEC_ASN1DecoderContext \* context = SEC_ASN1DecoderStart(NULL, &data, myCertExtTemplate);
          rv = SEC_ASN1DecoderUpdate( context, (const char \*)(myextvalue.data), myextvalue.len);
          if (rv == SECSuccess)
          {
              /\* Now you can extract info from SECItem fields of your extension data structure \*/
              /\* See "Misc helper functions" below \*/
              .......
              /\* free the SECItem fields \*/
              SECITEM_FreeItem(&data.version, PR_FALSE);
              SECITEM_FreeItem(&data.streetaddress, PR_FALSE);
              ......
              SECITEM_FreeItem(&data.maxusers, PR_FALSE);
          }
      }*

   .. rubric:: Some miscellaneous helper functions
      :name: some_miscellaneous_helper_functions

   -  Compare two SECItems (e.g., two OIDs)
      *PRBool SECITEM_ItemsAreEqual(const SECItem \*a, const SECItem \*b);*
   -  Interpreting a SECItem value as an integer
      If SECItem \*item->len <=4, then int value = *DER_GetInteger(item)*;
   -  Interpreting a SECItem value as a string
      Use string copy functions to copy item->len bytes from item->data and null terminate
      explicitly

   .. rubric:: Some higher level extension functions
      :name: some_higher_level_extension_functions

   -  Get a specific extension from the list of extensions, given the extension tag
      *SECStatus CERT_FindCertExtension  (CERTCertificate \*cert, int tag, SECItem \*value);*
   -  Get a specific extension from the ISSUER's cert\ *
      SECStatus CERT_FindIssuerCertExtension  (CERTCertificate \*cert, int tag, SECItem \*value);*
   -  Get the value of an extension with the given OID
      *SECStatus CERT_FindCertExtensionByOID (CERTCertificate \*cert, SECItem \*oid, SECItem
      \*value);*
   -  Get the decoded value of the "Basic Constraints" extension
      *SECStatus CERT_FindBasicConstraintExten (CERTCertificate \*cert, CERTBasicConstraints
      \*value);*
   -  Get value of the keyUsage extension.  This uses PR_Alloc to allocate buffer for the decoded
      value, The  caller should free up the storage allocated in value->data.
      *SECStatus CERT_FindKeyUsageExtension (CERTCertificate \*cert, SECItem \*value);*
   -  Get decoded value of the subjectKeyID extension.  This uses PR_Alloc to allocate buffer for
      the decoded value, The  caller should free up the storage allocated in value->data.
      *SECStatus CERT_FindSubjectKeyIDExten (CERTCertificate \*cert, SECItem \*retItem);*

   *
   *

.. _for_more_information:

`For more information <#for_more_information>`__
------------------------------------------------

.. container::

   -  Browse through the NSS source code online at
      http://lxr.mozilla.org/mozilla/source/security/nss/  and http://lxr.mozilla.org/security/
   -  documentation on some cert funcs
      `http://www.mozilla.org/projects/security/pki/nss/ref/ssl/sslcrt.html <https://www.mozilla.org/projects/security/pki/nss/ref/ssl/sslcrt.html>`__