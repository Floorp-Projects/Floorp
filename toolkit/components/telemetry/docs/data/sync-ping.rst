
"sync" ping
===========

This is an aggregated format that contains information about each sync that occurred during a timeframe. It is submitted every 12 hours, and on browser shutdown, but only if the ``syncs`` property would not be empty. The ping does not contain the environment block, nor the clientId.

Each item in the ``syncs`` property is generated after a sync is completed, for both successful and failed syncs, and contains measurements pertaining to sync performance and error information.

A JSON-schema document describing the exact format of the ping's payload property can be found at `services/sync/tests/unit/sync\_ping\_schema.json <https://dxr.mozilla.org/mozilla-central/source/services/sync/tests/unit/sync_ping_schema.json>`_.

Structure:

.. code-block:: js

    {
      version: 4,
      type: "sync",
      ... common ping data
      payload: {
        version: 1,
        os : { ... }, // os data from the current telemetry environment. OS specific, but typically includes name, version and locale.
        discarded: <integer count> // Number of syncs discarded -- left out if zero.
        why: <string>, // Why did we submit the ping? Either "shutdown", "schedule", or "idchanged".
        uid: <string>, // Hashed FxA unique ID, or string of 32 zeros. If this changes between syncs, the payload is submitted.
        deviceID: <string>, // Hashed FxA Device ID, hex string of 64 characters, not included if the user is not logged in. If this changes between syncs, the payload is submitted.
        sessionStartDate: <ISO date>, // Hourly precision, ISO date in local time
        // Array of recorded syncs. The ping is not submitted if this would be empty
        syncs: [{
          when: <integer milliseconds since epoch>,
          took: <integer duration in milliseconds>,
          didLogin: <bool>, // Optional, is this the first sync after login? Excluded if we don't know.
          why: <string>, // Optional, why the sync occurred, excluded if we don't know.

          // Optional, excluded if there was no error.
          failureReason: {
            name: <string>, // "httperror", "networkerror", "shutdownerror", etc.
            code: <integer>, // Only present for "httperror" and "networkerror".
            error: <string>, // Only present for "othererror" and "unexpectederror".
            from: <string>, // Optional, and only present for "autherror".
          },

          // Optional, excluded if we couldn't get a valid uid or local device id
          devices: [{
            os: <string>, // OS string as reported by Services.appinfo.OS, if known
            version: <string>, // Firefox version, as reported by Services.appinfo.version if known
            id: <string>, // Hashed FxA device id for device
            type: <string>, // broad device "type", as reported by fxa ("mobile", "tv", etc).
            syncID: <string>, // Hashed Sync device id for device, if the user is a sync user.
          }],

          // Internal sync status information. Omitted if it would be empty.
          status: {
            sync: <string>, // The value of the Status.sync property, unless it indicates success.
            service: <string>, // The value of the Status.service property, unless it indicates success.
          },
          // Information about each engine's sync.
          engines: [
            {
              name: <string>, // "bookmarks", "tabs", etc.
              took: <integer duration in milliseconds>, // Optional, values of 0 are omitted.

              status: <string>, // The value of Status.engines, if it holds a non-success value.

              // Optional, excluded if all items would be 0. A missing item indicates a value of 0.
              incoming: {
                applied: <integer>, // Number of records applied
                succeeded: <integer>, // Number of records that applied without error
                failed: <integer>, // Number of records that failed to apply
                newFailed: <integer>, // Number of records that failed for the first time this sync
                reconciled: <integer>, // Number of records that were reconciled
              },

              // Optional, excluded if it would be empty. Records that would be
              // empty (e.g. 0 sent and 0 failed) are omitted.
              outgoing: [
                {
                  sent: <integer>, // Number of outgoing records sent. Zero values are omitted.
                  failed: <integer>, // Number that failed to send. Zero values are omitted.
                }
              ],
              // Optional, excluded if there were no errors
              failureReason: { ... }, // Same as above.

              // Timings and counts for detailed steps that the engine reported
              // as part of its sync. Optional; omitted if the engine didn't
              // report any extra steps.
              steps: {
                name: <string>, // The step name.
                took: <integer duration in milliseconds>, // Omitted if 0.
                // Optional, extra named counts (e.g., number of items handled
                // in this step). Omitted if the engine didn't report extra
                // counts.
                counts: [
                  {
                    name: <string>, // The counter name.
                    count: <integer>, // The counter value.
                  },
                ],
              },

              // Optional, excluded if it would be empty or if the engine cannot
              // or did not run validation on itself.
              validation: {
                // Optional validator version, default of 0.
                version: <integer>,
                checked: <integer>,
                took: <non-monotonic integer duration in milliseconds>,
                // Entries with a count of 0 are excluded, the array is excluded if no problems are found.
                problems: [
                  {
                    name: <string>, // The problem identified.
                    count: <integer>, // Number of times it occurred.
                  }
                ],
                // Format is same as above, this is only included if we tried and failed
                // to run validation, and if it's present, all other fields in this object are optional.
                failureReason: { ... },
              }
            }
          ],
          // Information about any storage migrations that have occurred. Omitted if it would be empty.
          migrations: [
            // See the section on the `migrations` array for detailed documentation on what may appear here.
            {
              type: <string identifier>,
              // per-type data
            }
          ]
        }],
        // The "node type" as reported by the token server. This will not change
        // from sync to sync, so is reported once per ping. Optional because it
        // will not appear if the token server omits this information, but in
        // general, we will expect all "new" pings to have it.
        syncNodeType: <string>,
        events: [
          event_array // See events below.
        ],
        histograms: { ... } // See histograms below
      }
    }

