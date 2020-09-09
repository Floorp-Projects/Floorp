.. _mach_telemetry:

==============
Mach Telemetry
==============

`Glean <https://mozilla.github.io/glean/>`_ is used to collect telemetry, and uses the metrics
defined in ``/python/mach/metrics.yaml``.
Associated generated documentation can be found in :ref:`the metrics document<metrics>`.

.. toctree::
   :maxdepth: 1

   metrics

Updating Generated Metrics Docs
===============================

When ``metrics.yaml`` is changed, :ref:`the metrics document<metrics>` will need to be updated.
Glean provides doc-generation tooling for us::

    glean_parser translate -f markdown -o python/mach/docs/ python/mach/metrics.yaml python/mach/pings.yaml
