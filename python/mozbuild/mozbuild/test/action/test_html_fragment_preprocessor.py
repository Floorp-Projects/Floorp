import os
import re
import unittest
import mozunit
import xml.etree.ElementTree as ET

from mozbuild.action.html_fragment_preprocesor import (
    generate,
    get_fragment_key,
    get_html_fragments_from_file,
    fill_html_fragments_map,
)
import mozpack.path as mozpath

test_data_path = mozpath.abspath(mozpath.dirname(__file__))
test_data_path = mozpath.join(test_data_path, "data", "html_fragment_preprocesor")


def data(name):
    return os.path.join(test_data_path, name)


TEST_PATH = "/some/path/somewhere/example.xml".replace("/", os.sep)
EXAMPLE_BASIC = data("example_basic.xml")
EXAMPLE_TEMPLATES = data("example_multiple_templates.xml")
EXAMPLE_XUL = data("example_xul.xml")
DUMMY_FILE = data("dummy.js")


def strip_white_space(xml):
    return re.sub(r">\s+", ">", re.sub(r"\s+<", "<", xml))


class TestNode(unittest.TestCase):
    """
    Tests for html_fragment_preprocesor.py.
    """

    maxDiff = None

    def test_get_fragment_key_path(self):
        key = get_fragment_key("/some/path/somewhere/example.xml")

        self.assertEqual(key, "example")

    def test_get_fragment_key_with_named_template(self):
        key = get_fragment_key(TEST_PATH, "some-template")

        self.assertEqual(key, "example/some-template")

    def test_get_html_fragments_from_template_no_doctype_no_name(self):
        key = "example"
        fragment_map = {}
        template = ET.Element("template")
        p1 = ET.SubElement(template, "p")
        p1.text = "Hello World"
        p2 = ET.SubElement(template, "p")
        p2.text = "Goodbye"
        fill_html_fragments_map(fragment_map, TEST_PATH, template)
        self.assertEqual(fragment_map[key], "<p>Hello World</p><p>Goodbye</p>")

    def test_get_html_fragments_from_named_template_with_html_element(self):
        key = "example/some-template"
        fragment_map = {}
        template = ET.Element("template")
        template.attrib["name"] = "some-template"
        p = ET.SubElement(template, "p")
        p.text = "Hello World"
        fill_html_fragments_map(fragment_map, TEST_PATH, template)
        self.assertEqual(fragment_map[key], "<p>Hello World</p>")

    def test_get_html_fragments_from_template_with_doctype(self):
        key = "example"
        doctype = "doctype definition goes here"
        fragment_map = {}
        template = ET.Element("template")
        p = ET.SubElement(template, "p")
        p.text = "Hello World"
        fill_html_fragments_map(fragment_map, TEST_PATH, template, doctype)
        self.assertEqual(
            fragment_map[key], "doctype definition goes here\n<p>Hello World</p>"
        )

    def test_get_html_fragments_from_file_basic(self):
        key = "example_basic"
        fragment_map = {}
        get_html_fragments_from_file(fragment_map, EXAMPLE_BASIC)
        xml_without_whitespace = strip_white_space(fragment_map[key])
        self.assertEqual(
            xml_without_whitespace,
            '<div xmlns="http://www.w3.org/1999/xhtml" class="main">'
            + "<p>Hello World</p></div>",
        )

    def test_get_html_fragments_from_file_multiple_templates(self):
        key1 = "example_multiple_templates/alpha"
        key2 = "example_multiple_templates/beta"
        key3 = "example_multiple_templates/charlie"
        fragment_map = {}
        get_html_fragments_from_file(fragment_map, EXAMPLE_TEMPLATES)
        self.assertIn("<p>Hello World</p>", fragment_map[key1], "Has HTML content")
        self.assertIn(
            '<!ENTITY % exampleDTD SYSTEM "chrome://global/locale/example.dtd">',
            fragment_map[key1],
            "Has doctype",
        )
        self.assertIn("<p>Lorem ipsum", fragment_map[key2], "Has HTML content")
        self.assertIn(
            '<!ENTITY % exampleDTD SYSTEM "chrome://global/locale/example.dtd">',
            fragment_map[key2],
            "Has doctype",
        )
        self.assertIn("<p>Goodbye</p>", fragment_map[key3], "Has HTML content")
        self.assertIn(
            '<!ENTITY % exampleDTD SYSTEM "chrome://global/locale/example.dtd">',
            fragment_map[key3],
            "Has doctype",
        )

    def test_get_html_fragments_from_file_with_xul(self):
        key = "example_xul"
        fragment_map = {}
        get_html_fragments_from_file(fragment_map, EXAMPLE_XUL)
        xml_without_whitespace = strip_white_space(fragment_map[key])
        self.assertEqual(
            xml_without_whitespace,
            '<html:link xmlns:html="http://www.w3.org/1999/xhtml" '
            + 'href="chrome://global/skin/example.css" rel="stylesheet" />'
            + '<hbox xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"'
            + ' id="label-box" part="label-box" flex="1" role="none">'
            + '<image part="icon" role="none" />'
            + '<label id="label" part="label" crop="right" flex="1" role="none" />'
            + '<label id="highlightable-label" part="label" crop="right" flex="1" role="none" />'
            + "</hbox>"
            + '<html:slot xmlns:html="http://www.w3.org/1999/xhtml" />',
        )

    def test_generate(self):
        with open(DUMMY_FILE, "w") as file:
            deps = generate(
                file,
                EXAMPLE_BASIC,
                EXAMPLE_TEMPLATES,
                EXAMPLE_XUL,
            )
        with open(DUMMY_FILE, "r") as file:
            contents = file.read()
            self.assertIn(
                "<!ENTITY % exampleDTD SYSTEM",
                contents,
                "Has doctype",
            )
            self.assertIn("<p>Lorem ipsum", contents, "Has HTML content")
            self.assertIn('"example_basic"', contents, "Has basic fragment key")
            self.assertIn(
                '"example_multiple_templates/alpha"',
                contents,
                "Has multiple templates fragment key",
            )
            self.assertIn('"example_xul"', contents, "Has XUL fragment key")
            self.assertIn(
                "const getHTMLFragment =",
                contents,
                "Has fragment loader method declaration",
            )
        os.remove(DUMMY_FILE)
        self.assertEqual(len(deps), 3, "deps are correct")
        self.assertIn(EXAMPLE_BASIC, deps, "deps are correct")
        self.assertIn(EXAMPLE_TEMPLATES, deps, "deps are correct")
        self.assertIn(EXAMPLE_XUL, deps, "deps are correct")


if __name__ == "__main__":
    mozunit.main()