info
----

discarded
~~~~~~~~~

The ping may only contain a certain number of entries in the ``"syncs"`` array, currently 500 (it is determined by the ``"services.sync.telemetry.maxPayloadCount"`` preference). Entries beyond this are discarded, and recorded in the discarded count.

syncs.took
~~~~~~~~~~

These values should be monotonic. If we can't get a monotonic timestamp, -1 will be reported on the payload, and the values will be omitted from the engines. Additionally, the value will be omitted from an engine if it would be 0 (either due to timer inaccuracy or finishing instantaneously).

uid
~~~~~~~~~

This property containing a hash of the FxA account identifier, which is a 32 character hexadecimal string. In the case that we are unable to authenticate with FxA and have never authenticated in the past, it will be a placeholder string consisting of 32 repeated ``0`` characters.

syncs.why
~~~~~~~~~

One of the following values:

- ``startup``: This is the first sync triggered after browser startup.
- ``schedule``: This is a sync triggered because it has been too long since the last sync.
- ``score``: This sync is triggered by a high score value one of sync's trackers, indicating that many changes have occurred since the last sync.
- ``user``: The user manually triggered the sync.
- ``tabs``: The user opened the synced tabs sidebar, which triggers a sync.

syncs.status
~~~~~~~~~~~~

The ``engine.status``, ``payload.status.sync``, and ``payload.status.service`` properties are sync error codes, which are listed in `services/sync/modules/constants.js <https://dxr.mozilla.org/mozilla-central/source/services/sync/modules/constants.js>`_, and success values are not reported.

syncs.failureReason
~~~~~~~~~~~~~~~~~~~

Stores error information, if any is present. Always contains the "name" property, which identifies the type of error it is. The types can be.

- ``httperror``: Indicates that we received an HTTP error response code, but are unable to be more specific about the error. Contains the following properties:

    - ``code``: Integer HTTP status code.

- ``nserror``: Indicates that an exception with the provided error code caused sync to fail.

    - ``code``: The nsresult error code (integer).

- ``shutdownerror``: Indicates that the sync failed because we shut down before completion.

- ``autherror``: Indicates an unrecoverable authentication error.

    - ``from``: Where the authentication error occurred, one of the following values: ``tokenserver``, ``fxaccounts``, or ``hawkclient``.

- ``othererror``: Indicates that it is a sync error code that we are unable to give more specific information on. As with the ``syncStatus`` property, it is a sync error code, which are listed in `services/sync/modules/constants.js <https://dxr.mozilla.org/mozilla-central/source/services/sync/modules/constants.js>`_.

    - ``error``: String identifying which error was present.

- ``unexpectederror``: Indicates that some other error caused sync to fail, typically an uncaught exception.

   - ``error``: The message provided by the error.

- ``sqlerror``: Indicates that we received a ``mozIStorageError`` from a database query.

    - ``code``: Value of the ``error.result`` property, one of the constants listed `here <https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Reference/Interface/MozIStorageError#Constants>`_.

syncs.engine.name
~~~~~~~~~~~~~~~~~

Third-party engines are not reported, so only the following values are allowed: ``addons``, ``bookmarks``, ``clients``, ``forms``, ``history``, ``passwords``, ``prefs``, and ``tabs``.

syncs.engine.validation.problems
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For engines that can run validation on themselves, an array of objects describing validation errors that have occurred. Items that would have a count of 0 are excluded. Each engine will have its own set of items that it might put in the ``name`` field, but there are a finite number. See ``BookmarkProblemData.getSummary`` in `services/sync/modules/bookmark\_validator.js <https://dxr.mozilla.org/mozilla-central/source/services/sync/modules/bookmark_validator.js>`_ for an example.

syncs.devices
~~~~~~~~~~~~~

The list of remote devices associated with this account, as reported by the clients collection. The ID of each device is hashed using the same algorithm as the local id.

Events in the "sync" ping
-------------------------

The sync ping includes events in the same format as they are included in the
main ping, see :ref:`eventtelemetry`.

All events submitted as part of the sync ping which already include the "extra"
object (the 6th parameter of the event array described in the event telemetry
documentation) may also include a "serverTime" parameter, which the most recent
unix timestamp sent from the sync server (as a string). This arrives in the
``X-Weave-Timestamp`` HTTP header, and may be omitted in cases where the client
has not yet made a request to the server, or doesn't have it for any other
reason. It is included to improve flow analysis across multiple clients.

