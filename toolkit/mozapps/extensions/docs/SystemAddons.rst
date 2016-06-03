Firefox System Add-on Update Protocol
=====================================
This document describes the protocol that Firefox uses when retrieving updates
for System Add-ons from the automatic update service (AUS, currently `Balrog`_),
and the expected behavior of Firefox based on the updater service's response.

.. _Balrog: https://wiki.mozilla.org/Balrog

System Add-ons
--------------
System add-ons:

* Are add-ons that ship with Firefox and cannot be disabled
* Can be updated by Firefox depending on the AUS response to Firefox's update
  request
* Are stored in two locations:

  * The **default** set ships with Firefox and is stored in the application
    directory.
  * The **update** set is stored in the user’s profile directory. If an add-on
    is both in the update and default set, the update version gets precedence.

Update Request
--------------
To determine what updates to install, Firefox makes an HTTP **GET** request to
AUS once a day via a URL of the form::

  https://aus5.mozilla.org/update/3/SystemAddons/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml

The path segments surrounded by ``%`` symbols are variable fields that Firefox
fills in with information about itself and the environment it's running in:

``VERSION``
  Firefox version number
``BUILD_ID``
  Build ID
``BUILD_TARGET``
  Build target
``LOCALE``
  Build locale
``CHANNEL``
  Update channel
``OS_VERSION``
  OS Version
``DISTRIBUTION``
  Firefox Distribution
``DISTRIBUTION_VERSION``
  Firefox Distribution version

Update Response
---------------
AUS should respond with an XML document that looks something like this:

.. code-block:: xml

  <?xml version="1.0"?>
  <updates>
    <addons>
      <addon id="loop@mozilla.org" URL="https://ftp.mozilla.org/pub/system-addons/hello/loop@mozilla.org-1.0.xpi" hashFunction="sha512" hashValue="abcdef123" size="1234" version="1.0"/>
      <addon id="pocket@mozilla.org" URL="https://ftp.mozilla.org/pub/system-addons/pocket/pocket@mozilla.org-1.0.xpi" hashFunction="sha512" hashValue="abcdef123" size="1234" version="1.0"/>
    </addons>
  </updates>

* The root element is ``<updates>``, used for all updater responses.
* The only child of ``<updates>`` is ``<addons>``, which represents a list of
  system add-ons to update.
* Within ``<addons>`` are several ``<addon>`` tags, each one corresponding to a
  system add-on to update.

``<addon>`` tags **must** have the following attributes:

``id``
  The extension ID
``URL``
  URL to a signed XPI of the specified add-on version to download
``hashFunction``
  Identifier of the hash function used to generate the hashValue attribute.
``hashValue``
  Hash of the XPI file linked from the URL attribute, calculated using the function specified in the hashValue attribute.
``size``
  Size (in bytes) of the XPI file linked from the URL attribute.
``version``
  Version number of the add-on

Update Behavior
---------------
After receiving the update response, Firefox modifies the **update** add-ons
according to the following algorithm:

1. If the ``<addons>`` tag is missing in the response,
   **disable all system add-ons**, including both the **update** and **default**
   sets.
2. If no add-ons were specified in the response (i.e. the ``<addons>`` tag has
   no children but is present), do nothing and finish.
3. If the **update** add-on set is equal to the set of add-ons specified in the
   update response, do nothing and finish.
4. If the set of **default** add-ons is equal to the set of add-ons specified in
   the update response, remove all the  **update** add-ons and finish.
5. Download each add-on specified in the update response and store them in the
   "downloaded add-on set". A failed download **must** abort the entire system
   add-on update.
6. Validate the downloaded add-ons. The following **must** be true for all
   downloaded add-ons, or the update process is aborted:

   a. The ID and version of the downloaded add-on must match the specified ID or
      version in the update response.
   b. The hash provided in the update response must match the downloaded add-on
      file.
   c. The downloaded add-on file size must match the size given in the update
      response.
   d. The add-on must be compatible with Firefox (i.e. it must not be for a
      different application, such as Thunderbird).
   e. The add-on must be packed (i.e. be an XPI file).
   f. The add-on must be restartless.
   g. The add-on must be signed by the system add-on root certificate.

6. Once all downloaded add-ons are validated, install them into the profile
   directory as part of the **update** set.
7. Disable any **default** add-ons that were not present in the update response.

Notes on the update process:

* Add-ons are considered "equal" if they have the same ID and version number.

Examples
--------
The follow section describes common situations that we have or expect to run
into and how the protocol described above handles them.

For simplicity, unless otherwise specified, all examples assume that there are
two system add-ons in existence: **Loop** and **Pocket**.

