# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest
import mozunit
import os
from buildconfig import topobjdir
from mozpack.packager import (
    preprocess_manifest,
    CallDeque,
    Component,
    SimplePackager,
    SimpleManifestSink,
)
from mozpack.files import GeneratedFile
from mozpack.chrome.manifest import (
    ManifestBinaryComponent,
    ManifestContent,
    ManifestResource,
)
from mozunit import MockedOpen
from mozbuild.preprocessor import Preprocessor
from mozpack.errors import (
    errors,
    ErrorMessage,
)
import mozpack.path as mozpath

MANIFEST = """
bar/*
[foo]
foo/*
-foo/bar
chrome.manifest
[zot destdir="destdir"]
foo/zot
; comment
#ifdef baz
[baz]
baz@SUFFIX@
#endif
"""


class TestPreprocessManifest(unittest.TestCase):
    MANIFEST_PATH = mozpath.join("$OBJDIR", "manifest")

    EXPECTED_LOG = [
        ((MANIFEST_PATH, 2), "add", "", "bar/*"),
        ((MANIFEST_PATH, 4), "add", "foo", "foo/*"),
        ((MANIFEST_PATH, 5), "remove", "foo", "foo/bar"),
        ((MANIFEST_PATH, 6), "add", "foo", "chrome.manifest"),
        ((MANIFEST_PATH, 8), "add", 'zot destdir="destdir"', "foo/zot"),
    ]

    def setUp(self):
        class MockSink(object):
            def __init__(self):
                self.log = []

            def add(self, component, path):
                self._log(errors.get_context(), "add", repr(component), path)

            def remove(self, component, path):
                self._log(errors.get_context(), "remove", repr(component), path)

            def _log(self, *args):
                self.log.append(args)

        self.sink = MockSink()
        self.cwd = os.getcwd()
        os.chdir(topobjdir)

    def tearDown(self):
        os.chdir(self.cwd)

    def test_preprocess_manifest(self):
        with MockedOpen({"manifest": MANIFEST}):
            preprocess_manifest(self.sink, "manifest")
        self.assertEqual(self.sink.log, self.EXPECTED_LOG)

    def test_preprocess_manifest_missing_define(self):
        with MockedOpen({"manifest": MANIFEST}):
            self.assertRaises(
                Preprocessor.Error,
                preprocess_manifest,
                self.sink,
                "manifest",
                {"baz": 1},
            )

    def test_preprocess_manifest_defines(self):
        with MockedOpen({"manifest": MANIFEST}):
            preprocess_manifest(self.sink, "manifest", {"baz": 1, "SUFFIX": ".exe"})
        self.assertEqual(
            self.sink.log,
            self.EXPECTED_LOG + [((self.MANIFEST_PATH, 12), "add", "baz", "baz.exe")],
        )


class MockFinder(object):
    def __init__(self, files):
        self.files = files
        self.log = []

    def find(self, path):
        self.log.append(path)
        for f in sorted(self.files):
            if mozpath.match(f, path):
                yield f, self.files[f]

    def __iter__(self):
        return self.find("")


class MockFormatter(object):
    def __init__(self):
        self.log = []

    def add_base(self, *args):
        self._log(errors.get_context(), "add_base", *args)

    def add_manifest(self, *args):
        self._log(errors.get_context(), "add_manifest", *args)

    def add_interfaces(self, *args):
        self._log(errors.get_context(), "add_interfaces", *args)

    def add(self, *args):
        self._log(errors.get_context(), "add", *args)

    def _log(self, *args):
        self.log.append(args)


