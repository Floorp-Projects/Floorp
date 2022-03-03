.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nns_3_76.rst
   nss_3_75.rst
   nss_3_74.rst
   nss_3_68_2.rst
   nss_3_73_1.rst
   nss_3_73.rst
   nss_3_72_1.rst
   nss_3_72.rst
   nss_3_71.rst
   nss_3_70.rst
   nss_3_69_1.rst
   nss_3_69.rst
   nss_3_68_2.rst
   nss_3_68_1.rst
   nss_3_68.rst
   nss_3_67.rst
   nss_3_66.rst
   nss_3_65.rst
   nss_3_64.rst

.. note::

   **NSS 3.76** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_76_release_notes`

   **NSS 3.68.2** is the latest LTS version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_68_2_release_notes`


.. container::

   Changes in 3.76 included in this release:

   - Bug 1755555 - Hold tokensLock through nssToken_GetSlot calls in nssTrustDomain_GetActiveSlots. r=rrelyea 
   - Bug 1370866 - Check return value of PK11Slot_GetNSSToken. r=djackson 
   - Bug 1747957 - Use Wycheproof JSON for RSASSA-PSS, r=nss-reviewers,bbeurdouche 
   - Bug 1679803 - Add SHA256 fingerprint comments to old certdata.txt entries. r=nss-reviewers,bbeurdouche
   - Bug 1753505 - Avoid truncating files in nss-release-helper.py. r=bbeurdouche
   - Bug 1751157 - Throw illegal_parameter alert for illegal extensions in handshake message. r=djackson

