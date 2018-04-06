=======
Scalars
=======

Historically we started to overload our histogram mechanism to also collect scalar data,
such as flag values, counts, labels and others.
The scalar measurement types are the suggested way to collect that kind of scalar data.
The serialized scalar data is submitted with the :doc:`main pings <../data/main-ping>`. Adding scalars is supported in artifact builds and build faster workflows.

.. important::

    Every new data collection in Firefox needs a `data collection review <https://wiki.mozilla.org/Firefox/Data_Collection#Requesting_Approval>`_ from a data collection peer. Just set the feedback? flag for one of the data peers. We try to reply within a business day.

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

  Services.telemetry.keyedScalarAdd(aName, aKey, aValue);
  Services.telemetry.keyedScalarSet(aName, aKey, aValue);
  Services.telemetry.keyedScalarSetMaximum(aName, aKey, aValue);

These functions can throw if, for example, an operation is performed on a scalar type that doesn't support it
(e.g. calling scalarSetMaximum on a scalar of the string kind). Please look at the `code documentation <https://dxr.mozilla.org/mozilla-central/search?q=regexp%3ATelemetryScalar%3A%3A(Set%7CAdd)+file%3ATelemetryScalar.cpp&redirect=false>`_ for
additional information.

``registerScalars()``
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: js

  Services.telemetry.registerScalars(category, scalarData);

Register new scalars from add-ons.

* ``category`` - *(required, string)* The unique category the scalars are registered in (see :ref:`limitations <scalar-limitations>`).
* ``scalarData`` - *(required, object)* An object of the form ``{scalarName1: scalar1Data, ...}`` that contains registration data for multiple scalars; ``scalarName1`` is subject to :ref:`limitations <scalar-limitations>`; each scalar is an object with the following properties:

  * ``kind`` - *(required, uint)*  One of the scalar types (nsITelemetry::SCALAR_TYPE_*).
  * ``keyed`` - *(optional, bool)* Whether this is a keyed scalar or not. Defaults to false.
  * ``record_on_release`` - *(optional, bool)* Whether to record this data on release. Defaults to false.
  * ``expired`` - *(optional, bool)* Whether this scalar entry is expired. This allows recording it without error, but it will be discarded. Defaults to false.

For scalars recorded from add-ons, registration happens at runtime. Any new scalar must first be registered through this function before they can be recorded.

After registration, the scalars can be recorded through the usual scalar JS API. If the accumulation happens in a content process right after the registration and the definition still has to reach this process, it will be discarded: one way to work around the problem is to send an IPC message to the content process and start accumulating data once this message has been received. The accumulated data will be submitted in the main pings payload under ``processes.dynamic.scalars``.

.. note::

    Accumulating in dynamic scalars only works in content child processes and in the parent process. All the accumulations (parent and content chldren) are aggregated together .

New scalars registered here are subject to the same :ref:`limitations <scalar-limitations>` as the ones registered through ``Scalars.yaml``, e.g. the length of the category name or the allowed characters.

When add-ons are updated, they may re-register all of their scalars. In that case, any changes to scalars that are already registered are ignored. The only exception is expiry; a scalar that is re-registered with ``expired: true`` will not be recorded anymore.

Example:

.. code-block:: js

  Services.telemetry.registerScalars("myAddon.category", {
    "counter_scalar": {
      kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
      keyed: false,
      record_on_release: false
    },
  });
  // Now scalars can be recorded.
  Services.telemetry.scalarSet("myAddon.category.counter_scalar", 37);

C++ API
-------
Probes in native code can use the more convenient helper functions declared in `Telemetry.h <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/telemetry/Telemetry.h>`_:

.. code-block:: cpp

    void ScalarAdd(mozilla::Telemetry::ScalarID aId, uint32_t aValue);
    void ScalarSet(mozilla::Telemetry::ScalarID aId, uint32_t aValue);
    void ScalarSet(mozilla::Telemetry::ScalarID aId, const nsAString& aValue);
    void ScalarSet(mozilla::Telemetry::ScalarID aId, bool aValue);
    void ScalarSetMaximum(mozilla::Telemetry::ScalarID aId, uint32_t aValue);

    void ScalarAdd(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, uint32_t aValue);
    void ScalarSet(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, uint32_t aValue);
    void ScalarSet(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, bool aValue);
    void ScalarSetMaximum(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, uint32_t aValue);