class TestSimplePackager(unittest.TestCase):
    def test_simple_packager(self):
        class GeneratedFileWithPath(GeneratedFile):
            def __init__(self, path, content):
                GeneratedFile.__init__(self, content)
                self.path = path

        formatter = MockFormatter()
        packager = SimplePackager(formatter)
        curdir = os.path.abspath(os.curdir)
        file = GeneratedFileWithPath(
            os.path.join(curdir, "foo", "bar.manifest"),
            b"resource bar bar/\ncontent bar bar/",
        )
        with errors.context("manifest", 1):
            packager.add("foo/bar.manifest", file)

        file = GeneratedFileWithPath(
            os.path.join(curdir, "foo", "baz.manifest"), b"resource baz baz/"
        )
        with errors.context("manifest", 2):
            packager.add("bar/baz.manifest", file)

        with errors.context("manifest", 3):
            packager.add(
                "qux/qux.manifest",
                GeneratedFile(
                    b"".join(
                        [
                            b"resource qux qux/\n",
                            b"binary-component qux.so\n",
                        ]
                    )
                ),
            )
        bar_xpt = GeneratedFile(b"bar.xpt")
        qux_xpt = GeneratedFile(b"qux.xpt")
        foo_html = GeneratedFile(b"foo_html")
        bar_html = GeneratedFile(b"bar_html")
        with errors.context("manifest", 4):
            packager.add("foo/bar.xpt", bar_xpt)
        with errors.context("manifest", 5):
            packager.add("foo/bar/foo.html", foo_html)
            packager.add("foo/bar/bar.html", bar_html)

        file = GeneratedFileWithPath(
            os.path.join(curdir, "foo.manifest"),
            b"".join(
                [
                    b"manifest foo/bar.manifest\n",
                    b"manifest bar/baz.manifest\n",
                ]
            ),
        )
        with errors.context("manifest", 6):
            packager.add("foo.manifest", file)
        with errors.context("manifest", 7):
            packager.add("foo/qux.xpt", qux_xpt)

        file = GeneratedFileWithPath(
            os.path.join(curdir, "addon", "chrome.manifest"), b"resource hoge hoge/"
        )
        with errors.context("manifest", 8):
            packager.add("addon/chrome.manifest", file)

        install_rdf = GeneratedFile(b"<RDF></RDF>")
        with errors.context("manifest", 9):
            packager.add("addon/install.rdf", install_rdf)

        with errors.context("manifest", 10):
            packager.add("addon2/install.rdf", install_rdf)
            packager.add(
                "addon2/chrome.manifest", GeneratedFile(b"binary-component addon2.so")
            )

        with errors.context("manifest", 11):
            packager.add("addon3/install.rdf", install_rdf)
            packager.add(
                "addon3/chrome.manifest",
                GeneratedFile(b"manifest components/components.manifest"),
            )
            packager.add(
                "addon3/components/components.manifest",
                GeneratedFile(b"binary-component addon3.so"),
            )

        with errors.context("manifest", 12):
            install_rdf_addon4 = GeneratedFile(
                b"<RDF>\n<...>\n<em:unpack>true</em:unpack>\n<...>\n</RDF>"
            )
            packager.add("addon4/install.rdf", install_rdf_addon4)

        with errors.context("manifest", 13):
            install_rdf_addon5 = GeneratedFile(
                b"<RDF>\n<...>\n<em:unpack>false</em:unpack>\n<...>\n</RDF>"
            )
            packager.add("addon5/install.rdf", install_rdf_addon5)

        with errors.context("manifest", 14):
            install_rdf_addon6 = GeneratedFile(
                b"<RDF>\n<... em:unpack=true>\n<...>\n</RDF>"
            )
            packager.add("addon6/install.rdf", install_rdf_addon6)

        with errors.context("manifest", 15):
            install_rdf_addon7 = GeneratedFile(
                b"<RDF>\n<... em:unpack=false>\n<...>\n</RDF>"
            )
            packager.add("addon7/install.rdf", install_rdf_addon7)

        with errors.context("manifest", 16):
            install_rdf_addon8 = GeneratedFile(
                b'<RDF>\n<... em:unpack="true">\n<...>\n</RDF>'
            )
            packager.add("addon8/install.rdf", install_rdf_addon8)

        with errors.context("manifest", 17):
            install_rdf_addon9 = GeneratedFile(
                b'<RDF>\n<... em:unpack="false">\n<...>\n</RDF>'
            )
            packager.add("addon9/install.rdf", install_rdf_addon9)

        with errors.context("manifest", 18):
            install_rdf_addon10 = GeneratedFile(
                b"<RDF>\n<... em:unpack='true'>\n<...>\n</RDF>"
            )
            packager.add("addon10/install.rdf", install_rdf_addon10)

        with errors.context("manifest", 19):
            install_rdf_addon11 = GeneratedFile(
                b"<RDF>\n<... em:unpack='false'>\n<...>\n</RDF>"
            )
            packager.add("addon11/install.rdf", install_rdf_addon11)

        we_manifest = GeneratedFile(
            b'{"manifest_version": 2, "name": "Test WebExtension", "version": "1.0"}'
        )
        # hybrid and hybrid2 are both bootstrapped extensions with
        # embedded webextensions, they differ in the order in which
        # the manifests are added to the packager.
        with errors.context("manifest", 20):
            packager.add("hybrid/install.rdf", install_rdf)

        with errors.context("manifest", 21):
            packager.add("hybrid/webextension/manifest.json", we_manifest)

        with errors.context("manifest", 22):
            packager.add("hybrid2/webextension/manifest.json", we_manifest)

        with errors.context("manifest", 23):
            packager.add("hybrid2/install.rdf", install_rdf)

        with errors.context("manifest", 24):
            packager.add("webextension/manifest.json", we_manifest)

        non_we_manifest = GeneratedFile(b'{"not a webextension": true}')
        with errors.context("manifest", 25):
            packager.add("nonwebextension/manifest.json", non_we_manifest)

        self.assertEqual(formatter.log, [])

        with errors.context("dummy", 1):
            packager.close()
        self.maxDiff = None
        # The formatter is expected to reorder the manifest entries so that
        # chrome entries appear before the others.
        self.assertEqual(
            formatter.log,
            [
                (("dummy", 1), "add_base", "", False),
                (("dummy", 1), "add_base", "addon", True),
                (("dummy", 1), "add_base", "addon10", "unpacked"),
                (("dummy", 1), "add_base", "addon11", True),
                (("dummy", 1), "add_base", "addon2", "unpacked"),
                (("dummy", 1), "add_base", "addon3", "unpacked"),
                (("dummy", 1), "add_base", "addon4", "unpacked"),
                (("dummy", 1), "add_base", "addon5", True),
                (("dummy", 1), "add_base", "addon6", "unpacked"),
                (("dummy", 1), "add_base", "addon7", True),
                (("dummy", 1), "add_base", "addon8", "unpacked"),
                (("dummy", 1), "add_base", "addon9", True),
                (("dummy", 1), "add_base", "hybrid", True),
                (("dummy", 1), "add_base", "hybrid2", True),
                (("dummy", 1), "add_base", "qux", False),
                (("dummy", 1), "add_base", "webextension", True),
                (
                    (os.path.join(curdir, "foo", "bar.manifest"), 2),
                    "add_manifest",
                    ManifestContent("foo", "bar", "bar/"),
                ),
                (
                    (os.path.join(curdir, "foo", "bar.manifest"), 1),
                    "add_manifest",
                    ManifestResource("foo", "bar", "bar/"),
                ),
                (
                    ("bar/baz.manifest", 1),
                    "add_manifest",
                    ManifestResource("bar", "baz", "baz/"),
                ),
                (
                    ("qux/qux.manifest", 1),
                    "add_manifest",
                    ManifestResource("qux", "qux", "qux/"),
                ),
                (
                    ("qux/qux.manifest", 2),
                    "add_manifest",
                    ManifestBinaryComponent("qux", "qux.so"),
                ),
                (("manifest", 4), "add_interfaces", "foo/bar.xpt", bar_xpt),
                (("manifest", 7), "add_interfaces", "foo/qux.xpt", qux_xpt),
                (
                    (os.path.join(curdir, "addon", "chrome.manifest"), 1),
                    "add_manifest",
                    ManifestResource("addon", "hoge", "hoge/"),
                ),
                (
                    ("addon2/chrome.manifest", 1),
                    "add_manifest",
                    ManifestBinaryComponent("addon2", "addon2.so"),
                ),
                (
                    ("addon3/components/components.manifest", 1),
                    "add_manifest",
                    ManifestBinaryComponent("addon3/components", "addon3.so"),
                ),
                (("manifest", 5), "add", "foo/bar/foo.html", foo_html),
                (("manifest", 5), "add", "foo/bar/bar.html", bar_html),
                (("manifest", 9), "add", "addon/install.rdf", install_rdf),
                (("manifest", 10), "add", "addon2/install.rdf", install_rdf),
                (("manifest", 11), "add", "addon3/install.rdf", install_rdf),
                (("manifest", 12), "add", "addon4/install.rdf", install_rdf_addon4),
                (("manifest", 13), "add", "addon5/install.rdf", install_rdf_addon5),
                (("manifest", 14), "add", "addon6/install.rdf", install_rdf_addon6),
                (("manifest", 15), "add", "addon7/install.rdf", install_rdf_addon7),
                (("manifest", 16), "add", "addon8/install.rdf", install_rdf_addon8),
                (("manifest", 17), "add", "addon9/install.rdf", install_rdf_addon9),
                (("manifest", 18), "add", "addon10/install.rdf", install_rdf_addon10),
                (("manifest", 19), "add", "addon11/install.rdf", install_rdf_addon11),
                (("manifest", 20), "add", "hybrid/install.rdf", install_rdf),
                (
                    ("manifest", 21),
                    "add",
                    "hybrid/webextension/manifest.json",
                    we_manifest,
                ),
                (
                    ("manifest", 22),
                    "add",
                    "hybrid2/webextension/manifest.json",
                    we_manifest,
                ),
                (("manifest", 23), "add", "hybrid2/install.rdf", install_rdf),
                (("manifest", 24), "add", "webextension/manifest.json", we_manifest),
                (
                    ("manifest", 25),
                    "add",
                    "nonwebextension/manifest.json",
                    non_we_manifest,
                ),
            ],
        )

        self.assertEqual(
            packager.get_bases(),
            set(
                [
                    "",
                    "addon",
                    "addon2",
                    "addon3",
                    "addon4",
                    "addon5",
                    "addon6",
                    "addon7",
                    "addon8",
                    "addon9",
                    "addon10",
                    "addon11",
                    "qux",
                    "hybrid",
                    "hybrid2",
                    "webextension",
                ]
            ),
        )
        self.assertEqual(packager.get_bases(addons=False), set(["", "qux"]))

    def test_simple_packager_manifest_consistency(self):
        formatter = MockFormatter()
        # bar/ is detected as an addon because of install.rdf, but top-level
        # includes a manifest inside bar/.
        packager = SimplePackager(formatter)
        packager.add(
            "base.manifest",
            GeneratedFile(
                b"manifest foo/bar.manifest\n" b"manifest bar/baz.manifest\n"
            ),
        )
        packager.add("foo/bar.manifest", GeneratedFile(b"resource bar bar"))
        packager.add("bar/baz.manifest", GeneratedFile(b"resource baz baz"))
        packager.add("bar/install.rdf", GeneratedFile(b""))

        with self.assertRaises(ErrorMessage) as e:
            packager.close()

        self.assertEqual(
            str(e.exception),
            'Error: "bar/baz.manifest" is included from "base.manifest", '
            'which is outside "bar"',
        )

        # bar/ is detected as a separate base because of chrome.manifest that
        # is included nowhere, but top-level includes another manifest inside
        # bar/.
        packager = SimplePackager(formatter)
        packager.add(
            "base.manifest",
            GeneratedFile(
                b"manifest foo/bar.manifest\n" b"manifest bar/baz.manifest\n"
            ),
        )
        packager.add("foo/bar.manifest", GeneratedFile(b"resource bar bar"))
        packager.add("bar/baz.manifest", GeneratedFile(b"resource baz baz"))
        packager.add("bar/chrome.manifest", GeneratedFile(b"resource baz baz"))

        with self.assertRaises(ErrorMessage) as e:
            packager.close()

        self.assertEqual(
            str(e.exception),
            'Error: "bar/baz.manifest" is included from "base.manifest", '
            'which is outside "bar"',
        )

        # bar/ is detected as a separate base because of chrome.manifest that
        # is included nowhere, but chrome.manifest includes baz.manifest from
        # the same directory. This shouldn't error out.
        packager = SimplePackager(formatter)
        packager.add("base.manifest", GeneratedFile(b"manifest foo/bar.manifest\n"))
        packager.add("foo/bar.manifest", GeneratedFile(b"resource bar bar"))
        packager.add("bar/baz.manifest", GeneratedFile(b"resource baz baz"))
        packager.add("bar/chrome.manifest", GeneratedFile(b"manifest baz.manifest"))
        packager.close()