Every event recorded in this ping will have a category of ``sync``. The following
events are defined, categorized by the event method.

Histograms in the "sync" ping
-----------------------------

The sync ping includes histograms relating to measurements of password manager usage.
These histograms are duplicated in the main ping. Histograms are only included in a ping if they have been set by the pwmgr code.
Currently, the histograms that can be included are:

PWMGR_BLOCKLIST_NUM_SITES
PWMGR_FORM_AUTOFILL_RESULT
PWMGR_LOGIN_LAST_USED_DAYS
PWMGR_LOGIN_PAGE_SAFETY
PWMGR_NUM_PASSWORDS_PER_HOSTNAME
PWMGR_NUM_SAVED_PASSWORDS
PWMGR_PROMPT_REMEMBER_ACTION
PWMGR_PROMPT_UPDATE_ACTION
PWMGR_SAVING_ENABLED

Histograms are objects with the following 6 properties:
- min - minimum bucket size
- max - maximum bucket size
- histogram_type
- counts - array representing contents of the buckets in the histogram
- ranges - array with calculated bucket sizes

sendcommand
~~~~~~~~~~~

Records that Sync wrote a remote "command" to another client. These commands
cause that other client to take some action, such as resetting Sync on that
client, or opening a new URL.

- object: The specific command being written.
- value: Not used (ie, ``null``)
- extra: An object with the following attributes:

  - deviceID: A GUID which identifies the device the command is being sent to.
  - flowID: A GUID which uniquely identifies this command invocation. This GUID
            is the same for every device the tab is sent to.
  - streamID: A GUID which uniquely identifies this command invocation's
              specific target. This GUID is unique for every device the tab is
              sent to (new in Firefox 79).
  - serverTime: (optional) Most recent server timestamp, as described above.

processcommand
~~~~~~~~~~~~~~

Records that Sync processed a remote "command" previously sent by another
client. This is logically the "other end" of ``sendcommand``.

- object: The specific command being processed.
- value: Not used (ie, ``null``)
- extra: An object with the following attributes:

  - flowID: A GUID which uniquely identifies this command invocation. The value
            for this GUID will be the same as the flowID sent to the client via
            ``sendcommand``.
  - streamID: A GUID which uniquely identifies this command invocation's
              specific target. The value for this GUID will be the same as the
              streamID sent to the client via ``sendcommand`` (new in Firefox 79).
  - reason: A string value of either ``"poll"``, ``"push"``, or ``"push-missed"``
            representing an explanation for why the command is being processed.
  - serverTime: (optional) Most recent server timestamp, as described above.

The ``migrations`` Array
------------------------

The application-services developers are in the process of oxidizing parts of firefox sync and the related data storage code, which typically requires migrating the old storage into a new database and/or format.

When a migration like this occurs, a record is reported in this list the next time the sync ping is submitted.

Because the format of each data store may be drastically different, we are not attempting to come up with a generic representation here, and currently planning on allowing each migration record to vary independently (at least for now). These records will be distinctly identified by their ``"type"`` field.

They should only appear once per migration (that is, we'd rather fail to report a record than report them multiple times).

migrations.type: ``"webext-storage"``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This indicates a migration was performed from the legacy kinto-based extension-storage database into the new webext-storage rust implementation.

It contains the following fields:

- ``type``: Always the string ``"webext-storage"``.

- ``entries``: The number of entries/preferences in the source (legacy) database, including ones we failed to read. See below for information on the distinction between ``entries`` and ``extensions`` in this record.

- ``entriesSuccessful``: The number of entries/preferences (see below) which we have successfully migrated into the destination database..

- ``extensions``: The number of distinct extensions which have at least one preference in the source (legacy) database.

- ``extensionsSuccessful``: The number of distinct extensions which have at least one preference in the destination (migrated) database.

- ``openFailure``: A boolean flag that is true if we hit a read error prior to . This likely indicates complete corruption, or a bug in an underlying library like rusqlite.


Note: "entries" vs "extensions"
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``webext-storage`` migration record detailed above contains counts for both:

- The number of "entries" detected vs successfully migrated.
- The number of "extensions" detected vs successfully migrated.

This may seem redundant, but these refer to different (but related) things. The distinction here has to do with the way the two databases store extension-storage data:

* The legacy database stores one row for each (``extension_id``, ``preference_name``, ``preference_value``) triple. These are referred to as ``entries``.

* Conversely, the new database stores one row per extension, which is a pair containing both the ``extension_id``, as well as a dictionary holding all preference data, and so are equivalent to extensions.

(The description above is a somewhat simplified view of things, as it ignores a number values each database stores which is irrelevant for migration)

That is, ``entries`` represent each individual preference setting, and ``extensions`` represent the collected set of preferences for a given extension.

Counts for are *both* of these are present as it's likely that the disparity would point to different kinds of issues with the migration code.
