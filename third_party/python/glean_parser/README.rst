============
Glean Parser
============

Parser tools for Mozilla's Glean telemetry.

Features
--------

Parses the ``metrics.yaml`` files for the Glean telemetry SDK and produces
output for various integrations.

Documentation
-------------

The full documentation is available `here <https://mozilla.github.io/glean_parser/>`__.

Requirements
------------

- Python 3.5 (or later)

The following library requirements are installed automatically when glean_parser
is installed by `pip`.

- appdirs
- Click
- diskcache
- Jinja2
- jsonschema
- PyYAML

Additionally on Python 3.6 and 3.5:

- iso8601

And on Python 3.5:

- pep487

Usage
-----

.. code-block:: console

  $ glean_parser --help

Read in `metrics.yaml`, translate to kotlin format, and output to `output_dir`:

.. code-block:: console

  $ glean_parser translate -o output_dir -f kotlin metrics.yaml

Check a Glean ping against the ping schema:

.. code-block:: console

  $ glean_parser check < ping.json
