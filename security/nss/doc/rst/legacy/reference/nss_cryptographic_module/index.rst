.. _mozilla_projects_nss_reference_nss_cryptographic_module:

NSS cryptographic module
========================

.. container::

   This chapter describes the data types and functions that one can use to perform cryptographic
   operations with the NSS cryptographic module. The NSS cryptographic module uses the industry
   standard `PKCS #11 <http://www.rsasecurity.com/rsalabs/node.asp?id=2133>`__ v2.20 as its API with
   some extensions. Therefore, an application that supports PKCS #11 cryptographic tokens can be
   easily modified to use the NSS cryptographic module.

   The NSS cryptographic module has two modes of operation: the non-FIPS (default) mode and FIPS
   mode. The FIPS mode is an Approved mode of operation compliant to FIPS 140-2. Both modes of
   operation use the same data types but are implemented by different functions.

   -  The standard PKCS #11 function C_GetFunctionList or the equivalent NSC_GetFunctionList
      function returns pointers to the functions that implement the default mode of operation.
   -  To enable the FIPS mode of operation, use the function FC_GetFunctionList instead to get
      pointers to the functions that implement the FIPS mode of operation.

   The NSS cryptographic module also exports the function NSC_ModuleDBFunc for managing the NSS
   module database secmod.db. The following sections document the data types and functions.

   -  :ref:`mozilla_projects_nss_reference_nss_cryptographic_module_data_types`
   -  :ref:`mozilla_projects_nss_pkcs11_functions`
   -  :ref:`mozilla_projects_nss_reference_nss_cryptographic_module_fips_mode_of_operation`
   -  NSC_ModuleDBFunc