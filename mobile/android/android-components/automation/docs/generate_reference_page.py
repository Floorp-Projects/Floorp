# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os

def add_version_to_reference_page(version):
    with open('docs/reference.md', 'r') as f:
        content = f.read()

    content = content.replace(
        "<!-- MARKER -->",
        "<!-- MARKER -->\n## [%s](../api/%s/index)" % (version, version))

    with open('docs/reference.md', 'w') as f:
        f.write(content)


def generate_reference_page(version):
    modules = sorted([file for file in os.listdir("docs/api/%s/" % version)])

    with open("docs/api/%s/index.md" % version, 'w') as f:
        f.write("---\n")
        f.write("layout: page\n")
        f.write("title: API Reference - %s\n" % version)
        f.write("permalink: /api/%s/index\n" % version)
        f.write("---\n")
        f.write("\n")

        f.write("\n")

        for module in modules:
            f.write("* [%s](./%s/index.html)\n" % (module, module))


def main():
    version = sys.argv[1]

    add_version_to_reference_page(version)
    generate_reference_page(version)


if __name__ == "__main__":
    main()
