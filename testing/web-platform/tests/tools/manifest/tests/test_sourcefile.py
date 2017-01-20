import pytest

from ..sourcefile import SourceFile

def create(filename, contents=b""):
    assert isinstance(contents, bytes)
    return SourceFile("/", filename, "/", contents=contents)


def items(s):
    item_type, items = s.manifest_items()
    if item_type == "support":
        return []
    else:
        return [(item_type, item.url) for item in items]


@pytest.mark.parametrize("rel_path", [
    ".gitignore",
    ".travis.yml",
    "MANIFEST.json",
    "tools/test.html",
    "resources/test.html",
    "common/test.html",
    "support/test.html",
    "css21/archive/test.html",
    "work-in-progress/test.html",
    "conformance-checkers/test.html",
    "conformance-checkers/README.md",
    "conformance-checkers/html/Makefile",
    "conformance-checkers/html/test.html",
    "foo/tools/test.html",
    "foo/resources/test.html",
    "foo/support/test.html",
])
def test_name_is_non_test(rel_path):
    s = create(rel_path)
    assert s.name_is_non_test or s.name_is_conformance_support

    assert not s.content_is_testharness

    assert items(s) == []


@pytest.mark.parametrize("rel_path", [
    "foo/common/test.html",
    "foo/conformance-checkers/test.html",
    "foo/_certs/test.html",
    "foo/css21/archive/test.html",
    "foo/work-in-progress/test.html",
])
def test_not_name_is_non_test(rel_path):
    s = create(rel_path)
    assert not (s.name_is_non_test or s.name_is_conformance_support)
    # We aren't actually asserting what type of test these are, just their
    # name doesn't prohibit them from being tests.


@pytest.mark.parametrize("rel_path", [
    "html/test-manual.html",
    "html/test-manual.xhtml",
    "html/test-manual.https.html",
    "html/test-manual.https.xhtml"
])
def test_name_is_manual(rel_path):
    s = create(rel_path)
    assert not s.name_is_non_test
    assert s.name_is_manual

    assert not s.content_is_testharness

    assert items(s) == [("manual", "/" + rel_path)]


@pytest.mark.parametrize("rel_path", [
    "html/test-visual.html",
    "html/test-visual.xhtml",
])
def test_name_is_visual(rel_path):
    s = create(rel_path)
    assert not s.name_is_non_test
    assert s.name_is_visual

    assert not s.content_is_testharness

    assert items(s) == [("visual", "/" + rel_path)]


@pytest.mark.parametrize("rel_path", [
    "css-namespaces-3/reftest/ref-lime-1.xml",
    "css21/reference/pass_if_box_ahem.html",
    "css21/csswg-issues/submitted/css2.1/reference/ref-green-box-100x100.xht",
    "selectors-3/selectors-empty-001-ref.xml",
    "css21/text/text-indent-wrap-001-notref-block-margin.xht",
    "css21/text/text-indent-wrap-001-notref-block-margin.xht",
    "css21/css-e-notation-ref-1.html",
    "css21/floats/floats-placement-vertical-004-ref2.xht",
    "css21/box/rtl-linebreak-notref1.xht",
    "css21/box/rtl-linebreak-notref2.xht",
    "2dcontext/drawing-images-to-the-canvas/drawimage_html_image_5_ref.html",
    "2dcontext/line-styles/lineto_ref.html",
    "html/rendering/non-replaced-elements/the-fieldset-element-0/ref.html"
])
def test_name_is_reference(rel_path):
    s = create(rel_path)
    assert not s.name_is_non_test
    assert s.name_is_reference

    assert not s.content_is_testharness

    assert items(s) == []


def test_worker():
    s = create("html/test.worker.js")
    assert not s.name_is_non_test
    assert not s.name_is_manual
    assert not s.name_is_visual
    assert not s.name_is_multi_global
    assert s.name_is_worker
    assert not s.name_is_reference

    assert not s.content_is_testharness

    assert items(s) == [("testharness", "/html/test.worker.html")]

def test_worker_long_timeout():
    s = create("html/test.worker.js",
               contents="""// <meta> timeout=long
importScripts('/resources/testharnes.js')
test()""")

    manifest_items = s.manifest_items()
    assert len(manifest_items) == 1
    assert manifest_items[0].timeout == "long"


def test_multi_global():
    s = create("html/test.any.js")
    assert not s.name_is_non_test
    assert not s.name_is_manual
    assert not s.name_is_visual
    assert s.name_is_multi_global
    assert not s.name_is_worker
    assert not s.name_is_reference

    assert not s.content_is_testharness

    assert items(s) == [
        ("testharness", "/html/test.any.html"),
        ("testharness", "/html/test.any.worker.html"),
    ]


@pytest.mark.parametrize("ext", ["htm", "html"])
def test_testharness(ext):
    content = b"<script src=/resources/testharness.js></script>"

    filename = "html/test." + ext
    s = create(filename, content)

    assert not s.name_is_non_test
    assert not s.name_is_manual
    assert not s.name_is_visual
    assert not s.name_is_multi_global
    assert not s.name_is_worker
    assert not s.name_is_reference

    assert s.content_is_testharness

    assert items(s) == [("testharness", "/" + filename)]