class TestSimpleManifestSink(unittest.TestCase):
    def test_simple_manifest_parser(self):
        formatter = MockFormatter()
        foobar = GeneratedFile(b"foobar")
        foobaz = GeneratedFile(b"foobaz")
        fooqux = GeneratedFile(b"fooqux")
        foozot = GeneratedFile(b"foozot")
        finder = MockFinder(
            {
                "bin/foo/bar": foobar,
                "bin/foo/baz": foobaz,
                "bin/foo/qux": fooqux,
                "bin/foo/zot": foozot,
                "bin/foo/chrome.manifest": GeneratedFile(b"resource foo foo/"),
                "bin/chrome.manifest": GeneratedFile(b"manifest foo/chrome.manifest"),
            }
        )
        parser = SimpleManifestSink(finder, formatter)
        component0 = Component("component0")
        component1 = Component("component1")
        component2 = Component("component2", destdir="destdir")
        parser.add(component0, "bin/foo/b*")
        parser.add(component1, "bin/foo/qux")
        parser.add(component1, "bin/foo/chrome.manifest")
        parser.add(component2, "bin/foo/zot")
        self.assertRaises(ErrorMessage, parser.add, "component1", "bin/bar")

        self.assertEqual(formatter.log, [])
        parser.close()
        self.assertEqual(
            formatter.log,
            [
                (None, "add_base", "", False),
                (
                    ("foo/chrome.manifest", 1),
                    "add_manifest",
                    ManifestResource("foo", "foo", "foo/"),
                ),
                (None, "add", "foo/bar", foobar),
                (None, "add", "foo/baz", foobaz),
                (None, "add", "foo/qux", fooqux),
                (None, "add", "destdir/foo/zot", foozot),
            ],
        )

        self.assertEqual(
            finder.log,
            [
                "bin/foo/b*",
                "bin/foo/qux",
                "bin/foo/chrome.manifest",
                "bin/foo/zot",
                "bin/bar",
                "bin/chrome.manifest",
            ],
        )


