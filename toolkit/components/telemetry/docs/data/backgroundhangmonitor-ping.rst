
"backgroundhangmonitor" ping
==========

Whenever a thread monitored by the Background Hang Monitor hangs, a stack and
some non-identifying information about the hang is captured. When 50 of these
hangs are collected across all processes, or the browser exits, this ping is
transmitted with the collected hang information.

This ping is only collected in nightly builds, to avoid the high volume of pings
which would be produced in Beta.

Structure:

.. code-block:: js

    {
      "type": "backgroundhangmonitor",
      ... // common ping data
      "environment": { ... },
      "payload": {
        "modules": [
          [
            <string>, // Name of the file holding the debug information.
            <string> // Breakpad ID of this module.
          ],
          ...
        ],
        "hangs": [
          {
            "duration": <number>, // duration of the hang in milliseconds.
            "thread": <string>, // name of the hanging thread.
            "runnableName": <string>, // name of the runnable executing during the hang.
                                      // Runnable names are only collected for the XPCOM main thread.
            "process": <string>, // Type of process that hung, see below for a list of types.
            "annotations": { ... }, // A set of annotations on the hang, see below.
            "pseudoStack": [ ... ], // List of pseudostack frame names.
            "stack": [ ... ], // interleaved hang stack, see below.
          },
          ...
        ]
      }
    }

.. note: :

  Hangs are collected whenever the current runnable takes over 128ms.

Process Types
-------------

The ``process`` field is a string denoting the kind of process that hung. Hangs
are currently sent only for the processes below:

+---------------+---------------------------------------------------+
| Kind          | Description                                       |
+===============+===================================================+
| default       | Main process, also known as the browser process   |
+---------------+---------------------------------------------------+
| tab           | Content process                                   |
+---------------+---------------------------------------------------+
| gpu           | GPU process                                       |
+---------------+---------------------------------------------------+

Stack Traces
------------

Each hang object contains a ``stack`` field which has been populated with an
interleaved stack trace of the hung thread. An interleaved stack consists of a
native backtrace with additional frames interleaved, representing chrome JS and
pseudostack entries.

Note that this field only contains native stack frames, pseudostack and chrome
JS script frames. If native stacks can not be collected on the target platform,
or stackwalking was not initialized, there will be no native frames present, and
the stack will consist only of pseudostack and chrome JS script frames.

A single frame in the stack is either a raw string, representing a pseudostack
or chrome JS script frame, or a native stack frame:

.. code-block:: js

    [
      <number>, // Index in the payload.modules list of the module description.
                // -1 if this frame was not in a valid module.
      <string> // Hex string (e.g. "FF0F") of the frame offset in the module.
    ]

Annotations
-----------

The annotations field is a map from key to string value, for example if the user
was interacting during a hang the annotations field would look something like
this:

.. code-block:: js

    {
        "UserInteracting": "true"
    }

The following annotations are currently present in tree:

+-----------------+-------------------------------------------------+
| Name            | Description                                     |
+=================+=================================================+
| UserInteracting | "true" if the user was actively interacting     |
+-----------------+-------------------------------------------------+
| pluginName      | Name of the currently running plugin            |
+-----------------+-------------------------------------------------+
| pluginVersion   | Version of the currently running plugin         |
+-----------------+-------------------------------------------------+
| HangUIShown     | "true" if the hang UI was shown                 |
+-----------------+-------------------------------------------------+
| HangUIContinued | "true" if continue was selected in the hang UI  |
+-----------------+-------------------------------------------------+
| HangUIDontShow  | "true" if the hang UI was not shown             |
+-----------------+-------------------------------------------------+
