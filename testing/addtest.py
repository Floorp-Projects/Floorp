
from __future__ import absolute_import, unicode_literals, print_function

import os
import manifestparser


class Creator(object):
    def __init__(self, topsrcdir, test, suite, doc, **kwargs):
        self.topsrcdir = topsrcdir
        self.test = test
        self.suite = suite
        self.doc = doc
        self.kwargs = kwargs

    def check_args(self):
        """Perform any validation required for suite-specific arguments"""
        return True

    def __iter__(self):
        """Iterate over a list of (path, data) tuples coresponding to the files
        to be created"""
        yield (self.test, self._get_template_contents())

    def _get_template_contents(self, **kwargs):
        raise NotImplementedError

    def update_manifest(self):
        """Perform any manifest updates required to register the added tests"""
        raise NotImplementedError


class XpcshellCreator(Creator):
    template_body = """/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_TODO() {
  ok(true, "TODO: implement the test");
});
"""

    def _get_template_contents(self):
        return self.template_body

    def update_manifest(self):
        manifest_file = os.path.join(os.path.dirname(self.test), "xpcshell.ini")
        filename = os.path.basename(self.test)

        if not os.path.isfile(manifest_file):
            print('Could not open manifest file {}'.format(manifest_file))
            return
        write_to_ini_file(manifest_file, filename)


class MochitestCreator(Creator):
    templates = {
        "mochitest-browser-chrome": "browser.template.txt",
        "mochitest-plain": "plain%(doc)s.template.txt",
        "mochitest-chrome": "chrome%(doc)s.template.txt",
    }

    def _get_template_contents(self):
        mochitest_templates = os.path.abspath(
            os.path.join(os.path.dirname(__file__), 'mochitest', 'static')
        )
        template_file_name = None

        template_file_name = self.templates.get(self.suite)

        if template_file_name is None:
            print("Sorry, `addtest` doesn't currently know how to add {}".format(self.suite))
            return None

        template_file_name = template_file_name % {"doc": self.doc}

        template_file = os.path.join(mochitest_templates, template_file_name)
        if not os.path.isfile(template_file):
            print("Sorry, `addtest` doesn't currently know how to add {} with document type {}"
                  .format(self.suite, self.doc))
            return None

        with open(template_file) as f:
            return f.read()

    def update_manifest(self):
        # attempt to insert into the appropriate manifest
        guessed_ini = {
            "mochitest-plain": "mochitest.ini",
            "mochitest-chrome": "chrome.ini",
            "mochitest-browser-chrome": "browser.ini"
        }[self.suite]
        manifest_file = os.path.join(os.path.dirname(self.test), guessed_ini)
        filename = os.path.basename(self.test)

        if not os.path.isfile(manifest_file):
            print('Could not open manifest file {}'.format(manifest_file))
            return

        write_to_ini_file(manifest_file, filename)