.. warning::

  Scalar operations are designed to be cheap, not free. If you wish to manipulate Scalars in a performance-sensitive piece of code, store the operations locally and change the Scalar only after the performance-sensitive piece ("hot path") has completed.

The YAML definition file
========================
Scalar probes are required to be registered, both for validation and transparency reasons,
in the `Scalars.yaml <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/telemetry/Scalars.yaml>`_
definition file.

The probes in the definition file are represented in a fixed-depth, two-level structure:

.. code-block:: yaml

    # The following is a category.
    a.category.hierarchy:
      a_probe_name:
        kind: uint
        ...
      another_probe:
        kind: string
        ...
      ...
    category2:
      probe:
        kind: int
        ...

.. _scalar-limitations:

Category and probe names need to follow a few rules:

- they cannot exceed 40 characters each;
- category names must be alpha-numeric + ``.``, with no leading/trailing digit or ``.``;
- probe names must be alpha-numeric + ``_``, with no leading/trailing digit or ``_``.

A probe can be defined as follows:

.. code-block:: yaml

    a.category.hierarchy:
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
- ``expires``: The version number in which the scalar expires, e.g. "30"; a version number of type "N" is automatically converted to "N.0a1" in order to expire the scalar also in the development channels. A telemetry probe acting on an expired scalar will print a warning into the browser console. For scalars that never expire the value ``never`` can be used.
- ``kind``: A string representing the scalar type. Allowed values are ``uint``, ``string`` and ``boolean``.
- ``notification_emails``: A list of email addresses to notify with alerts of expiring probes. More importantly, these are used by the data steward to verify that the probe is still useful.
- ``record_in_processes``: A list of processes the scalar is allowed to record in. Currently supported values are:

  - ``main``;
  - ``content``;
  - ``gpu``;
  - ``all_children`` (record in all the child processes);
  - ``all`` (record in all the processes).

Optional Fields
---------------

- ``cpp_guard``: A string that gets inserted as an ``#ifdef`` directive around the automatically generated C++ declaration. This is typically used for platform-specific scalars, e.g. ``ANDROID``.
- ``release_channel_collection``: This can be either ``opt-in`` (default) or ``opt-out``. With the former the scalar is submitted by default on pre-release channels, unless the user has opted out. With the latter the scalar is submitted by default on release and pre-release channels, unless the user has opted out.
- ``keyed``: A boolean that determines whether this is a keyed scalar. It defaults to ``False``.

String type restrictions
------------------------
To prevent abuses, the content of a string scalar is limited to 50 characters in length. Trying
to set a longer string will result in an error and no string being set.

Keyed Scalars
-------------
Keyed scalars are collections of one of the available scalar types, indexed by a string key that can contain UTF8 characters and cannot be longer than 72 characters. Keyed scalars can contain up to 100 keys. This scalar type is for example useful when you want to break down certain counts by a name, like how often searches happen with which search engine.

Keyed scalars should only be used if the set of keys are not known beforehand. If the keys are from a known set of strings, other options are preferred if suitable, like categorical histograms or splitting measurements up into separate scalars.

Multiple processes caveats
--------------------------
When recording data in different processes of the same type (e.g. multiple content processes), the user is responsible for preventing races between the operations on the scalars.
Races can happen because scalar changes are sent from each child process to the parent process, and then merged into the final storage location. Since there's no synchronization between the processes, operations like ``setMaximum`` can potentially produce different results if sent from more than one child process.

The processor scripts
=====================
The scalar definition file is processed and checked for correctness at compile time. If it
conforms to the specification, the processor scripts generate two C++ headers files, included
by the Telemetry C++ core.

gen_scalar_data.py
------------------
This script is called by the build system to generate the ``TelemetryScalarData.h`` C++ header
file out of the scalar definitions.
This header file contains an array holding the scalar names and version strings, in addition
to an array of ``ScalarInfo`` structures representing all the scalars.

