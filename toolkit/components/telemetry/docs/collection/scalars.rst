=======
Scalars
=======

Historically we started to overload our histogram mechanism to also collect scalar data,
such as flag values, counts, labels and others.
The scalar measurement types are the suggested way to collect that kind of scalar data.
We currently only support recording of scalars from the parent process.
The serialized scalar data is submitted with the :doc:`main pings <../data/main-ping>`.

The API
=======
Scalar probes can be managed either through the `nsITelemetry interface <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/telemetry/nsITelemetry.idl>`_
or the `C++ API <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/telemetry/Telemetry.h>`_.

JS API
------
Probes in privileged JavaScript code can use the following functions to manipulate scalars:

.. code-block:: js

  Services.telemetry.scalarAdd(aName, aValue);
  Services.telemetry.scalarSet(aName, aValue);
  Services.telemetry.scalarSetMaximum(aName, aValue);

These functions can throw if, for example, an operation is performed on a scalar type that doesn't support it
(e.g. calling scalarSetMaximum on a scalar of the string kind). Please look at the code documentation for
additional informations.

C++ API
-------
Probes in native code can use the more convenient helper functions declared in `Telemetry.h <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/telemetry/Telemetry.h>`_:

.. code-block:: cpp

    void ScalarAdd(mozilla::Telemetry::ScalarID aId, uint32_t aValue);
    void ScalarSet(mozilla::Telemetry::ScalarID aId, uint32_t aValue);
    void ScalarSet(mozilla::Telemetry::ScalarID aId, const nsAString& aValue);
    void ScalarSet(mozilla::Telemetry::ScalarID aId, bool aValue);
    void ScalarSetMaximum(mozilla::Telemetry::ScalarID aId, uint32_t aValue);

The YAML definition file
========================
Scalar probes are required to be registered, both for validation and transparency reasons,
in the `Scalars.yaml <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/telemetry/Scalars.yaml>`_
definition file.

The probes in the definition file are represented in a fixed-depth, two-level structure:

.. code-block:: yaml

    # The following is a group.
    a.group.hierarchy:
      a_probe_name:
        kind: uint
        ...
      another_probe:
        kind: string
        ...
      ...
    group2:
      probe:
        kind: int
        ...

Group and probe names need to follow a few rules:

- they cannot exceed 40 characters each;
- group names must be alpha-numeric + ``.``, with no leading/trailing digit or ``.``;
- probe names must be alpha-numeric + ``_``, with no leading/trailing digit or ``_``.

A probe can be defined as follows:

.. code-block:: yaml

    a.group.hierarchy:
      a_scalar:
        bug_numbers:
          - 1276190
        description: A nice one-line description.
        expires: never
        kind: uint
        notification_emails:
          - telemetry-client-dev@mozilla.com

Required Fields
---------------

- ``bug_numbers``: A list of unsigned integers representing the number of the bugs the probe was introduced in.
- ``description``: A single or multi-line string describing what data the probe collects and when it gets collected.
- ``expires``: The version number in which the scalar expires, e.g. "30"; a version number of type "N" and "N.0" is automatically converted to "N.0a1" in order to expire the scalar also in the development channels. A telemetry probe acting on an expired scalar will print a warning into the browser console. For scalars that never expire the value ``never`` can be used.
- ``kind``: A string representing the scalar type. Allowed values are ``uint``, ``string`` and ``boolean``.
- ``notification_emails``: A list of email addresses to notify with alerts of expiring probes. More importantly, these are used by the data steward to verify that the probe is still useful.

Optional Fields
---------------

- ``cpp_guard``: A string that gets inserted as an ``#ifdef`` directive around the automatically generated C++ declaration. This is typically used for platform-specific scalars, e.g. ``ANDROID``.
- ``release_channel_collection``: This can be either ``opt-in`` (default) or ``opt-out``. With the former the scalar is submitted by default on pre-release channels; on the release channel only if the user opted into additional data collection. With the latter the scalar is submitted by default on release and pre-release channels, unless the user opted out.

String type restrictions
------------------------
To prevent abuses, the content of a string scalar is limited to 50 characters in length. Trying
to set a longer string will result in an error and no string being set.

The processor scripts
=====================
The scalar definition file is processed and checked for correctness at compile time. If it
conforms to the specification, the processor scripts generate two C++ headers files, included
by the Telemetry C++ core.

gen-scalar-data.py
------------------
This script is called by the build system to generate the ``TelemetryScalarData.h`` C++ header
file out of the scalar definitions.
This header file contains an array holding the scalar names and version strings, in addition
to an array of ``ScalarInfo`` structures representing all the scalars.

gen-scalar-enum.py
------------------
This script is called by the build system to generate the ``TelemetryScalarEnums.h`` C++ header
file out of the scalar definitions.
This header file contains an enum class with all the scalar identifiers used to access them
from code through the C++ API.