class WebPlatformTestsCreator(Creator):
    template_prefix = """<!doctype html>
%(documentElement)s<meta charset=utf-8>
"""
    template_long_timeout = "<meta name=timeout content=long>\n"

    template_body_th = """<title></title>
<script src=/resources/testharness.js></script>
<script src=/resources/testharnessreport.js></script>
<script>

</script>
"""

    template_body_reftest = """<title></title>
<link rel=%(match)s href=%(ref)s>
"""

    template_body_reftest_wait = """<script src="/common/reftest-wait.js"></script>
"""

    upstream_path = os.path.join("testing", "web-platform", "tests")
    local_path = os.path.join("testing", "web-platform", "mozilla", "tests")

    def __init__(self, *args, **kwargs):
        super(WebPlatformTestsCreator, self).__init__(*args, **kwargs)
        self.reftest = self.suite == "web-platform-tests-reftest"

    @classmethod
    def get_parser(cls, parser):
        parser.add_argument("--long-timeout", action="store_true",
                            help="Test should be given a long timeout "
                            "(typically 60s rather than 10s, but varies depending on environment)")
        parser.add_argument("-m", "--reference", dest="ref", help="Path to the reference file")
        parser.add_argument("--mismatch", action="store_true",
                            help="Create a mismatch reftest")
        parser.add_argument("--wait", action="store_true",
                            help="Create a reftest that waits until takeScreenshot() is called")

    def check_args(self):
        if self.wpt_type(self.test) is None:
            print("""Test path %s is not in wpt directories:
testing/web-platform/tests for tests that may be shared
testing/web-platform/mozilla/tests for Gecko-only tests""" % self.test)
            return False

        if not self.reftest:
            if self.kwargs["ref"]:
                print("--ref only makes sense for a reftest")
                return False

            if self.kwargs["mismatch"]:
                print("--mismatch only makes sense for a reftest")
                return False

            if self.kwargs["wait"]:
                print("--wait only makes sense for a reftest")
                return False
        else:
            # Set the ref to a url relative to the test
            if self.kwargs["ref"]:
                if self.ref_path(self.kwargs["ref"]) is None:
                    print("--ref doesn't refer to a path inside web-platform-tests")
                    return False

    def __iter__(self):
        yield (self.test, self._get_template_contents())

        if self.reftest and self.kwargs["ref"]:
            ref_path = self.ref_path(self.kwargs["ref"])
            yield (ref_path, self._get_template_contents(reference=True))

    def _get_template_contents(self, reference=False):
        args = {"documentElement": "<html class=reftest-wait>\n"
                if self.kwargs["wait"] else ""}

        template = self.template_prefix % args
        if self.kwargs["long_timeout"]:
            template += self.template_long_timeout

        if self.reftest:
            if not reference:
                args = {"match": "match" if not self.kwargs["mismatch"] else "mismatch",
                        "ref": self.ref_url(self.kwargs["ref"]) if self.kwargs["ref"] else '""'}
                template += self.template_body_reftest % args
                if self.kwargs["wait"]:
                    template += self.template_body_reftest_wait
            else:
                template += "<title></title>"
        else:
            template += self.template_body_th

        return template

    def update_manifest(self):
        pass

    def src_rel_path(self, path):
        if path is None:
            return

        abs_path = os.path.normpath(os.path.abspath(path))
        return os.path.relpath(abs_path, self.topsrcdir)

    def wpt_type(self, path):
        path = self.src_rel_path(path)
        if path.startswith(self.upstream_path):
            return "upstream"
        elif path.startswith(self.local_path):
            return "local"
        return None

    def ref_path(self, path):
        # The ref parameter can be one of several things
        # 1. An absolute path to a reference file
        # 2. A path to a file relative to the topsrcdir
        # 3. A path relative to the test file
        # These are not unambiguous, so it's somewhat best effort

        if os.path.isabs(path):
            path = os.path.normpath(path)
            if not path.startswith(self.topsrcdir):
                # Path is an absolute URL relative to the tests root
                if path.startswith("/_mozilla/"):
                    base = self.local_path
                    path = path[len("/_mozilla/"):]
                else:
                    base = self.upstream_path
                    path = path[1:]
                path = path.replace("/", os.sep)
                return os.path.join(base, path)
            else:
                return self.src_rel_path(path)
        else:
            if self.wpt_type(path) is not None:
                return path
            else:
                test_rel_path = self.src_rel_path(
                    os.path.join(os.path.dirname(self.test), path))
                if self.wpt_type(test_rel_path) is not None:
                    return test_rel_path
        # Returning None indicates that the path wasn't valid

    def ref_url(self, path):
        ref_path = self.ref_path(path)
        if not ref_path:
            return

        if path[0] == "/" and len(path) < len(ref_path):
            # This is an absolute url
            return path

        # Othewise it's a file path
        wpt_type_ref = self.wpt_type(ref_path)
        wpt_type_test = self.wpt_type(self.test)
        if wpt_type_ref == wpt_type_test:
            return os.path.relpath(ref_path, os.path.dirname(self.test))

        # If we have a local test referencing an upstream ref,
        # or vice-versa use absolute paths
        if wpt_type_ref == "upstream":
            rel_path = os.path.relpath(ref_path, self.upstream_path)
            url_base = "/"
        elif wpt_type_ref == "local":
            rel_path = os.path.relpath(ref_path, self.local_path)
            url_base = "/_mozilla/"
        else:
            return None
        return url_base + rel_path.replace(os.path.sep, "/")


# Insert a new test in the right place within a given manifest file
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


TEST_CREATORS = {"mochitest": MochitestCreator,
                 "web-platform-tests": WebPlatformTestsCreator,
                 "xpcshell": XpcshellCreator}


def creator_for_suite(suite):
    if suite.split("-")[0] == "mochitest":
        base_suite = "mochitest"
    else:
        base_suite = suite.rsplit("-", 1)[0]
    return TEST_CREATORS.get(base_suite)