gen_scalar_enum.py
------------------
This script is called by the build system to generate the ``TelemetryScalarEnums.h`` C++ header
file out of the scalar definitions.
This header file contains an enum class with all the scalar identifiers used to access them
from code through the C++ API.

Adding a new probe
==================
Making a scalar measurement is a two step process:

1. add the probe definition to the scalar registry;
2. record into the scalar using the API.

Registering the scalar
----------------------
Let's start by registering two probes in the `Scalars.yaml <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/telemetry/Scalars.yaml>`_ definition file: a simple boolean scalar and a keyed unsigned scalar.

.. code-block:: yaml

    # The following section contains the demo scalars.
    profile:
      was_reset:
        bug_numbers:
          - 1301364
        description: True if the profile was reset.
        expires: "60"
        kind: boolean
        notification_emails:
          - change-me@allizom.com
        release_channel_collection: opt-out
        record_in_processes:
          - 'main'

    ui:
      download_button_activated:
        bug_numbers:
          - 1301364
        description: >
          The number of times the download button was activated, per
          input type (e.g. 'mouse_click', 'touchscreen', ...).
        expires: "60"
        kind: uint
        keyed: true
        notification_emails:
          - change-me@allizom.com
        release_channel_collection: opt-in
        record_in_processes:
          - 'main'

These two scalars have different collection policies and are both constrained to recording only in the main process.
For example, the ``ui.download_button_activated`` can be recorded only by users on running pre-release builds of Firefox.

Using the JS API
----------------
Changing the demo scalars from privileged JavaScript code is straightforward:

.. code-block:: js

  // Set the scalar value: trying to use a non-boolean value doesn't throw
  // but rather prints a warning to the browser console
  Services.telemetry.scalarSet("profile.was_reset", true);

  // This call increments the value stored in "mouse_click" within the
  // "ui.download_button_activated" scalar, by 1.
  Services.telemetry.keyedScalarAdd("ui.download_button_activated", "mouse_click", 1);

More usage examples can be found in the tests covering the `JS Scalars API <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/telemetry/tests/unit/test_TelemetryScalars.js>`_ and `child processes scalars <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/telemetry/tests/unit/test_ChildScalars.js>`_.

Using the C++ API
-----------------
Native code can take advantage of Scalars as well, by including the ``Telemetry.h`` header file.

.. code-block:: cpp

    Telemetry::ScalarSet(Telemetry::ScalarID::PROFILE_WAS_RESET, false);

    Telemetry::ScalarAdd(Telemetry::ScalarID::UI_DOWNLOAD_BUTTON_ACTIVATED,
                         NS_LITERAL_STRING("touchscreen"), 1);

The ``ScalarID`` enum is automatically generated by the build process, with an example being available `here <https://dxr.mozilla.org/mozilla-central/search?q=path%3ATelemetryScalarEnums.h&redirect=false>`_ .

Other examples can be found in the `test coverage <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/telemetry/tests/gtest/TestScalars.cpp>`_ for the scalars C++ API.

Version History
===============

- Firefox 50: Initial scalar support (`bug 1276195 <https://bugzilla.mozilla.org/show_bug.cgi?id=1276195>`_).
- Firefox 51: Added keyed scalars (`bug 1277806 <https://bugzilla.mozilla.org/show_bug.cgi?id=1277806>`_).
- Firefox 53: Added child process scalars (`bug 1278556 <https://bugzilla.mozilla.org/show_bug.cgi?id=1278556>`_).
- Firefox 58

  - Added support for recording new scalars from add-ons (`bug 1393801 <bug https://bugzilla.mozilla.org/show_bug.cgi?id=1393801>`_).
  - Ignore re-registering existing scalars for a category instead of failing (`bug 1409323 <https://bugzilla.mozilla.org/show_bug.cgi?id=1409323>`_).

- Firefox 60: Enabled support for adding scalars in artifact builds and build-faster workflows (`bug 1425909 <https://bugzilla.mozilla.org/show_bug.cgi?id=1425909>`_).