Basic
~~~~~
A user has Firefox 45, which shipped with Loop 1.0 and Pocket 1.0. We want to
update users to Loop 2.0. AUS sends out the following update response:

.. code-block:: xml

  <updates>
    <addons>
      <addon id="loop@mozilla.org" URL="https://ftp.mozilla.org/pub/system-addons/hello/loop@mozilla.org-2.0.xpi" hashFunction="sha512" hashValue="abcdef123" size="1234" version="2.0"/>
      <addon id="pocket@mozilla.org" URL="https://ftp.mozilla.org/pub/system-addons/pocket/pocket@mozilla.org-1.0.xpi" hashFunction="sha512" hashValue="abcdef123" size="1234" version="1.0"/>
    </addons>
  </updates>

Firefox will download Loop 2.0 and Pocket 1.0 and store them in the profile directory.

Missing Add-on
~~~~~~~~~~~~~~
A user has Firefox 45, which shipped with Loop 1.0 and Pocket 1.0. We want to
update users to Loop 2.0, but accidentally forget to specify Pocket in the
update response. AUS sends out the following:

.. code-block:: xml

  <updates>
    <addons>
      <addon id="loop@mozilla.org" URL="https://ftp.mozilla.org/pub/system-addons/hello/loop@mozilla.org-2.0.xpi" hashFunction="sha512" hashValue="abcdef123" size="1234" version="2.0"/>
    </addons>
  </updates>

Firefox will download Loop 2.0 and store it in the profile directory. It will
disable Pocket completely.

Disable all system add-ons
~~~~~~~~~~~~~~~~~~~~~~~~~~
A response from AUS with an empty add-on set will *disable all system add-ons*:

.. code-block:: xml

  <updates>
    <addons></addons>
  </updates>

Rollout
~~~~~~~
A user has Firefox 45, which shipped with Loop 1.0 and Pocket 1.0. We want to
rollout Loop 2.0 at a 10% sample rate. 10% of the time, AUS sends out:

.. code-block:: xml

  <updates>
    <addons>
      <addon id="loop@mozilla.org" URL="https://ftp.mozilla.org/pub/system-addons/hello/loop@mozilla.org-2.0.xpi" hashFunction="sha512" hashValue="abcdef123" size="1234" version="2.0"/>
      <addon id="pocket@mozilla.org" URL="https://ftp.mozilla.org/pub/system-addons/pocket/pocket@mozilla.org-1.0.xpi" hashFunction="sha512" hashValue="abcdef123" size="1234" version="1.0"/>
    </addons>
  </updates>

With this response, Firefox will download Pocket 1.0 and Loop 2.0 and install
them into the profile directory.

The other 90% of the time, AUS sends out an empty response:

.. code-block:: xml

  <updates></updates>

With the empty response, Firefox will not make any changes. This means users who
haven’t seen the 10% update response will stay on Loop 1.0, and users who have
seen it will stay on Loop 2.0.

Once we’re happy with the rollout and want to switch to 100%, AUS will send the
10% update response to 100% of users, upgrading everyone to Loop 2.0.

Rollback
~~~~~~~~
This example continues from the “Rollout” example. If, during the 10% rollout,
we find a major issue with Loop 2.0, we want to roll all users back to Loop 1.0.
AUS sends out the following:

.. code-block:: xml

  <updates>
    <addons>
      <addon id="loop@mozilla.org" URL="https://ftp.mozilla.org/pub/system-addons/hello/loop@mozilla.org-1.0.xpi" hashFunction="sha512" hashValue="abcdef123" size="1234" version="1.0"/>
      <addon id="pocket@mozilla.org" URL="https://ftp.mozilla.org/pub/system-addons/pocket/pocket@mozilla.org-1.0.xpi" hashFunction="sha512" hashValue="abcdef123" size="1234" version="1.0"/>
    </addons>
  </updates>

For users who have updated, Firefox will download Loop 1.0 and Pocket 1.0 and
install them into the profile directory. For users that haven’t yet updated,
Firefox will see that the **default** add-on set matches the set in the update
ping and clear the **update** add-on set.

Disable an Add-on
~~~~~~~~~~~~~~~~~
A user has Firefox 45, with Pocket 1.0 and Loop 1.0. Loop 1.0 ends up having a
serious bug, and we want to disable the add-on completely while we work on a
fix. AUS sends out the following:

.. code-block:: xml

  <updates>
    <addons>
      <addon id="pocket@mozilla.org" URL="https://ftp.mozilla.org/pub/system-addons/pocket/pocket@mozilla.org-1.0.xpi" hashFunction="sha512" hashValue="abcdef123" size="1234" version="1.0"/>
    </addons>
  </updates>

Firefox will download Pocket 1.0 and install it to the profile directory, and disable Loop.
