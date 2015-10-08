#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Mozilla-specific signing methods.
"""

import os
import re

from mozharness.base.errors import BaseErrorList
from mozharness.base.log import ERROR, FATAL
from mozharness.base.signing import AndroidSigningMixin, BaseSigningMixin

AndroidSignatureVerificationErrorList = BaseErrorList + [{
    "regex": re.compile(r'''^Invalid$'''),
    "level": FATAL,
    "explanation": "Signature is invalid!"
}, {
    "substr": "filename not matched",
    "level": ERROR,
}, {
    "substr": "ERROR: Could not unzip",
    "level": ERROR,
}, {
    "regex": re.compile(r'''Are you sure this is a (nightly|release) package'''),
    "level": FATAL,
    "explanation": "Not signed!"
}]


# SigningMixin {{{1

class SigningMixin(BaseSigningMixin):
    """Generic signing helper methods."""
    def query_moz_sign_cmd(self, formats=['gpg']):
        if 'MOZ_SIGNING_SERVERS' not in os.environ:
            self.fatal("MOZ_SIGNING_SERVERS not in env; no MOZ_SIGN_CMD for you!")
        dirs = self.query_abs_dirs()
        signing_dir = os.path.join(dirs['abs_work_dir'], 'tools', 'release', 'signing')
        cache_dir = os.path.join(dirs['abs_work_dir'], 'signing_cache')
        token = os.path.join(dirs['base_work_dir'], 'token')
        nonce = os.path.join(dirs['base_work_dir'], 'nonce')
        host_cert = os.path.join(signing_dir, 'host.cert')
        python = self.query_exe('python')
        cmd = [
            python,
            os.path.join(signing_dir, 'signtool.py'),
            '--cachedir', cache_dir,
            '-t', token,
            '-n', nonce,
            '-c', host_cert,
        ]
        if formats:
            for f in formats:
                cmd += ['-f', f]
        for h in os.environ['MOZ_SIGNING_SERVERS'].split(","):
            cmd += ['-H', h]
        return cmd


# MobileSigningMixin {{{1
class MobileSigningMixin(AndroidSigningMixin, SigningMixin):
    def verify_android_signature(self, apk, script=None, key_alias="nightly",
                                 tools_dir="tools/", env=None):
        """Runs mjessome's android signature verification script.
        This currently doesn't check to see if the apk exists; you may want
        to do that before calling the method.
        """
        c = self.config
        dirs = self.query_abs_dirs()
        if script is None:
            script = c.get('signature_verification_script')
        if env is None:
            env = self.query_env()
        return self.run_command(
            [script, "--tools-dir=%s" % tools_dir, "--%s" % key_alias,
             "--apk=%s" % apk],
            cwd=dirs['abs_work_dir'],
            env=env,
            error_list=AndroidSignatureVerificationErrorList
        )
