#!/usr/bin/python
# -*- coding: utf-8 -*-
#  This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Check translation files for missing or wrong number of placeholders."""

from os import path, walk
from sys import exit
import xml.etree.ElementTree as ET


def etree_to_dict(tree):
    d = {}
    for element in tree.getchildren():
        if 'name' in element.attrib:
            d[element.attrib['name']] = element.text
        else:
            d.update(etree_to_dict(element))
    return d


def raise_exception(lang, code):
    print(" * [{lang}/strings.xml] missing placeholder in translation, key: {code}".format(
        lang=lang,
        code=code
    ))

PATH = path.join(path.dirname(path.abspath(__file__)), '../../app/src/main/res/')
files = []

# Make list of all locale xml files
for directory, directories, file_names in walk(PATH):
    for file_name in file_names:
        if file_name == "strings.xml" and 'values-' in directory:
            files.append(
                path.join(directory, file_name)
            )
# Read source file
source_xml = ET.parse(path.join(PATH, 'values/strings.xml'))
source = etree_to_dict(source_xml.getroot())

got_error = False

for source_xml in files:
    target = etree_to_dict(ET.parse(source_xml).getroot())
    print("Checking for placeholders in {lang}".format(lang=source_xml.split('/')[-2].replace('values-', '')))
    for key in source:
        # pass missing translations and empty strings
        if key not in target:
            continue
        if not target[key]:
            continue
        if not source[key]:
            continue
        # Check for placeholders
        for placeholder in ['%1$s', '%2$s', '%3$s', '%4$s', '%5$s']:
            if placeholder in source[key] and \
                    source[key].count(placeholder) != target[key].count(placeholder):
                raise_exception(source_xml.split('/')[-2], key)
                got_error = True

if got_error:
    exit(1)
