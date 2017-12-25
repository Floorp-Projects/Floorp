#!/usr/bin/python
# -*- coding: utf-8 -*-
#  This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Check imported locales with release locales."""

from os import path, listdir
from sys import exit
from locales import SCREENSHOT_LOCALES

PATH = path.join(path.dirname(path.abspath(__file__)), '../../app/src/main/res/')
IGNORED_DIRECTORIES = ['sw600dp']

def check_locales():
    print("Checking for imported locales...")
    got_error = False
    imported_locales = [
        directory[7:].replace('-r', '-') for directory in listdir(PATH) if directory.startswith('values-')
    ]
    for ignored in IGNORED_DIRECTORIES:
        try:
            imported_locales.remove(ignored)
        except ValueError:
            pass
    for locale in imported_locales:
        if locale not in SCREENSHOT_LOCALES:
            print("Error: * [values-{locale}] missing in SCREENSHOT_LOCALES".format(locale=locale))
            got_error = True
    for locale in SCREENSHOT_LOCALES:
        if locale not in imported_locales:
            print("Warning: * [{locale}] missing in imported locales".format(locale=locale))

    if got_error:
        exit(1)


if __name__ == '__main__':
    check_locales()
