.. _mozilla_projects_nss_pkcs11_module_installation:

PKCS11 module installation
==========================

.. container::

   `PKCS #11 </en-US/PKCS11>`__ modules are external modules which add to Firefox support for
   smartcard readers, biometric security devices, and external certificate stores. This article
   covers the two methods for installing PKCS #11 modules into Firefox. Users can use the
   preferences dialog to install or remove PKCS #11 module. Extensions can programmatically manage
   PKCS #11 modules using the nsIPKCS11 programming interface.

   .. note::

      **Note:** The information in this article is specific to Firefox 3.5 and newer. Older versions
      of Firefox may support the
      `window.pkcs11 <https://developer.mozilla.org/en-US/docs/Web/API/Window/pkcs11>`__ property
      for installing PKCS #11 modules.

.. _using_the_firefox_preferences_to_install_pkcs_11_modules:

`Using the Firefox preferences to install PKCS #11 modules <#using_the_firefox_preferences_to_install_pkcs_11_modules>`__
-------------------------------------------------------------------------------------------------------------------------

.. container::

   #. Save the PKCS #11 module to a permanent location on your local computer
   #. Open the Firefox preferences dialog. Choose "Advanced" > "Encryption" > "Security Devices"
   #. Choose "Load"
   #. Enter a name for the security module, such as "My Client Database". NOTE: there is currently a
      bug in Firefox where international characters may cause problems.
   #. Choose "Browse..." to find the location of the PKCS #11 module on your local computer, and
      choose "OK" when done.

.. _provisioning_pkcs_11_modules_using_the_pkcs11_api:

`Provisioning PKCS #11 modules using the pkcs11 API <#provisioning_pkcs_11_modules_using_the_pkcs11_api>`__
-----------------------------------------------------------------------------------------------------------

.. container::

   Starting with Firefox 58, extensions can use the ``pkcs11`` browser extension API to enumerate
   PKCS #11 modules and make them accessible to the browser as sources of keys and certificates.

.. _see_also:

`See also <#see_also>`__
------------------------

.. container::

   -  `Web security <https://developer.mozilla.org/en-US/docs/Web/Security>`__
   -  :ref:`mozilla_projects_nss_pkcs11`
   -  ``pkcs11.installModule()``
   -  ``pkcs11.isModuleInstalled()``