#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import os

from mozharness.base.python import VirtualenvMixin
from mozharness.base.script import PostScriptRun

blobupload_config_options = [
    [["--blob-upload-branch"],
    {"dest": "blob_upload_branch",
     "help": "Branch for blob server's metadata",
    }],
    [["--blob-upload-server"],
    {"dest": "blob_upload_servers",
     "action": "extend",
     "help": "Blob servers's location",
    }]
    ]


class BlobUploadMixin(VirtualenvMixin):
    """Provides mechanism to automatically upload files written in
    MOZ_UPLOAD_DIR to the blobber upload server at the end of the
    running script.

    This is dependent on ScriptMixin and BuildbotMixin.
    The testing script inheriting this class is to specify as cmdline
    options the <blob-upload-branch> and <blob-upload-server>

    """
    def __init__(self, *args, **kwargs):
        requirements = [
            'blobuploader==1.2.4',
        ]
        super(BlobUploadMixin, self).__init__(*args, **kwargs)
        for req in requirements:
            self.register_virtualenv_module(req, method='pip')

    def upload_blobber_files(self):
        self.debug("Check branch and server cmdline options.")
        if self.config.get('blob_upload_branch') and \
            (self.config.get('blob_upload_servers') or
             self.config.get('default_blob_upload_servers')) and \
                 self.config.get('blob_uploader_auth_file'):

            self.info("Blob upload gear active.")
            upload = [self.query_python_path(), self.query_python_path("blobberc.py")]

            dirs = self.query_abs_dirs()
            self.debug("Get the directory from which to upload the files.")
            if dirs.get('abs_blob_upload_dir'):
                blob_dir = dirs['abs_blob_upload_dir']
            else:
                self.warning("Couldn't find the blob upload folder's path!")
                return

            if not os.path.isdir(blob_dir):
                self.warning("Blob upload directory does not exist!")
                return

            if not os.listdir(blob_dir):
                self.info("There are no files to upload in the directory. "
                          "Skipping the blob upload mechanism ...")
                return

            self.info("Preparing to upload files from %s." % blob_dir)
            auth_file = self.config.get('blob_uploader_auth_file')
            if not os.path.isfile(auth_file):
                self.warning("Could not find the credentials files!")
                return
            blob_branch = self.config.get('blob_upload_branch')
            blob_servers_list = self.config.get('blob_upload_servers',
                               self.config.get('default_blob_upload_servers'))

            servers = []
            for server in blob_servers_list:
                servers.extend(['-u', server])
            auth = ['-a', auth_file]
            branch = ['-b', blob_branch]
            dir_to_upload = ['-d', blob_dir]
            # We want blobberc to tell us if a summary file was uploaded through this manifest file
            manifest_path = os.path.join(dirs['abs_work_dir'], 'uploaded_files.json')
            record_uploaded_files = ['--output-manifest', manifest_path]
            self.info("Files from %s are to be uploaded with <%s> branch at "
                      "the following location(s): %s" % (blob_dir, blob_branch,
                      ", ".join(["%s" % s for s in blob_servers_list])))

            # call blob client to upload files to server
            self.run_command(upload + servers + auth + branch + dir_to_upload + record_uploaded_files)

            uploaded_files = '{}'
            if os.path.isfile(manifest_path):
                with open(manifest_path, 'r') as f:
                    uploaded_files = f.read()
                self.rmtree(manifest_path)

            self.set_buildbot_property(prop_name='blobber_files',
                    prop_value=uploaded_files, write_to_file=True)
        else:
            self.warning("Blob upload gear skipped. Missing cmdline options.")

    @PostScriptRun
    def _upload_blobber_files(self):
        self.upload_blobber_files()
