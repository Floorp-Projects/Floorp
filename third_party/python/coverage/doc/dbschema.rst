.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

.. _dbschema:

===========================
Coverage.py database schema
===========================

.. versionadded:: 5.0

Coverage.py stores data in a SQLite database, by default called ``.coverage``.
For most needs, the :class:`.CoverageData` API will be sufficient, and should
be preferred to accessing the database directly.  Only advanced uses will need
to use the database.

The schema can change without changing the major version of coverage.py, so be
careful when accessing the database directly.  The `coverage_schema` table has
the schema number of the database.  The schema described here corresponds to:

.. copied_from: coverage/sqldata.py

.. code::

    SCHEMA_VERSION = 7

.. end_copied_from

You can use SQLite tools such as the :mod:`sqlite3 <python:sqlite3>` module in
the Python standard library to access the data.  Some data is stored in a
packed format that will need custom functions to access.  See
:func:`.register_sqlite_functions`.


Database schema
---------------

This is the database schema.

TODO: explain more. Readers: what needs explaining?

.. copied_from: coverage/sqldata.py

.. code::

    CREATE TABLE coverage_schema (
        -- One row, to record the version of the schema in this db.
        version integer
    );

    CREATE TABLE meta (
        -- Key-value pairs, to record metadata about the data
        key text,
        value text,
        unique (key)
        -- Keys:
        --  'has_arcs' boolean      -- Is this data recording branches?
        --  'sys_argv' text         -- The coverage command line that recorded the data.
        --  'version' text          -- The version of coverage.py that made the file.
        --  'when' text             -- Datetime when the file was created.
    );

    CREATE TABLE file (
        -- A row per file measured.
        id integer primary key,
        path text,
        unique (path)
    );

    CREATE TABLE context (
        -- A row per context measured.
        id integer primary key,
        context text,
        unique (context)
    );

    CREATE TABLE line_bits (
        -- If recording lines, a row per context per file executed.
        -- All of the line numbers for that file/context are in one numbits.
        file_id integer,            -- foreign key to `file`.
        context_id integer,         -- foreign key to `context`.
        numbits blob,               -- see the numbits functions in coverage.numbits
        foreign key (file_id) references file (id),
        foreign key (context_id) references context (id),
        unique (file_id, context_id)
    );

    CREATE TABLE arc (
        -- If recording branches, a row per context per from/to line transition executed.
        file_id integer,            -- foreign key to `file`.
        context_id integer,         -- foreign key to `context`.
        fromno integer,             -- line number jumped from.
        tono integer,               -- line number jumped to.
        foreign key (file_id) references file (id),
        foreign key (context_id) references context (id),
        unique (file_id, context_id, fromno, tono)
    );

    CREATE TABLE tracer (
        -- A row per file indicating the tracer used for that file.
        file_id integer primary key,
        tracer text,
        foreign key (file_id) references file (id)
    );

.. end_copied_from


.. _numbits:

Numbits
-------

.. automodule:: coverage.numbits
    :members:
