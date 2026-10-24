# SPDX-License-Identifier: MPL-2.0
"""Select distributable application archives, keeping auxiliary XPIDL ZIPs separate."""

_SUFFIXES = {"linux": (".tar.xz", ".tar.gz"), "windows": (".zip",), "mac": (".dmg",)}


def is_product_archive(name):
    # mach package also produces this exact auxiliary archive suffix. Its
    # typelib data is not an executable application package.
    return (not name.endswith(".xpt_artifacts.zip") and
            name.endswith((".zip", ".tar.xz", ".tar.gz", ".dmg")))


def distributable_packages(directory):
    return sorted(path for path in directory.glob("floorp-*")
                  if path.is_file() and is_product_archive(path.name))


def select_native_package(entries, platform):
    packages = [entry for entry in entries
                if is_product_archive(entry.get("name", "")) and
                entry.get("name", "").endswith(_SUFFIXES[platform])]
    if len(packages) != 1:
        raise RuntimeError("Expected exactly one native product package for execution")
    return packages[0]
