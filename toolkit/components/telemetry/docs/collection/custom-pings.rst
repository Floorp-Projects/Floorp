=======================
Submitting custom pings
=======================

Custom pings can be submitted from JavaScript using:

.. code-block:: js

    TelemetryController.submitExternalPing(type, payload, options)

- ``type`` - a ``string`` that is the type of the ping, limited to ``/^[a-z0-9][a-z0-9-]+[a-z0-9]$/i``.
- ``payload`` - the actual payload data for the ping, should be a JSON style object.
- ``options`` - optional, an object containing additional options:
   - ``addClientId``- whether to add the client id to the ping, defaults to ``false``
   - ``addEnvironment`` - whether to add the environment data to the ping, defaults to ``false``
   - ``overrideEnvironment`` - a JSON style object that overrides the environment data

``TelemetryController`` will assemble a ping with the passed payload and the specified options.
That ping will be archived locally for use with Shield and inspection in ``about:telemetry``.
If the preferences allow upload of Telemetry pings, the ping will be uploaded at the next opportunity (this is subject to throttling, retry-on-failure, etc.).

Tools
=====

Helpful tools for designing new pings include:

- `gzipServer <https://github.com/mozilla/gzipServer>`_ - a Python script that can run locally and receives and saves Telemetry pings. Making Firefox send to it allows inspecting outgoing pings easily.
- ``about:telemetry`` - allows inspecting submitted pings from the local archive, including all custom ones.

Designing custom pings
======================

In general, creating a new custom ping means you don't benefit automatically from the existing tooling. Further work is needed to make data show up in re:dash or other analysis tools.

In addition to the `data collection review <https://wiki.mozilla.org/Firefox/Data_Collection>`_, questions to guide a new pings design are:

- Submission interval & triggers:
   - What events trigger ping submission?
   - What interval is the ping submitted in?
   - Is there a throttling mechanism?
   - What is the desired latency? (submitting "at least daily" still leads to certain latency tails)
   - Are pings submitted on a clock schedule? Or based on "time since session start", "time since last ping" etc.? (I.e. will we get sharp spikes in submission volume?)
- Size and volume:
   - What’s the size of the submitted payload?
   - What's the full ping size including metadata in the pipeline?
   - What’s the target population?
   - What's the overall estimated volume?
- Dataset:
   - Is it opt-out?
   - Does it need to be opt-out?
   - Does it need to be in a separate ping? (why can’t the data live in probes?)
- Privacy:
   - Is there risk to leak PII?
   - How is that risk mitigated?
- Data contents:
   - Does the submitted data answer the posed product questions?
   - Does the shape of the data allow to answer the questions efficiently?
   - Is the data limited to whats needed to answer the questions?
   - Does the data use common formats? (i.e. can we re-use tooling or analysis know-how)
