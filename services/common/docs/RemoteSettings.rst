.. _services/remotesettings:

===============
Remote Settings
===============

The `remote-settings.js <https://dxr.mozilla.org/mozilla-central/source/services/common/remote-settings.js>`_ module offers the ability to fetch remote settings that are kept in sync with Mozilla servers.


Usage
=====

The `get()` method returns the list of entries for a specific key. Each entry can have arbitrary attributes, and can only be modified on the server.

.. code-block:: js

    const { RemoteSettings } = ChromeUtils.import("resource://services-common/remote-settings.js", {});

    const data = await RemoteSettings("a-key").get();

    /*
      data == [
        {label: "Yahoo",  enabled: true,  weight: 10, id: "d0782d8d", last_modified: 1522764475905},
        {label: "Google", enabled: true,  weight: 20, id: "8883955f", last_modified: 1521539068414},
        {label: "Ecosia", enabled: false, weight: 5,  id: "337c865d", last_modified: 1520527480321},
      ]
    */

    for(const entry of data) {
      // Do something with entry...
      // await InternalAPI.load(entry.id, entry.label, entry.weight);
    });

.. note::
    The ``id`` and ``last_modified`` (timestamp) attributes are assigned by the server.

Options
-------

The list can optionally be filtered or ordered:

.. code-block:: js

    const subset = await RemoteSettings("a-key").get({
      filters: {
        "enabled": true,
      },
      order: "-weight"
    });

Events
------

The ``change`` event allows to be notified when the remote settings are changed. The event ``data`` attribute contains the whole new list of settings.

.. code-block:: js

    RemoteSettings("a-key").on("change", event => {
      const { data } = event;
      for(const entry of data) {
        // Do something with entry...
        // await InternalAPI.reload(entry.id, entry.label, entry.weight);
      }
    });

.. note::
    Currently, the update of remote settings is triggered by the `nsBlocklistService <https://dxr.mozilla.org/mozilla-central/source/toolkit/mozapps/extensions/nsBlocklistService.js>`_ (~ every 24H).

File attachments
----------------

When an entry has a file attached to it, it has an ``attachment`` attribute, which contains the file related information (url, hash, size, mimetype, etc.). Remote files are not downloaded automatically.

.. code-block:: js

    const data = await RemoteSettings("a-key").get();

    data.filter(d => d.attachment)
        .forEach(async ({ attachment: { url, filename, size } }) => {
          if (size < OS.freeDiskSpace) {
            await downloadLocally(url, filename);
          }
        });


Uptake Telemetry
================

Some :ref:`uptake telemetry <telemetry/collection/uptake>` is collected in order to monitor how remote settings are propagated.

It is submitted to a single :ref:`keyed histogram <histogram-type-keyed>` whose id is ``UPTAKE_REMOTE_CONTENT_RESULT_1`` and the keys are prefixed with ``main/`` (eg. ``main/a-key`` in the above example).


Create new remote settings
==========================

Staff members can create new kinds of remote settings, following `this documentation <https://mana.mozilla.org/wiki/pages/viewpage.action?pageId=66655528>`_.

It basically consists in:

#. Choosing a key (eg. ``search-providers``)
#. Assigning collaborators to editors and reviewers groups
#. (*optional*) Define a JSONSchema to validate entries
#. (*optional*) Allow attachments on entries

And once done:

#. Create, modify or delete entries and let reviewers approve the changes
#. Wait for Firefox to pick-up the changes for your settings key
