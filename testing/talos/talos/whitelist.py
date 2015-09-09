# -*- Mode: python; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# vim: set ts=8 sts=4 et sw=4 tw=80:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import utils

KEY_XRE = '{xre}'
DEFAULT_DURATION = 100.0


class Whitelist:
    # we need to find the root dir of the profile at runtime
    PRE_PROFILE = ''

    def __init__(self, test_name, paths, path_substitutions,
                 name_substitutions, init_with=None):
        self.test_name = test_name
        self.listmap = init_with if init_with else {}
        self.dependent_libs = self.load_dependent_libs() \
            if init_with and KEY_XRE in paths else {}
        self.paths = paths
        self.path_substitutions = path_substitutions
        self.name_substitutions = name_substitutions

    def load(self, filename):
        if not self.load_dependent_libs():
            return False

        try:
            with open(filename, 'r') as fHandle:
                temp = json.load(fHandle)

            for whitelist_name in temp:
                self.listmap[whitelist_name.lower()] = temp[whitelist_name]

        except IOError as e:
            print "%s: %s" % (e.filename, e.strerror)
            return False
        return True

    def sanitize_filename(self, filename):
        filename = filename.lower()
        filename.replace(' (x86)', '')

        for path, subst in self.path_substitutions.iteritems():
            parts = filename.split(path)
            if len(parts) >= 2:
                if self.PRE_PROFILE == '' and subst == '{profile}':
                    fname = self.sanitize_filename(parts[0])
                    self.listmap[fname] = {}
                    # Windows can have {appdata}\local\temp\longnamedfolder
                    # or {appdata}\local\temp\longna~1
                    self.listmap[fname] = {}
                    if not fname.endswith('~1'):
                        # parse the longname into longna~1
                        dirs = fname.split('\\')
                        dirs[-1] = "%s~1" % (dirs[-1][:6])
                        # now we want to ensure that every parent dir is
                        # added since we seem to be accessing them sometimes
                        diter = 2
                        while (diter < len(dirs)):
                            self.listmap['\\'.join(dirs[:diter])] = {}
                            diter = diter + 1
                        self.PRE_PROFILE = fname

                filename = "%s%s" % (subst, path.join(parts[1:]))

        for old_name, new_name in self.name_substitutions.iteritems():
            parts = filename.split(old_name)
            if len(parts) >= 2:
                filename = "%s%s" % (parts[0], new_name)

        return filename.strip('/\\\ \t')

    def check(self, test, file_name_index):
        errors = {}
        for row_key in test.iterkeys():
            filename = self.sanitize_filename(row_key[file_name_index])

            if filename in self.listmap:
                if 'ignore' in self.listmap[filename] and \
                        self.listmap[filename]['ignore']:
                    continue
            elif filename in self.dependent_libs:
                continue
            else:
                if filename not in errors:
                    errors[filename] = []
                errors[filename].append(test[row_key])
        return errors

    def checkDuration(self, test, file_name_index, file_duration_index):
        errors = {}
        for idx, (row_key, row_value) in utils.indexed_items(test.iteritems()):
            if row_value[file_duration_index] > DEFAULT_DURATION:
                filename = self.sanitize_filename(row_key[file_name_index])
                if filename in self.listmap and \
                   'ignoreduration' in self.listmap[filename]:
                    # we have defined in the json manifest max values
                    # (max found value * 2) and will ignore it
                    if row_value[file_duration_index] <= \
                            self.listmap[filename]['ignoreduration']:
                        continue

                if filename not in errors:
                    errors[filename] = []
                errors[filename].append("Duration %s > %S"
                                        % (row_value[file_duration_index]),
                                        DEFAULT_DURATION)
        return errors

    def filter(self, test, file_name_index):
        for row_key in test.keys():
            filename = self.sanitize_filename(row_key[file_name_index])
            if filename in self.listmap:
                if 'ignore' in self.listmap[filename] and \
                        self.listmap[filename]['ignore']:
                    del test[row_key]
                    continue
            elif filename in self.dependent_libs:
                del test[row_key]
                continue

    @staticmethod
    def get_error_strings(errors):
        error_strs = []
        for filename, data in errors.iteritems():
            for datum in data:
                error_strs.append("File '%s' was accessed and we were not"
                                  " expecting it: %r" % (filename, datum))
        return error_strs

    def print_errors(self, error_strs):
        for error_msg in error_strs:
            print "TEST-UNEXPECTED-FAIL | %s | %s" % (self.test_name,
                                                      error_msg)

    # Note that we don't store dependent libs in listmap. This makes
    # save_baseline cleaner. Since a baseline whitelist should not include
    # the dependent_libs, we would need to filter them out if everything was
    # stored in the same dict.
    def load_dependent_libs(self):
        filename = "%s%sdependentlibs.list" % (self.paths[KEY_XRE],
                                               os.path.sep)
        try:
            with open(filename, 'r') as f:
                libs = f.readlines()
            self.dependent_libs = \
                {"%s%s%s" % (KEY_XRE, os.path.sep, lib.strip()):
                    {'ignore': True} for lib in libs}
            return True
        except IOError as e:
            print "%s: %s" % (e.filename, e.strerror)
            return False
