
.. -*- Mode: rst; fill-column: 100; -*-

=======================================
Downloadable content (DLC) UI Telemetry
=======================================

The downloadable content service (DLC) uses UI telemetry to report successful and failed performed actions (like downloading new content or synchronizing the catalog of available content).

Note: The download content service downloads additional assets for using the browser (like fonts, dictionaries or translations) and is not related to downloads that the user performs when browsing.

All UI events use action ``action.1`` and method ``service``. Additional information is structured as JSON blobs (``extras`` field).

Downloading content
===================

Telemetry about downloaded content has the extra field ``action`` set to ``dlc_download``. The ``result`` field will be set to ``success`` or ``failure`` depending on whether the content could be downloaded successfully. The ``content`` field will contain the UUID of the content (as listed in the catalog, see "Synchronizing catalog" section) that was downloaded or failed to be downloaded. This is helpful to identify potentially corrupt content.

Telemetry extras sent for a successful content download might look like this:

.. code-block:: js

    extras: {
        "action": "dlc_download",
        "result": "success",
        "content": "25610abb-5dc8-fd75-40e7-990507f010c4"
    }

For failed content downloads an additional ``error`` field contains the error type that occured when downloading the content. The value can be one of:

- no_network
- network_metered
- disk_space
- checksum
- io_disk
- io_network
- memory
- server
- logic
- unrecoverable

Telemetry extras sent for a failed content download might look like this:

.. code-block:: js

    extras: {
        "action": "dlc_download",
        "result": "failure",
        "content": "25610abb-5dc8-fd75-40e7-990507f010c4"
        "error": "io_network"
    }

Synchronizing catalog
=====================

The app has a local catalog of content that can be downloaded. This catalog can be updated and is synchronized from a server. Telemetry about synchronizing the catalog has the extra field ``action`` set to ``dlc_sync``. Like for content downloads the ``result`` field will be set to ``success`` or ``failure`` depending on whether the synchronization was successful or not.

Telemetry extras for a successful synchronization contain two additional fields:

- ``updated`` - This field is set to ``true`` if the local catalog was updated or false if a synchronization was successful but the catalog has not changed since the last synchronization
- ``action_required`` - If the value is ``true`` then this update required a local action: New or updated content needs to be downloaded or old content needs to be removed. If the value is ``false`` then the catalog was updated but no further action is required by the client.

A successful synchronization might send the following telemetry extras:

.. code-block:: js

    extras: {
        "action": "dlc_sync",
        "result": "success",
        "updated": false,
        "action_required": false
    }

Telemetry extras for a failed synchronization will contain an additional ``error`` field with the same possible values as listed for the failed download case.
