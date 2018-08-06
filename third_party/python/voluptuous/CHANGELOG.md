# Changelog

## [0.11.0]

**Changes**:

- [#293](https://github.com/alecthomas/voluptuous/pull/293): Support Python 3.6.
- [#294](https://github.com/alecthomas/voluptuous/pull/294): Drop support for Python 2.6, 3.1 and 3.2.
- [#318](https://github.com/alecthomas/voluptuous/pull/318): Allow to use nested schema and allow any validator to be compiled.
- [#324](https://github.com/alecthomas/voluptuous/pull/324):
  Default values MUST now pass validation just as any regular value. This is a backward incompatible change if a schema uses default values that don't pass validation against the specified schema.
- [#328](https://github.com/alecthomas/voluptuous/pull/328):
  Modify `__lt__` in Marker class to allow comparison with non Marker objects, such as str and int.

**New**:

- [#307](https://github.com/alecthomas/voluptuous/pull/307): Add description field to `Marker` instances.
- [#311](https://github.com/alecthomas/voluptuous/pull/311): Add `Schema.infer` method for basic schema inference.
- [#314](https://github.com/alecthomas/voluptuous/pull/314): Add `SomeOf` validator.

**Fixes**:

- [#279](https://github.com/alecthomas/voluptuous/pull/279):
  Treat Python 2 old-style classes like types when validating.
- [#280](https://github.com/alecthomas/voluptuous/pull/280): Make
  `IsDir()`, `IsFile()` and `PathExists()` consistent between different Python versions.
- [#290](https://github.com/alecthomas/voluptuous/pull/290): Use absolute imports to avoid import conflicts.
- [#291](https://github.com/alecthomas/voluptuous/pull/291): Fix `Coerce` validator to catch `decimal.InvalidOperation`.
- [#298](https://github.com/alecthomas/voluptuous/pull/298): Make `Schema([])` usage consistent with `Schema({})`.
- [#303](https://github.com/alecthomas/voluptuous/pull/303): Allow partial validation when using validate decorator.
- [#316](https://github.com/alecthomas/voluptuous/pull/316): Make `Schema.__eq__` deterministic.
- [#319](https://github.com/alecthomas/voluptuous/pull/319): Replace implementation of `Maybe(s)` with `Any(None, s)` to allow it to be compiled.

## [0.10.5]

- [#278](https://github.com/alecthomas/voluptuous/pull/278): Unicode
translation to python 2 issue fixed.

## [0.10.2]

**Changes**:

- [#195](https://github.com/alecthomas/voluptuous/pull/195):
  `Range` raises `RangeInvalid` when testing `math.nan`.
- [#215](https://github.com/alecthomas/voluptuous/pull/215):
  `{}` and `[]` now always evaluate as is, instead of as any dict or any list.
  To specify a free-form list, use `list` instead of `[]`. To specify a
  free-form dict, use `dict` instead of `Schema({}, extra=ALLOW_EXTRA)`.
- [#224](https://github.com/alecthomas/voluptuous/pull/224):
  Change the encoding of keys in error messages from Unicode to UTF-8.

**New**:

- [#185](https://github.com/alecthomas/voluptuous/pull/185):
  Add argument validation decorator.
- [#199](https://github.com/alecthomas/voluptuous/pull/199):
  Add `Unordered`.
- [#200](https://github.com/alecthomas/voluptuous/pull/200):
  Add `Equal`.
- [#207](https://github.com/alecthomas/voluptuous/pull/207):
  Add `Number`.
- [#210](https://github.com/alecthomas/voluptuous/pull/210):
  Add `Schema` equality check.
- [#212](https://github.com/alecthomas/voluptuous/pull/212):
  Add `coveralls`.
- [#227](https://github.com/alecthomas/voluptuous/pull/227):
  Improve `Marker` management in `Schema`.
- [#232](https://github.com/alecthomas/voluptuous/pull/232):
  Add `Maybe`.
- [#234](https://github.com/alecthomas/voluptuous/pull/234):
  Add `Date`.
- [#236](https://github.com/alecthomas/voluptuous/pull/236), [#237](https://github.com/alecthomas/voluptuous/pull/237), and [#238](https://github.com/alecthomas/voluptuous/pull/238):
  Add script for updating `gh-pages`.
- [#256](https://github.com/alecthomas/voluptuous/pull/256):
  Add support for `OrderedDict` validation.
- [#258](https://github.com/alecthomas/voluptuous/pull/258):
  Add `Contains`.

**Fixes**:

- [#197](https://github.com/alecthomas/voluptuous/pull/197):
  `ExactSequence` checks sequences are the same length.
- [#201](https://github.com/alecthomas/voluptuous/pull/201):
  Empty lists are evaluated as is.
- [#205](https://github.com/alecthomas/voluptuous/pull/205):
  Filepath validators correctly handle `None`.
- [#206](https://github.com/alecthomas/voluptuous/pull/206):
  Handle non-subscriptable types in `humanize_error`.
- [#231](https://github.com/alecthomas/voluptuous/pull/231):
  Validate `namedtuple` as a `tuple`.
- [#235](https://github.com/alecthomas/voluptuous/pull/235):
  Update docstring.
- [#249](https://github.com/alecthomas/voluptuous/pull/249):
  Update documentation.
- [#262](https://github.com/alecthomas/voluptuous/pull/262):
  Fix a performance issue of exponential complexity where all of the dict keys were matched against all keys in the schema.
  This resulted in O(n*m) complexity where n is the number of keys in the dict being validated and m is the number of keys in the schema.
  The fix ensures that each key in the dict is matched against the relevant schema keys only. It now works in O(n).
- [#266](https://github.com/alecthomas/voluptuous/pull/266):
  Remove setuptools as a dependency.

## 0.9.3 (2016-08-03)

Changelog not kept for 0.9.3 and earlier releases.
