
"sync" ping
===========

This is an aggregated format that contains information about each sync that occurred during a timeframe. It is submitted every 12 hours, and on browser shutdown, but only if the syncs property would not be empty. The ping does not contain the enviroment block, nor the clientId.

Each item in the syncs property is generated after a sync is completed, for both successful and failed syncs, and contains measurements pertaining to sync performance and error information.

A JSON-schema document describing the exact format of the ping's payload property can be found at `services/sync/tests/unit/sync\_ping\_schema.json <https://dxr.mozilla.org/mozilla-central/source/services/sync/tests/unit/sync_ping_schema.json>`_.

Structure:

.. code-block:: js

    {
      version: 4,
      type: "sync",
      ... common ping data
      payload: {
        version: 1,
        discarded: <integer count> // Number of syncs discarded -- left out if zero.
        why: <string>, // Why did we submit the ping? Either "shutdown" or "schedule".
        // Array of recorded syncs. The ping is not submitted if this would be empty
        syncs: [{
          when: <integer milliseconds since epoch>,
          took: <integer duration in milliseconds>,
          uid: <string>, // Hashed FxA unique ID, or string of 32 zeros.
          deviceID: <string>, // Hashed FxA Device ID, hex string of 64 characters, not included if the user is not logged in.
          didLogin: <bool>, // Optional, is this the first sync after login? Excluded if we don't know.
          why: <string>, // Optional, why the sync occured, excluded if we don't know.

          // Optional, excluded if there was no error.
          failureReason: {
            name: <string>, // "httperror", "networkerror", "shutdownerror", etc.
            code: <integer>, // Only present for "httperror" and "networkerror".
            error: <string>, // Only present for "othererror" and "unexpectederror".
            from: <string>, // Optional, and only present for "autherror".
          },

          // Optional, excluded if we couldn't get a valid uid or local device id
          devices: [{
            os: <string>, // OS string as reported by Services.appinfo.OS,
            version: <string>, // Firefox version, as reported by Services.appinfo.version
            id: <string>, // Hashed FxA device id for device
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
          ]
        }]
      }
    }

info
----

discarded
~~~~~~~~~

The ping may only contain a certain number of entries in the ``"syncs"`` array, currently 500 (it is determined by the ``"services.sync.telemetry.maxPayloadCount"`` preference).  Entries beyond this are discarded, and recorded in the discarded count.

syncs.took
~~~~~~~~~~

These values should be monotonic.  If we can't get a monotonic timestamp, -1 will be reported on the payload, and the values will be omitted from the engines. Additionally, the value will be omitted from an engine if it would be 0 (either due to timer inaccuracy or finishing instantaneously).

syncs.uid
~~~~~~~~~

This property containing a hash of the FxA account identifier, which is a 32 character hexidecimal string.  In the case that we are unable to authenticate with FxA and have never authenticated in the past, it will be a placeholder string consisting of 32 repeated ``0`` characters.

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

- ``httperror``: Indicates that we recieved an HTTP error response code, but are unable to be more specific about the error. Contains the following properties:

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

- ``sqlerror``: Indicates that we recieved a ``mozIStorageError`` from a database query.

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
