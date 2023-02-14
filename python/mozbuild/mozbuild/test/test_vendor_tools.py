# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit

from mozbuild.vendor.vendor_manifest import list_of_paths_to_readable_string


def test_list_of_paths_to_readable_string():
    paths = ["/tmp/a", "/tmp/b"]
    s = list_of_paths_to_readable_string(paths)
    assert not s.endswith(", ]")
    assert s.endswith("]")
    assert "/tmp/a" in s
    assert "/tmp/b" in s

    paths = ["/tmp/a", "/tmp/b", "/tmp/c", "/tmp/d"]
    s = list_of_paths_to_readable_string(paths)
    assert not s.endswith(", ")
    assert s.endswith("]")
    assert "/tmp/a" not in s
    assert "/tmp/b" not in s
    assert "4 items in /tmp" in s

    paths = [
        "/tmp/a",
        "/tmp/b",
        "/tmp/c",
        "/tmp/d",
        "/tmp/d",
        "/tmp/d",
        "/tmp/d",
        "/tmp/d",
        "/tmp/d",
        "/tmp/d",
    ]
    s = list_of_paths_to_readable_string(paths)
    assert not s.endswith(", ")
    assert s.endswith("]")
    assert "/tmp/a" not in s
    assert " a" not in s
    assert "/tmp/b" not in s
    assert "10 (omitted) items in /tmp" in s

    paths = ["/tmp", "/foo"]
    s = list_of_paths_to_readable_string(paths)
    assert not s.endswith(", ")
    assert s.endswith("]")
    assert "/tmp" in s
    assert "/foo" in s

    paths = [
        "/tmp/a",
        "/tmp/b",
        "/tmp/c",
        "/tmp/d",
        "/tmp/d",
        "/tmp/d",
        "/tmp/d",
        "/tmp/d",
        "/tmp/d",
        "/tmp/d",
    ]
    paths.extend(["/foo/w", "/foo/x", "/foo/y", "/foo/z"])
    paths.extend(["/bar/m", "/bar/n"])
    paths.extend(["/etc"])
    s = list_of_paths_to_readable_string(paths)
    assert not s.endswith(", ")
    assert s.endswith("]")
    assert "/tmp/a" not in s
    assert " d" not in s
    assert "/tmp/b" not in s
    assert "10 (omitted) items in /tmp" in s

    assert "/foo/w" not in s
    assert "/foo/x" not in s
    assert "4 items in /foo" in s
    assert " w" in s

    assert "/bar/m" in s
    assert "/bar/n" in s

    assert "/etc" in s

    assert len(s) < len(str(paths))


if __name__ == "__main__":
    mozunit.main()
