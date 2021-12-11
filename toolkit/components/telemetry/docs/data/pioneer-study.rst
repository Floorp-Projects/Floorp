=============
Pioneer Study
=============

The `pioneer-study` ping is the main transport used by the Pioneer components.

-------
Payload
-------

It is made up of a clear text payload and an encrypted data payload, following the structure described below.

Structure:

.. code-block:: js

  "payload": {
    "encryptedData": "<encrypted token>",
    "schemaVersion": 1,
    "schemaName": "debug",
    "schemaNamespace": "<namespace>",
    "encryptionKeyId": "<key id>",
    "pioneerId": "<UUID>",
    "studyName": "pioneer-v2-example"
  }

See also the `JSON schemas <https://github.com/mozilla-services/mozilla-pipeline-schemas/tree/master/schemas/pioneer-debug>`_.

encryptedData
  The encrypted data sent using the Pioneer platform.

schemaVersion
  The payload format version.

schemaName
  The name of the schema of the encrypted data.

schemaNamespace
  The namespace used to segregate data on the pipeline.

encryptionKeyId
  The id of the key used to encrypt the data. If `discarded` is used, then the `encryptedData` will be ignored and not decoded (only possible for `deletion-request` and `pioneer-enrollment` schemas).

pioneerId
  The id of the pioneer client.

studyName (optional)
  The id of the study for which the data is being collected.

------------------------
Special Pioneer Payloads
------------------------

This ping has two special Pioneer payload configurations, indicated by the different `schemaName`: `deletion-request` and `pioneer-enrollemnt`.

The `deletion-request` is sent when a user opts out from a Pioneer study: it contains the `pioneerId` and the `studyName`.

The `pioneer-enrollment` is sent when a user opts into the Pioneer program: in this case it reports `schemaNamespace: "pioneer-meta"` and will have no `studyName`. It is also sent when enrolling into a study, in which case it reports the same namespace as the `deletion-request` (i.e. the id the study making the request) and the `pioneer-enrollment` schema name.
