v3.2.0
------

* Added a ``format_nongpl`` setuptools extra, which installs only ``format``
  dependencies that are non-GPL (#619).

v3.1.1
------

* Temporarily revert the switch to ``js-regex`` until #611 and #612 are
  resolved.

v3.1.0
------

* Regular expressions throughout schemas now respect the ECMA 262 dialect, as
  recommended by the specification (#609).

v3.0.2
------

* Fixed a bug where ``0`` and ``False`` were considered equal by
  ``const`` and ``enum`` (#575).

v3.0.1
------

* Fixed a bug where extending validators did not preserve their notion
  of which validator property contains ``$id`` information.

v3.0.0
------

* Support for Draft 6 and Draft 7
* Draft 7 is now the default
* New ``TypeChecker`` object for more complex type definitions (and overrides)
* Falling back to isodate for the date-time format checker is no longer
  attempted, in accordance with the specification

v2.6.0
------

* Support for Python 2.6 has been dropped.
* Improve a few error messages for ``uniqueItems`` (#224) and
  ``additionalProperties`` (#317)
* Fixed an issue with ``ErrorTree``'s handling of multiple errors (#288) 

v2.5.0
------

* Improved performance on CPython by adding caching around ref resolution
  (#203)

v2.4.0
------

* Added a CLI (#134)
* Added absolute path and absolute schema path to errors (#120)
* Added ``relevance``
* Meta-schemas are now loaded via ``pkgutil``

v2.3.0
------

* Added ``by_relevance`` and ``best_match`` (#91)
* Fixed ``format`` to allow adding formats for non-strings (#125)
* Fixed the ``uri`` format to reject URI references (#131)

v2.2.0
------

* Compile the host name regex (#127)
* Allow arbitrary objects to be types (#129)

v2.1.0
------

* Support RFC 3339 datetimes in conformance with the spec
* Fixed error paths for additionalItems + items (#122)
* Fixed wording for min / maxProperties (#117)


v2.0.0
------

* Added ``create`` and ``extend`` to ``jsonschema.validators``
* Removed ``ValidatorMixin``
* Fixed array indices ref resolution (#95)
* Fixed unknown scheme defragmenting and handling (#102)


v1.3.0
------

* Better error tracebacks (#83)
* Raise exceptions in ``ErrorTree``\s for keys not in the instance (#92)
* __cause__ (#93)


v1.2.0
------

* More attributes for ValidationError (#86)
* Added ``ValidatorMixin.descend``
* Fixed bad ``RefResolutionError`` message (#82)


v1.1.0
------

* Canonicalize URIs (#70)
* Allow attaching exceptions to ``format`` errors (#77)


v1.0.0
------

* Support for Draft 4
* Support for format
* Longs are ints too!
* Fixed a number of issues with ``$ref`` support (#66)
* Draft4Validator is now the default
* ``ValidationError.path`` is now in sequential order
* Added ``ValidatorMixin``


v0.8.0
------

* Full support for JSON References
* ``validates`` for registering new validators
* Documentation
* Bugfixes

    * uniqueItems not so unique (#34)
    * Improper any (#47)


v0.7
----

* Partial support for (JSON Pointer) ``$ref``
* Deprecations

  * ``Validator`` is replaced by ``Draft3Validator`` with a slightly different
    interface
  * ``validator(meta_validate=False)``


v0.6
----

* Bugfixes

  * Issue #30 - Wrong behavior for the dependencies property validation
  * Fixed a miswritten test


v0.5
----

* Bugfixes

  * Issue #17 - require path for error objects
  * Issue #18 - multiple type validation for non-objects


v0.4
----

* Preliminary support for programmatic access to error details (Issue #5).
  There are certainly some corner cases that don't do the right thing yet, but
  this works mostly.

    In order to make this happen (and also to clean things up a bit), a number
    of deprecations are necessary:

        * ``stop_on_error`` is deprecated in ``Validator.__init__``. Use 
          ``Validator.iter_errors()`` instead.
        * ``number_types`` and ``string_types`` are deprecated there as well.
          Use ``types={"number" : ..., "string" : ...}`` instead.
        * ``meta_validate`` is also deprecated, and instead is now accepted as
          an argument to ``validate``, ``iter_errors`` and ``is_valid``.

* A bugfix or two


v0.3
----

* Default for unknown types and properties is now to *not* error (consistent
  with the schema).
* Python 3 support
* Removed dependency on SecureTypes now that the hash bug has been resolved.
* "Numerous bug fixes" -- most notably, a divisibleBy error for floats and a
  bunch of missing typechecks for irrelevant properties.
