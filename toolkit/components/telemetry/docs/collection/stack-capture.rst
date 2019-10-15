=============
Stack capture
=============

While studying behavior of Firefox in the wild it is sometimes useful to inspect
call stacks without causing the browser to crash. Historically we could only
obtain stacks for inspection from crash reports. Now stacks can be captured on
demand and annotated with a unique key for further inspection.

Capturing stacks is only supported on official builds with ``--enable-profiling``
switch enabled, such as Nightly builds, for example. The feature is available on
Windows, Linux and macOS builds of Firefox.

Captured stacks are grouped by a user-defined key. Identical stacks captured under
the same key are combined in order to reduce their memory footprint. A counter is
used to reflect the frequency of identical stacks.

The serialized stack data is submitted with the :doc:`main pings <../data/main-ping>`.

The API
=======
Capturing stacks is available either via the `nsITelemetry interface <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/telemetry/nsITelemetry.idl>`_
or the `C++ API <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/telemetry/Telemetry.h>`_.
Note that current implementation of the API is not thread safe. Also, capturing
stacks in the content process is not supported yet.

JS API
------
Privileged JavaScript code can capture stacks using the following function:

.. code-block:: js

  Services.telemetry.captureStack(aKey);

``captureStack(aKey)`` instructs Telemetry to take a snapshot of the current
call stack and store it under the given key (``aKey``). The actual stack capturing
will only take place once for each key. Every consequent calls for the identical
key will increase stack frequency counter without performing expensive stack
walking.

``aKey`` is a string used primarily as an identifier for a captured stack. It can
be used to identify stacks down the data processing pipeline and on ``about:telemetry``.

For more technical information please refer to the `code documentation <https://dxr.mozilla.org/mozilla-central/search?q=regexp%3ATelemetryScalar%3A%3A(Set%7CAdd)+file%3ATelemetryScalar.cpp&redirect=false>`_ .

C++ API
-------
Capturing stacks in native code can be achieved by calling:

.. code-block:: cpp

    void CaptureStack(const nsCString& aKey);

The behavior of ``CaptureStack`` is identical to its JavaScript counterpart.

Limits and restrictions
-----------------------
Keys used for capturing stacks are meant to be unique among API users. Detection
of collisions, however, is left to the users themselves. In its current implementation
the API does not provide any means for key registration.

To prevent abuses, the content of a key is limited to 50 characters in length.
Additionally, keys may only contain alphanumeric characters or ``-``.

Both the depth of the captured stacks and the total number of keys in the
dictionary are limited to ``50``.
