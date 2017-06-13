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
import json
import sys

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
        python = sys.executable
        # A mock environment is a special case, the system python isn't
        # available there
        if 'mock_target' in self.config:
            python = 'python2.7'
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

    def generate_signing_manifest(self, files):
        """Generate signing manifest for signingworkers

        Every entry in the manifest requires a dictionary of
        "file_to_sign" (basename) and "hash" (SHA512) of every file to be
        signed. Signing format is defined in the signing task.
        """
        manifest_content = [
            {
                "file_to_sign": os.path.basename(f),
                "hash": self.query_sha512sum(f)
            }
            for f in files
        ]
        return json.dumps(manifest_content)


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
