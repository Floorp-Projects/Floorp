=====================
Experiment Annotation
=====================
This API allows privileged JavaScript to annotate the :doc:`../data/environment` with any experiments a client is participating in.

The experiment annotations are sent with any ping that includes the :doc:`../data/environment` data.

The JS API
==========
Privileged JavaScript code can annotate experiments using the functions exposed by ``TelemetryEnvironment.jsm``.

The following function adds an annotation to the environment for the provided ``id``, ``branch`` and ``options``. Calling this function repeatedly with the same ``id`` will overwrite the state and trigger new subsessions (subject to throttling).
``options`` is an object that currently may contain ``type``, to tag the experiment with a specific type.

.. code-block:: js

    TelemetryEnvironment.setExperimentActive(id, branch, [options={}}])

This removes the annotation for the experiment with the provided ``id``.

.. code-block:: js

    TelemetryEnvironment.setExperimentInactive(id)

This synchronously returns a dictionary containing the information for each active experiment.

.. code-block:: js

    TelemetryEnvironment.getActiveExperiments()

.. note::

    Both ``setExperimentActive`` and ``setExperimentInactive`` trigger a new subsession. However
    the latter only does so if there was an active experiment with the provided ``id``.

Limits and restrictions
-----------------------
To prevent abuses, the content of both the experiment ``id`` and ``branch`` is limited to
100 characters in length.
``type`` is limited to a length of 20 characters.
