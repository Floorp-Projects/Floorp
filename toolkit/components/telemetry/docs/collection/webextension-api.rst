.. _webextension-telemetry:

==============================
WebExtension API for Telemetry
==============================

Use the ``browser.telemetry`` API to send telemetry data to the Mozilla Telemetry service. Restricted to Mozilla privileged webextensions.

Types
-----

``ScalarType``
~~~~~~~~~~~~~~

Type of scalar: 'count' for numeric values, 'string' for string values, 'boolean' for boolean values. Maps to ``nsITelemetry.SCALAR_TYPE_*``.

``ScalarData``
~~~~~~~~~~~~~~

Represents registration data for a Telemetry scalar.

Properties:

* ``kind`` - See ScalarType_.
* ``keyed`` - *(optional, boolean)* True if this is a keyed scalar. Defaults to ``false``.
* ``record_on_release`` - *(optional, boolean)* True if this data should be recorded on release. Defaults to ``false``.
* ``expired`` - *(optional, boolean)* True if this scalar entry is expired. Operations on an expired scalar don't error (operations on an undefined scalar do), but the operations are no-ops. No data will be recorded. Defaults to ``false``.

``EventData``
~~~~~~~~~~~~~

Represents registration data for a Telemetry event.

Properties:

* ``methods`` - *(array)* List of methods for this event entry.
* ``objects`` - *(array)* List of objects for this event entry.
* ``extra_keys`` - *(array)* List of allowed extra keys for this event entry.
* ``record_on_release`` - *(optional, boolean)* True if this data should be recorded on release. Defaults to ``false``.
* ``expired`` - *(optional, boolean)* True if this event entry is expired. Recording an expired event doesn't error (operations on undefined events do). No data will be recorded. Defaults to ``false``.

Functions
---------

``submitPing``
~~~~~~~~~~~~~~

.. code-block:: js

  browser.telemetry.submitPing(type, message, options);

Submits a custom ping to the Telemetry backend. See :ref:`submitting-customping`.

* ``type`` - *(string)* The type of the ping.
* ``message`` - *(object)* The data payload for the ping.
* ``options`` - *(optional, object)* Options object.

  * ``addClientId`` - *(optional, boolean)* True if the ping should contain the client id. Defaults to ``false``.
  * ``addEnvironment`` - *(optional, boolean)* True if the ping should contain the environment data. Defaults to ``false``.
  * ``overrideEnvironment`` - *(optional, object)* Set to override the environment data. Default: not set.
  * ``usePingSender`` - *(optional, boolean)* If true, send the ping using the PingSender. Defaults to ``false``.


``canUpload``
~~~~~~~~~~~~~

.. code-block:: js

  browser.telemetry.canUpload();

Checks if Telemetry upload is enabled.

``scalarAdd``
~~~~~~~~~~~~~

.. code-block:: js

  browser.telemetry.scalarAdd(name, value);

Adds the value to the given scalar.

* ``name`` - *(string)* The scalar name.
* ``value`` - *(integer)* The numeric value to add to the scalar. Only unsigned integers supported.

``scalarSet``
~~~~~~~~~~~~~

.. code-block:: js

  browser.telemetry.scalarSet(name, value);

Sets the named scalar to the given value. Throws if the value type doesn't match the scalar type.

* ``name`` - *(string)* The scalar name.
* ``value`` - *(string|boolean|integer|object)* The value to set the scalar to.

``scalarSetMaximum``
~~~~~~~~~~~~~~~~~~~~

.. code-block:: js

  browser.telemetry.scalarSetMaximum(name, value);

Sets the scalar to the maximum of the current and the passed value

* ``name`` - *(string)* The scalar name.
* ``value`` - *(integer)* The numeric value to set the scalar to. Only unsigned integers supported.

``recordEvent``
~~~~~~~~~~~~~~~

.. code-block:: js

  browser.telemetry.recordEvent(category, method, object, value, extra);

Record an event in Telemetry. Throws when trying to record an unknown event.

* ``category`` - *(string)* The category name.
* ``method`` - *(string)* The method name.
* ``object`` - *(string)* The object name.
* ``value`` - *(optional, string)* An optional string value to record.
* ``extra`` - *(optional, object)* An optional object of the form (string -> string). It should only contain registered extra keys.

``registerScalars``
~~~~~~~~~~~~~~~~~~~

.. code-block:: js

  browser.telemetry.registerScalars(category, data);

Register new scalars to record them from addons. See :ref:`registerscalars` for more details.

* ``category`` - *(string)* The unique category the scalars are registered in.
* ``data`` - *(object)* An object that contains registration data for multiple scalars. Each property name is the scalar name, and the corresponding property value is an object of ScalarData_ type.

``registerEvents``
~~~~~~~~~~~~~~~~~~

.. code-block:: js

  browser.telemetry.registerEvents(category, data);

Register new events to record them from addons. See :ref:`registerevents` for more details.

* ``category`` - *(string)* The unique category the events are registered in.
* ``data`` - *(object)* An object that contains registration data for 1+ events. Each property name is the category name, and the corresponding property value is an object of EventData_ type.

``setEventRecordingEnabled``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: js

  browser.telemetry.setEventRecordingEnabled(category, enabled);

Enable recording of events in a category. Events default to recording disabled. This allows to toggle recording for all events in the specified category.

* ``category`` - *(string)* The category name.
* ``enabled`` - *(boolean)* Whether recording is enabled for events in that category.