class TestCallDeque(unittest.TestCase):
    def test_call_deque(self):
        class Logger(object):
            def __init__(self):
                self._log = []

            def log(self, str):
                self._log.append(str)

            @staticmethod
            def staticlog(logger, str):
                logger.log(str)

        def do_log(logger, str):
            logger.log(str)

        logger = Logger()
        d = CallDeque()
        d.append(logger.log, "foo")
        d.append(logger.log, "bar")
        d.append(logger.staticlog, logger, "baz")
        d.append(do_log, logger, "qux")
        self.assertEqual(logger._log, [])
        d.execute()
        self.assertEqual(logger._log, ["foo", "bar", "baz", "qux"])


class TestComponent(unittest.TestCase):
    def do_split(self, string, name, options):
        n, o = Component._split_component_and_options(string)
        self.assertEqual(name, n)
        self.assertEqual(options, o)

    def test_component_split_component_and_options(self):
        self.do_split("component", "component", {})
        self.do_split("trailingspace ", "trailingspace", {})
        self.do_split(" leadingspace", "leadingspace", {})
        self.do_split(" trim ", "trim", {})
        self.do_split(' trim key="value"', "trim", {"key": "value"})
        self.do_split(' trim empty=""', "trim", {"empty": ""})
        self.do_split(' trim space=" "', "trim", {"space": " "})
        self.do_split(
            'component key="value"  key2="second" ',
            "component",
            {"key": "value", "key2": "second"},
        )
        self.do_split(
            'trim key="  value with spaces   "  key2="spaces again"',
            "trim",
            {"key": "  value with spaces   ", "key2": "spaces again"},
        )

    def do_split_error(self, string):
        self.assertRaises(ValueError, Component._split_component_and_options, string)

    def test_component_split_component_and_options_errors(self):
        self.do_split_error('"component')
        self.do_split_error('comp"onent')
        self.do_split_error('component"')
        self.do_split_error('"component"')
        self.do_split_error("=component")
        self.do_split_error("comp=onent")
        self.do_split_error("component=")
        self.do_split_error('key="val"')
        self.do_split_error("component key=")
        self.do_split_error('component key="val')
        self.do_split_error('component key=val"')
        self.do_split_error('component key="val" x')
        self.do_split_error('component x key="val"')
        self.do_split_error('component key1="val" x key2="val"')

    def do_from_string(self, string, name, destdir=""):
        component = Component.from_string(string)
        self.assertEqual(name, component.name)
        self.assertEqual(destdir, component.destdir)

    def test_component_from_string(self):
        self.do_from_string("component", "component")
        self.do_from_string("component-with-hyphen", "component-with-hyphen")
        self.do_from_string('component destdir="foo/bar"', "component", "foo/bar")
        self.do_from_string('component destdir="bar spc"', "component", "bar spc")
        self.assertRaises(ErrorMessage, Component.from_string, "")
        self.assertRaises(ErrorMessage, Component.from_string, "component novalue=")
        self.assertRaises(
            ErrorMessage, Component.from_string, "component badoption=badvalue"
        )


if __name__ == "__main__":
    mozunit.main()
