.. _assert-module:

Assert module
=============

For XPCShell tests and mochitests, ``Assert`` is already present as an
instantiated global to which you can refer - you don't need to construct it
yourself. You can immediately start using ``Assert.ok`` and similar methods as
test assertions.

The full class documentation follows, but it is perhaps worth noting that this
API is largely identical to
`NodeJS' assert module <https://nodejs.org/api/assert.html>`_, with some
omissions/changes including strict mode and string matching.

.. js:autoclass:: Assert
  :members: ok, equal, notEqual, strictEqual, notStrictEqual, deepEqual, notDeepEqual,
            greater, less, greaterOrEqual, lessOrEqual, stringContains, stringMatches,
            throws, rejects, *
