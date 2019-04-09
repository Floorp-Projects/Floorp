
from __future__ import absolute_import, unicode_literals, print_function

import os
import manifestparser


class XpcshellCreator():
    template_body = """/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_TODO() {
  ok(true, "TODO: implement the test");
});
"""

    def get_template_contents(self, suite, doc):
        return self.template_body

    def add_test(self, test, suite, doc):
        content = self.get_template_contents(suite, doc)
        with open(test, "w") as f:
            f.write(content)

        manifest_file = os.path.join(os.path.dirname(test), "xpcshell.ini")
        filename = os.path.basename(test)

        if not os.path.isfile(manifest_file):
            print('Could not open manifest file {}'.format(manifest_file))
            return
        write_to_ini_file(manifest_file, filename)


class MochitestCreator():
    def get_template_contents(self, suite, doc):
        mochitest_templates = os.path.abspath(
            os.path.join(os.path.dirname(__file__), 'mochitest', 'static')
        )
        template_file_name = None
        if suite == "mochitest-browser":
            template_file_name = 'browser.template.txt'

        if suite == "mochitest-plain":
            template_file_name = 'plain{}.template.txt'.format(doc)

        if suite == "mochitest-chrome":
            template_file_name = 'chrome{}.template.txt'.format(doc)

        if template_file_name is None:
            return None

        template_file = os.path.join(mochitest_templates, template_file_name)
        if not os.path.isfile(template_file):
            return None

        with open(template_file) as f:
            return f.read()

    def add_test(self, test, suite, doc):
        content = self.get_template_contents(suite, doc)
        with open(test, "w") as f:
            f.write(content)

        # attempt to insert into the appropriate manifest
        guessed_ini = {
            "mochitest-plain": "mochitest.ini",
            "mochitest-chrome": "chrome.ini",
            "mochitest-browser": "browser.ini"
        }[suite]
        manifest_file = os.path.join(os.path.dirname(test), guessed_ini)
        filename = os.path.basename(test)

        if not os.path.isfile(manifest_file):
            print('Could not open manifest file {}'.format(manifest_file))
            return

        write_to_ini_file(manifest_file, filename)


def write_to_ini_file(manifest_file, filename):
    # Insert a new test in the right place within a given manifest file
    manifest = manifestparser.TestManifest(manifests=[manifest_file])
    insert_before = None

    if any(t['name'] == filename for t in manifest.tests):
        print("{} is already in the manifest.".format(filename))
        return

    for test in manifest.tests:
        if test.get('name') > filename:
            insert_before = test.get('name')
            break

    with open(manifest_file, "r") as f:
        contents = f.readlines()

    filename = '[{}]\n'.format(filename)

    if not insert_before:
        contents.append(filename)
    else:
        insert_before = '[{}]'.format(insert_before)
        for i in range(len(contents)):
            if contents[i].startswith(insert_before):
                contents.insert(i, filename)
                break

    with open(manifest_file, "w") as f:
        f.write("".join(contents))