@pytest.mark.parametrize("ext", ["htm", "html"])
def test_relative_testharness(ext):
    content = b"<script src=../resources/testharness.js></script>"

    filename = "html/test." + ext
    s = create(filename, content)

    assert not s.name_is_non_test
    assert not s.name_is_manual
    assert not s.name_is_visual
    assert not s.name_is_multi_global
    assert not s.name_is_worker
    assert not s.name_is_reference

    assert not s.content_is_testharness

    assert items(s) == []


@pytest.mark.parametrize("ext", ["xhtml", "xht", "xml"])
def test_testharness_xhtml(ext):
    content = b"""
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<body/>
</html>
"""

    filename = "html/test." + ext
    s = create(filename, content)

    assert not s.name_is_non_test
    assert not s.name_is_manual
    assert not s.name_is_visual
    assert not s.name_is_multi_global
    assert not s.name_is_worker
    assert not s.name_is_reference

    assert s.content_is_testharness

    assert items(s) == [("testharness", "/" + filename)]


@pytest.mark.parametrize("ext", ["xhtml", "xht", "xml"])
def test_relative_testharness_xhtml(ext):
    content = b"""
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<script src="../resources/testharness.js"></script>
<script src="../resources/testharnessreport.js"></script>
</head>
<body/>
</html>
"""

    filename = "html/test." + ext
    s = create(filename, content)

    assert not s.name_is_non_test
    assert not s.name_is_manual
    assert not s.name_is_visual
    assert not s.name_is_multi_global
    assert not s.name_is_worker
    assert not s.name_is_reference

    assert not s.content_is_testharness

    assert items(s) == []


def test_testharness_svg():
    content = b"""\
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg"
     xmlns:h="http://www.w3.org/1999/xhtml"
     version="1.1"
     width="100%" height="100%" viewBox="0 0 400 400">
<title>Null test</title>
<h:script src="/resources/testharness.js"/>
<h:script src="/resources/testharnessreport.js"/>
</svg>
"""

    filename = "html/test.svg"
    s = create(filename, content)

    assert not s.name_is_non_test
    assert not s.name_is_manual
    assert not s.name_is_visual
    assert not s.name_is_multi_global
    assert not s.name_is_worker
    assert not s.name_is_reference

    assert s.root
    assert s.content_is_testharness

    assert items(s) == [("testharness", "/" + filename)]


def test_relative_testharness_svg():
    content = b"""\
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg"
     xmlns:h="http://www.w3.org/1999/xhtml"
     version="1.1"
     width="100%" height="100%" viewBox="0 0 400 400">
<title>Null test</title>
<h:script src="../resources/testharness.js"/>
<h:script src="../resources/testharnessreport.js"/>
</svg>
"""

    filename = "html/test.svg"
    s = create(filename, content)

    assert not s.name_is_non_test
    assert not s.name_is_manual
    assert not s.name_is_visual
    assert not s.name_is_multi_global
    assert not s.name_is_worker
    assert not s.name_is_reference

    assert s.root
    assert not s.content_is_testharness

    assert items(s) == []


@pytest.mark.parametrize("filename", ["test", "test.test"])
def test_testharness_ext(filename):
    content = b"<script src=/resources/testharness.js></script>"

    s = create("html/" + filename, content)

    assert not s.name_is_non_test
    assert not s.name_is_manual
    assert not s.name_is_visual
    assert not s.name_is_multi_global
    assert not s.name_is_worker
    assert not s.name_is_reference

    assert not s.root
    assert not s.content_is_testharness

    assert items(s) == []


@pytest.mark.parametrize("ext", ["htm", "html"])
def test_reftest_node(ext):
    content = b"<link rel=match href=ref.html>"

    filename = "foo/test." + ext
    s = create(filename, content)

    assert not s.name_is_non_test
    assert not s.name_is_manual
    assert not s.name_is_visual
    assert not s.name_is_multi_global
    assert not s.name_is_worker
    assert not s.name_is_reference
    assert not s.content_is_testharness

    assert s.content_is_ref_node

    assert items(s) == [("reftest_node", "/" + filename)]


@pytest.mark.parametrize("ext", ["xht", "html", "xhtml", "htm", "xml", "svg"])
def test_css_visual(ext):
    content = b"""
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<link rel="help" href="http://www.w3.org/TR/CSS21/box.html#bidi-box-model"/>
</head>
<body></body>
</html>
"""

    filename = "html/test." + ext
    s = create(filename, content)

    assert not s.name_is_non_test
    assert not s.name_is_manual
    assert not s.name_is_visual
    assert not s.name_is_multi_global
    assert not s.name_is_worker
    assert not s.name_is_reference
    assert not s.content_is_testharness
    assert not s.content_is_ref_node

    assert s.content_is_css_visual

    assert items(s) == [("visual", "/" + filename)]


@pytest.mark.parametrize("ext", ["xht", "xhtml", "xml"])
def test_xhtml_with_entity(ext):
    content = b"""
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
   "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
&nbsp;
</html>
"""

    filename = "html/test." + ext
    s = create(filename, content)

    assert s.root is not None

    assert items(s) == []
