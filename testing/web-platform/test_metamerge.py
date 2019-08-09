from io import BytesIO

import mozunit

import metamerge

ancestor = """
global-new-deleted: A
global-new-changed: A
global-current-deleted: A
global-current-changed: A

[failing-test.html]
  [Unchanged subtest]
    expected: FAIL

  [New deleted subtest]
    expected: FAIL

  [New modified subtest]
    expected: TIMEOUT

  [Current deleted subtest]
    expected: FAIL

  [New modified current deleted]
    expected: FAIL

  [Ancestor no expected new expected]
    bug: 1234

[new-deleted-test.html]
  [Deleted subtest]
    expected: FAIL

[current-deleted-test.html]
  [Deleted subtest]
    expected: FAIL

[test-modified.html]
  expected: TIMEOUT

[new-modified-current-deleted.html]
  expected:
    if os == "linux": FAIL
    TIMEOUT
"""

new = """
global-new-added: A
global-new-changed: B
global-current-deleted: A
global-current-changed: A

[failing-test.html]
  [Unchanged subtest]
    expected: FAIL

  [New added subtest]
    expected: FAIL

  [New modified subtest]
    expected:
      if os == "linux": FAIL
      TIMEOUT

  [Current deleted subtest]
    expected: FAIL

  [New modified current deleted]
    expected: TIMEOUT

  [Ancestor no expected new expected]
    bug: 1234
    expected: FAIL

[new-added-test.html]
  [Added subtest]
    expected: FAIL

[current-deleted-test.html]
  [Deleted subtest]
    expected: FAIL

[test-modified.html]
  expected:
    if os == "linux": FAIL

[new-modified-current-deleted.html]
  expected:
    if os == "linux": FAIL
    if os == "mac": FAIL
    TIMEOUT
"""

current = """
global-new-deleted: A
global-new-changed: A
global-current-added: A
global-current-changed: B

[failing-test.html]
  [Unchanged subtest]
    expected: FAIL

  [New deleted subtest]
    expected: FAIL

  [New modified subtest]
    expected: TIMEOUT

  [Current added subtest]
    expected: FAIL

  [Ancestor no expected new expected]
    bug: 1234

[new-deleted-test.html]
  [Deleted subtest]
    expected: FAIL

[current-added-test.html]
  [Added subtest]
    expected: FAIL
"""

updated = """global-new-deleted: A
global-new-changed: A
global-current-added: A
global-current-changed: B
[failing-test.html]
  [Unchanged subtest]
    expected: FAIL

  [New modified subtest]
    expected:
      if os == "linux": FAIL
      TIMEOUT

  [Current added subtest]
    expected: FAIL

  [Ancestor no expected new expected]
    bug: 1234
    expected: FAIL

  [New added subtest]
    expected: FAIL

  [New modified current deleted]
    expected: TIMEOUT


[current-added-test.html]
  [Added subtest]
    expected: FAIL


[new-added-test.html]
  [Added subtest]
    expected: FAIL


[new-modified-current-deleted.html]
  expected:
    if os == "linux": FAIL
    if os == "mac": FAIL
    TIMEOUT

[test-modified.html]
  expected:
    if os == "linux": FAIL
"""


def test_merge():
    def get_manifest(str_data):
        bytes_io = BytesIO(str_data)
        return metamerge.compile(bytes_io,
                                 metamerge.data_cls_getter)
    ancestor_manifest = get_manifest(ancestor)
    current_manifest = get_manifest(current)
    new_manifest = get_manifest(new)

    assert metamerge.make_changes(ancestor_manifest,
                                  current_manifest,
                                  new_manifest) == updated

if __name__ == '__main__':
    mozunit.main()
