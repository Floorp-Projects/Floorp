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


def missing_target_exception(lang, code):
    # there is placeholder in source, but not in target.
    print("Error: * [{lang}/strings.xml] missing placeholder in translation, key: {code}".format(
        lang=lang,
        code=code
    ))

def missing_source_exception(lang, code):
    # there is placeholder in target, but not in source.
    print("Error:  * [{lang}/strings.xml] placeholder missing in source, key: {code}".format(
        lang=lang,
        code=code
    ))

def count_mismatch_warning(lang, code):
    # number of same placeholders used in source and target do not match
    print("Warning: * [{lang}/strings.xml] number of placeholders not matching, key: {code}".format(
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

print("Checking for placeholders in translation files...")

for source_xml in files:
    target = etree_to_dict(ET.parse(source_xml).getroot())
    for key in source:
        # pass missing translations and empty strings
        if key not in target:
            continue
        if not target[key]:
            continue
        if not source[key]:
            continue
        # Check for placeholders
        language = source_xml.split('/')[-2]
        for placeholder in ['%1$s', '%2$s', '%3$s', '%4$s', '%5$s']:
            if placeholder in source[key] and placeholder not in target[key]:
                missing_target_exception(language, key)
                got_error = True
            elif placeholder in target[key] and placeholder not in source[key]:
                missing_source_exception(language, key)
                got_error = True
            elif source[key].count(placeholder) != target[key].count(placeholder):
                count_mismatch_warning(language, key)

if got_error:
    exit(1)
