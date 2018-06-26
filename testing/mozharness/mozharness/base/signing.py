#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Generic signing methods.
"""

import getpass
import hashlib
import os
import re
import subprocess

from mozharness.base.errors import JarsignerErrorList, ZipErrorList, ZipalignErrorList
from mozharness.base.log import OutputParser, IGNORE, DEBUG, INFO, ERROR, FATAL

UnsignApkErrorList = [{
    'regex': re.compile(r'''zip warning: name not matched: '?META-INF/'''),
    'level': INFO,
    'explanation': r'''This apk is already unsigned.''',
}, {
    'substr': r'''zip error: Nothing to do!''',
    'level': IGNORE,
}] + ZipErrorList

TestJarsignerErrorList = [{
    "substr": "jarsigner: unable to open jar file:",
    "level": IGNORE,
}] + JarsignerErrorList


# BaseSigningMixin {{{1
class BaseSigningMixin(object):
    """Generic signing helper methods.
    """
    def query_filesize(self, file_path):
        self.info("Determining filesize for %s" % file_path)
        length = os.path.getsize(file_path)
        self.info(" %s" % str(length))
        return length

    # TODO this should be parallelized with the to-be-written BaseHelper!
    def query_sha512sum(self, file_path):
        self.info("Determining sha512sum for %s" % file_path)
        m = hashlib.sha512()
        contents = self.read_from_file(file_path, verbose=False,
                                       open_mode='rb')
        m.update(contents)
        sha512 = m.hexdigest()
        self.info(" %s" % sha512)
        return sha512


# AndroidSigningMixin {{{1
class AndroidSigningMixin(object):
    """
    Generic Android apk signing methods.

    Dependent on BaseScript.
    """
    # TODO port build/tools/release/signing/verify-android-signature.sh here

    key_passphrase = os.environ.get('android_keypass')
    store_passphrase = os.environ.get('android_storepass')

    def passphrase(self):
        if not self.store_passphrase:
            self.store_passphrase = getpass.getpass("Store passphrase: ")
        if not self.key_passphrase:
            self.key_passphrase = getpass.getpass("Key passphrase: ")

    def _verify_passphrases(self, keystore, key_alias, error_level=FATAL):
        self.info("Verifying passphrases...")
        status = self.sign_apk("NOTAREALAPK", keystore,
                               self.store_passphrase, self.key_passphrase,
                               key_alias, remove_signature=False,
                               log_level=DEBUG, error_level=DEBUG,
                               error_list=TestJarsignerErrorList)
        if status == 0:
            self.info("Passphrases are good.")
        elif status < 0:
            self.log("Encountered errors while trying to sign!",
                     level=error_level)
        else:
            self.log("Unable to verify passphrases!",
                     level=error_level)
        return status

    def verify_passphrases(self):
        c = self.config
        self._verify_passphrases(c['keystore'], c['key_alias'])

    def postflight_passphrase(self):
        self.verify_passphrases()

    def sign_apk(self, apk, keystore, storepass, keypass, key_alias,
                 remove_signature=True, error_list=None,
                 log_level=INFO, error_level=ERROR):
        """
        Signs an apk with jarsigner.
        """
        jarsigner = self.query_exe('jarsigner')
        if remove_signature:
            status = self.unsign_apk(apk)
            if status:
                self.error("Can't remove signature in %s!" % apk)
                return -1
        if error_list is None:
            error_list = JarsignerErrorList[:]
        # This needs to run silently, so no run_command() or
        # get_output_from_command() (though I could add a
        # suppress_command_echo=True or something?)
        self.log("(signing %s)" % apk, level=log_level)
        try:
            p = subprocess.Popen([jarsigner, "-keystore", keystore,
                                 "-storepass", storepass,
                                 "-keypass", keypass,
                                 apk, key_alias],
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT,
                                 bufsize=0)
        except OSError:
            self.exception("Error while signing %s (missing %s?):" % (apk, jarsigner))
            return -2
        except ValueError:
            self.exception("Popen called with invalid arguments during signing?")
            return -3
        parser = OutputParser(config=self.config, log_obj=self.log_obj,
                              error_list=error_list)
        loop = True
        while loop:
            if p.poll() is not None:
                """Avoid losing the final lines of the log?"""
                loop = False
            for line in p.stdout:
                parser.add_lines(line)
        if parser.num_errors:
            self.log("(failure)", level=error_level)
        else:
            self.log("(success)", level=log_level)
        return parser.num_errors

    def unsign_apk(self, apk, **kwargs):
        zip_bin = self.query_exe("zip")
        return self.run_command([zip_bin, apk, '-d', 'META-INF/*'],
                                error_list=UnsignApkErrorList,
                                success_codes=[0, 12],
                                return_type='num_errors', **kwargs)

    def align_apk(self, unaligned_apk, aligned_apk, error_level=ERROR):
        """
        Zipalign apk.
        Returns None on success, not None on failure.
        """
        dirs = self.query_abs_dirs()
        zipalign = self.query_exe("zipalign")
        if self.run_command([zipalign, '-f', '4',
                             unaligned_apk, aligned_apk],
                            return_type='num_errors',
                            cwd=dirs['abs_work_dir'],
                            error_list=ZipalignErrorList):
            self.log("Unable to zipalign %s to %s!" % (unaligned_apk, aligned_apk), level=error_level)
            return -1
