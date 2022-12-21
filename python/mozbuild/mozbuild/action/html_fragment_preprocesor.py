import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

JS_FILE_TEMPLATE = """\
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["getHTMLFragment"];

const Fragments = {json_string};

/*
 * Loads HTML fragment strings pulled from fragment documents.
 * @param key - key identifying HTML fragment
 *
 * @return raw HTML/XHTML string
 */
const getHTMLFragment = key => Fragments[key];
"""

RE_COLLAPSE_WHITESPACE = re.compile(r"\s+")


def get_fragment_key(path, template_name=None):
    key = Path(path).stem
    if template_name:
        key += "/" + template_name
    return key


def fill_html_fragments_map(fragment_map, path, template, doctype=None):
    # collape white space
    for elm in template.iter():
        if elm.text:
            elm.text = RE_COLLAPSE_WHITESPACE.sub(" ", elm.text)
        if elm.tail:
            elm.tail = RE_COLLAPSE_WHITESPACE.sub(" ", elm.tail)
    key = get_fragment_key(path, template.attrib.get("name"))
    xml = "".join(ET.tostring(elm, encoding="unicode") for elm in template).strip()
    if doctype:
        xml = doctype + "\n" + xml
    fragment_map[key] = xml


def get_html_fragments_from_file(fragment_map, path):
    for _, (name, value) in ET.iterparse(path, events=["start-ns"]):
        ET.register_namespace(name, value)
    tree = ET.parse(path)
    root = tree.getroot()
    sub_templates = root.findall("{http://www.w3.org/1999/xhtml}template")
    # if all nested nodes are templates then treat as list of templates
    if len(sub_templates) == len(root):
        doctype = ""
        for template in sub_templates:
            if template.get("doctype") == "true":
                doctype = template.text.strip()
                break
        for template in sub_templates:
            if template.get("doctype") != "true":
                fill_html_fragments_map(fragment_map, path, template, doctype)
    else:
        fill_html_fragments_map(fragment_map, path, root, None)


def generate(output, *inputs):
    """Builds an html fragments loader JS file from the input xml file(s)

    The xml files are expected to be in the format of:
    `<template>...xhtml markup...</template>`

    or `<template><template name="fragment_name">...xhtml markup...</template>...</template>`
    Where there are multiple templates. All markup is expected to be properly namespaced.

    In the JS file, calling getHTMLFragment(key) will return the HTML string from the xml file
    that matches the key.

    The key format is `filename_without_extension/template_name` for files with
    multiple templates, or just `filename_without_extension` for files with one template.
    `filename_without_extension` is the xml filename without the .xml extension
    and `template_name` is the name attribute of template node containing the xml fragment.

    Arguments:
    output -- File handle to JS file being generated
    inputs -- list of xml filenames to include in loader

    Returns:
    The set of dependencies which should trigger this command to be re-run.
    This is ultimately returned to the build system for use by the backend
    to ensure that incremental rebuilds happen when any dependency changes.
    """

    fragment_map = {}
    for file in inputs:
        get_html_fragments_from_file(fragment_map, file)
    json_string = json.dumps(fragment_map, separators=(",", ":"))
    contents = JS_FILE_TEMPLATE.format(json_string=json_string)
    output.write(contents)
    return set(inputs)
