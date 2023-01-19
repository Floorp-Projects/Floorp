.. _mozilla_projects_nss_new_nss_samples:

New NSS Samples
===============

.. _new_nss_sample_code:

`New NSS Sample Code <#new_nss_sample_code>`__
----------------------------------------------

.. container::

   This collection of sample code demonstrates how NSS can be used for cryptographic operations,
   certificate handling, SSL, etc. It also demonstrates some best practices in the application of
   cryptography.

   |
   | These new examples are a work in progress. See
     https://bugzilla.mozilla.org/show_bug.cgi?id=490238

   How to download the samples:

   .. code:: bz_comment_text

      hg clone https://hg.mozilla.org/projects/nss; cd nss; hg update SAMPLES_BRANCH

   Samples list:

   #. :ref:`mozilla_projects_nss_nss_sample_code_sample1_-_hashing`
   #. :ref:`mozilla_projects_nss_nss_sample_code_sample2_-_initialize_nss_database`
   #. :ref:`mozilla_projects_nss_encrypt_decrypt_mac_using_token`
   #. :ref:`mozilla_projects_nss_encrypt_decrypt_mac_keys_as_session_objects`
   #. :ref:`mozilla_projects_nss_nss_sample_code_enc_dec_mac_output_plblic_key_as_csr`
   #. :ref:`mozilla_projects_nss_nss_sample_code_enc_dec_mac_using_key_wrap_certreq_pkcs10_csr`

   Common code used by these samples:

   #. :ref:`mozilla_projects_nss_nss_sample_code_utiltiies_for_nss_samples`

   Thanks are due to Shailendra Jain, Mozilla Community member, who is the principal author of these
   samples.