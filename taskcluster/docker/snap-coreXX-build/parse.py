#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import sys

import yaml


def has_pkg_section(p, section):
    has_section = section in p.keys()
    if has_section:
        for pkg in p[section]:
            yield pkg


def iter_pkgs(part, all_pkgs):
    for pkg in has_pkg_section(part, "build-packages"):
        if pkg not in all_pkgs:
            all_pkgs.append(pkg)
    for pkg in has_pkg_section(part, "stage-packages"):
        if pkg not in all_pkgs:
            all_pkgs.append(pkg)


def parse(yaml_file):
    all_pkgs = []
    with open(yaml_file, "r") as inp:
        snap = yaml.safe_load(inp)
        parts = snap["parts"]
        for p in parts:
            iter_pkgs(parts[p], all_pkgs)
    return " ".join(all_pkgs)


if __name__ == "__main__":
    print(parse(sys.argv[1]))
