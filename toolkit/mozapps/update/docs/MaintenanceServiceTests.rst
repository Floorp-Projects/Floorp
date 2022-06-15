Maintenance Service Tests
=========================

The automated tests for the Mozilla Maintenance Service are a bit tricky. They
are located in ``toolkit/mozapps/update/tests/unit_service_updater/`` and they
allow for automated testing of application update using the Service.

In automation, everything gets signed and the tests properly check that things
like certificate verification work. If, however, you want to run the tests
locally, the MAR and the updater binary will not be signed. Thus, the
verification of those certificates will fail and the tests will not run
properly.

We don't want these tests to just always fail if someone runs large amounts of
tests locally. To avoid this, the tests basically just unconditionally pass if
you run them locally and don't take the time to set them up properly.

If you want them to actually run locally, you will need to set up your
environment properly.

Setting Up to Run the Tests Locally
-----------------------------------

In order to run the service tests locally, we have to bypass much of the
certificate verification. Thus, this method may not be helpful if you need to
test that feature in particular.

Add Fallback Key to Registry
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First, you will need to add the fallback key to the registry. Normally, the
Firefox installer writes some certificate information to a registry key in an
installation-specific place. In testing, however, we can't get the permissions
to write this key, nor can we have the test environment predict every possible
installation directory that we might test with. To get around this problem, if
the Service can't find the installation-specific key, it will check a static
fallback location.

The easiest way to correctly set up the fallback key is to copy the text below
into a ``.reg`` file and then double click it in the file browser to merge it
into the registry.

.. code::

   Windows Registry Editor Version 5.00

   [HKEY_LOCAL_MACHINE\SOFTWARE\Mozilla\MaintenanceService\3932ecacee736d366d6436db0f55bce4]

   [HKEY_LOCAL_MACHINE\SOFTWARE\Mozilla\MaintenanceService\3932ecacee736d366d6436db0f55bce4\0]
   "issuer"="DigiCert SHA2 Assured ID Code Signing CA"
   "name"="Mozilla Corporation"

   [HKEY_LOCAL_MACHINE\SOFTWARE\Mozilla\MaintenanceService\3932ecacee736d366d6436db0f55bce4\1]
   "issuer"="DigiCert Assured ID Code Signing CA-1"
   "name"="Mozilla Corporation"

   [HKEY_LOCAL_MACHINE\SOFTWARE\Mozilla\MaintenanceService\3932ecacee736d366d6436db0f55bce4\2]
   "issuer"="Mozilla Fake CA"
   "name"="Mozilla Fake SPC"

Build without Certificate Verification
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To disable certificate verification, add this build flag to your ``mozconfig``
file:

.. code::

   ac_add_options --enable-unverified-updates

You will need to rebuild for this to take effect.

Copy the Maintenance Service Binary
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This step will assume that you already have the Maintenance Service installed.

First, move the existing Maintenance Service binary out of the way. It will
initially be located at
``C:\Program Files (x86)\Mozilla Maintenance Service\maintenanceservice.exe``.
An easy way to do this is to append ``.bak`` to its name. You should probably
restore your original Maintenance Service binary when you are done testing.

Now, copy the Maintenance Service binary that you built into that directory.
It will be located at ``<obj directory>\dist\bin\maintenanceservice.exe``.

If you make changes to the Maintenance Service and rebuild, you will have to
repeat this step.

Running the Tests
-----------------

You should now be ready run a service test:

.. code::

   ./mach test toolkit/mozapps/update/tests/unit_service_updater/<test>

Or run all of them:

.. code::

   ./mach test toolkit/mozapps/update/tests/unit_service_updater
