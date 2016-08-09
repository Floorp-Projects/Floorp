#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""beet_mover.py.

downloads artifacts, scans them and uploads them to s3
"""
import hashlib
import sys
import os
import pprint
import re
from os import listdir
from os.path import isfile, join
import sh
import redo

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))
from mozharness.base.log import FATAL
from mozharness.base.python import VirtualenvMixin
from mozharness.base.script import BaseScript
from mozharness.mozilla.aws import pop_aws_auth_from_env
import mozharness
import mimetypes


def get_hash(content, hash_type="md5"):
    h = hashlib.new(hash_type)
    h.update(content)
    return h.hexdigest()


CONFIG_OPTIONS = [
    [["--template"], {
        "dest": "template",
        "help": "Specify jinja2 template file",
    }],
    [['--locale', ], {
        "action": "extend",
        "dest": "locales",
        "type": "string",
        "help": "Specify the locale(s) to upload."}],
    [["--platform"], {
        "dest": "platform",
        "help": "Specify the platform of the build",
    }],
    [["--version"], {
        "dest": "version",
        "help": "full release version based on gecko and tag/stage identifier. e.g. '44.0b1'"
    }],
    [["--app-version"], {
        "dest": "app_version",
        "help": "numbered version based on gecko. e.g. '44.0'"
    }],
    [["--partial-version"], {
        "dest": "partial_version",
        "help": "the partial version the mar is based off of"
    }],
    [["--artifact-subdir"], {
        "dest": "artifact_subdir",
        "default": 'build',
        "help": "subdir location for taskcluster artifacts after public/ base.",
    }],
    [["--build-num"], {
        "dest": "build_num",
        "help": "the release build identifier"
    }],
    [["--taskid"], {
        "dest": "taskid",
        "help": "taskcluster task id to download artifacts from",
    }],
    [["--bucket"], {
        "dest": "bucket",
        "help": "s3 bucket to move beets to.",
    }],
    [["--exclude"], {
        "dest": "excludes",
        "action": "append",
        "help": "List of filename patterns to exclude. See script source for default",
    }],
    [["-s", "--scan-parallelization"], {
        "dest": "scan_parallelization",
        "default": 4,
        "type": "int",
        "help": "Number of concurrent file scans",
    }],
]

DEFAULT_EXCLUDES = [
    r"^.*tests.*$",
    r"^.*crashreporter.*$",
    r"^.*\.zip(\.asc)?$",
    r"^.*\.log$",
    r"^.*\.txt$",
    r"^.*\.asc$",
    r"^.*/partner-repacks.*$",
    r"^.*.checksums(\.asc)?$",
    r"^.*/logs/.*$",
    r"^.*/jsshell.*$",
    r"^.*json$",
    r"^.*/host.*$",
    r"^.*/mar-tools/.*$",
    r"^.*robocop.apk$",
    r"^.*contrib.*"
]
CACHE_DIR = 'cache'

MIME_MAP = {
    '': 'text/plain',
    '.asc': 'text/plain',
    '.beet': 'text/plain',
    '.bundle': 'application/octet-stream',
    '.bz2': 'application/octet-stream',
    '.checksums': 'text/plain',
    '.dmg': 'application/x-iso9660-image',
    '.mar': 'application/octet-stream',
    '.xpi': 'application/x-xpinstall'
}


class BeetMover(BaseScript, VirtualenvMixin, object):
    def __init__(self, aws_creds):
        beetmover_kwargs = {
            'config_options': CONFIG_OPTIONS,
            'all_actions': [
                # 'clobber',
                'create-virtualenv',
                'activate-virtualenv',
                'generate-candidates-manifest',
                'refresh-antivirus',
                'verify-bits',  # beets
                'download-bits', # beets
                'scan-bits',     # beets
                'upload-bits',  # beets
            ],
            'require_config_file': False,
            # Default configuration
            'config': {
                # base index url where to find taskcluster artifact based on taskid
                "artifact_base_url": 'https://queue.taskcluster.net/v1/task/{taskid}/artifacts/public/{subdir}',
                "virtualenv_modules": [
                    "boto",
                    "PyYAML",
                    "Jinja2",
                    "redo",
                    "mar",
                ],
                "virtualenv_path": "venv",
                'product': 'firefox',
            },
        }
        #todo do excludes need to be configured via command line for specific builds?
        super(BeetMover, self).__init__(**beetmover_kwargs)

        c = self.config
        self.manifest = {}
        # assigned in _post_create_virtualenv
        self.virtualenv_imports = None
        self.bucket = c['bucket']
        if not all(aws_creds):
            self.fatal('credentials must be passed in env: "AWS_ACCESS_KEY_ID", "AWS_SECRET_ACCESS_KEY"')
        self.aws_key_id, self.aws_secret_key = aws_creds
        # if excludes is set from command line, use it otherwise use defaults
        self.excludes = self.config.get('excludes', DEFAULT_EXCLUDES)
        dirs = self.query_abs_dirs()
        self.dest_dir = os.path.join(dirs['abs_work_dir'], CACHE_DIR)
        self.mime_fix()

    def activate_virtualenv(self):
        """
        activates virtualenv and adds module imports to a instance wide namespace.

        creating and activating a virtualenv onto the currently executing python interpreter is a
        bit black magic. Rather than having import statements added in various places within the
        script, we import them here immediately after we activate the newly created virtualenv
        """
        VirtualenvMixin.activate_virtualenv(self)

        import boto
        import yaml
        import jinja2
        self.virtualenv_imports = {
            'boto': boto,
            'yaml': yaml,
            'jinja2': jinja2,
        }
        self.log("activated virtualenv with the modules: {}".format(str(self.virtualenv_imports)))

    def _get_template_vars(self):
        return {
            "platform": self.config['platform'],
            "locales": self.config.get('locales'),
            "version": self.config['version'],
            "app_version": self.config.get('app_version', ''),
            "partial_version": self.config.get('partial_version', ''),
            "build_num": self.config['build_num'],
            # keep the trailing slash
            "s3_prefix": 'pub/{prod}/candidates/{ver}-candidates/{n}/'.format(
                prod=self.config['product'], ver=self.config['version'],
                n=self.config['build_num']
            ),
            "artifact_base_url": self.config['artifact_base_url'].format(
                    taskid=self.config['taskid'], subdir=self.config['artifact_subdir']
            )
        }

    def generate_candidates_manifest(self):
        """
        generates and outputs a manifest that maps expected Taskcluster artifact names
        to release deliverable names
        """
        self.log('generating manifest from {}...'.format(self.config['template']))
        template_dir, template_file = os.path.split(os.path.abspath(self.config['template']))
        jinja2 = self.virtualenv_imports['jinja2']
        yaml = self.virtualenv_imports['yaml']

        jinja_env = jinja2.Environment(loader=jinja2.FileSystemLoader(template_dir),
                                       undefined=jinja2.StrictUndefined)
        template = jinja_env.get_template(template_file)
        self.manifest = yaml.safe_load(template.render(**self._get_template_vars()))

        self.log("manifest generated:")
        self.log(pprint.pformat(self.manifest['mapping']))

    def verify_bits(self):
        """
        inspects each artifact and verifies that they were created by trustworthy tasks
        """
        # TODO
        self.log('skipping verification. unimplemented...')

    def refresh_antivirus(self):
        self.info("Refreshing clamav db...")
        try:
            redo.retry(lambda:
                       sh.freshclam("--stdout", "--verbose", _timeout=300,
                                    _err_to_out=True))
            self.info("Done.")
        except sh.ErrorReturnCode:
            self.warning("Freshclam failed, skipping DB update")

    def download_bits(self):
        """
        downloads list of artifacts to self.dest_dir dir based on a given manifest
        """
        self.log('downloading and uploading artifacts to self_dest_dir...')
        dirs = self.query_abs_dirs()

        for locale in self.manifest['mapping']:
            for deliverable in self.manifest['mapping'][locale]:
                self.log("downloading '{}' deliverable for '{}' locale".format(deliverable, locale))
                source = self.manifest['mapping'][locale][deliverable]['artifact']
                self.retry(
                    self.download_file,
                    args=[source],
                    kwargs={'parent_dir': dirs['abs_work_dir']},
                    error_level=FATAL)
        self.log('Success!')

    def _strip_prefix(self, s3_key):
        """Return file name relative to prefix"""
        # "abc/def/hfg".split("abc/de")[-1] == "f/hfg"
        return s3_key.split(self._get_template_vars()["s3_prefix"])[-1]

    def upload_bits(self):
        """
        uploads list of artifacts to s3 candidates dir based on a given manifest
        """
        self.log('uploading artifacts to s3...')
        dirs = self.query_abs_dirs()

        # connect to s3
        boto = self.virtualenv_imports['boto']
        conn = boto.connect_s3(self.aws_key_id, self.aws_secret_key)
        bucket = conn.get_bucket(self.bucket)

        for locale in self.manifest['mapping']:
            for deliverable in self.manifest['mapping'][locale]:
                self.log("uploading '{}' deliverable for '{}' locale".format(deliverable, locale))
                # we have already downloaded the files locally so we can use that version
                source = self.manifest['mapping'][locale][deliverable]['artifact']
                s3_key = self.manifest['mapping'][locale][deliverable]['s3_key']
                downloaded_file = os.path.join(dirs['abs_work_dir'], self.get_filename_from_url(source))
                # generate checksums for every uploaded file
                beet_file_name = '{}.beet'.format(downloaded_file)
                # upload checksums to a separate subdirectory
                beet_dest = '{prefix}beetmover-checksums/{f}.beet'.format(
                    prefix=self._get_template_vars()["s3_prefix"],
                    f=self._strip_prefix(s3_key)
                )
                beet_contents = '{hash} sha512 {size} {name}\n'.format(
                    hash=self.file_sha512sum(downloaded_file),
                    size=os.path.getsize(downloaded_file),
                    name=self._strip_prefix(s3_key))
                self.write_to_file(beet_file_name, beet_contents)
                self.upload_bit(source=downloaded_file, s3_key=s3_key,
                                bucket=bucket)
                self.upload_bit(source=beet_file_name, s3_key=beet_dest,
                                bucket=bucket)
        self.log('Success!')


    def upload_bit(self, source, s3_key, bucket):
        boto = self.virtualenv_imports['boto']
        self.info('uploading to s3 with key: {}'.format(s3_key))
        key = boto.s3.key.Key(bucket)  # create new key
        key.key = s3_key  # set key name

        self.info("Checking if `{}` already exists".format(s3_key))
        key = bucket.get_key(s3_key)
        if not key:
            self.info("Uploading to `{}`".format(s3_key))
            key = bucket.new_key(s3_key)
            # set key value
            mime_type, _ = mimetypes.guess_type(source)
            self.retry(lambda: key.set_contents_from_filename(source, headers={'Content-Type': mime_type}),
                       error_level=FATAL),
        else:
            if not get_hash(key.get_contents_as_string()) == get_hash(open(source).read()):
                # for now, let's halt. If necessary, we can revisit this and allow for overwrites
                #  to the same buildnum release with different bits
                self.fatal("`{}` already exists with different checksum.".format(s3_key))
            self.log("`{}` has the same MD5 checksum, not uploading".format(s3_key))

    def scan_bits(self):

        dirs = self.query_abs_dirs()

        filenames = [f for f in listdir(dirs['abs_work_dir']) if isfile(join(dirs['abs_work_dir'], f))]
        self.mkdir_p(self.dest_dir)
        for file_name in filenames:
            if self._matches_exclude(file_name):
                self.info("Excluding {} from virus scan".format(file_name))
            else:
                self.info('Copying {} to {}'.format(file_name,self.dest_dir))
                self.copyfile(os.path.join(dirs['abs_work_dir'], file_name), os.path.join(self.dest_dir,file_name))
        self._scan_files()
        self.info('Emptying {}'.format(self.dest_dir))
        self.rmtree(self.dest_dir)

    def _scan_files(self):
        """Scan the files we've collected. We do the download and scan concurrently to make
        it easier to have a coherent log afterwards. Uses the venv python."""
        external_tools_path = os.path.join(
                              os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))), 'external_tools')
        self.run_command([self.query_python_path(), os.path.join(external_tools_path,'extract_and_run_command.py'),
                         '-j{}'.format(self.config['scan_parallelization']),
                         'clamscan', '--no-summary', '--', self.dest_dir])

    def _matches_exclude(self, keyname):
         return any(re.search(exclude, keyname) for exclude in self.excludes)

    def mime_fix(self):
        """ Add mimetypes for custom extensions """
        mimetypes.init()
        map(lambda (ext, mime_type,): mimetypes.add_type(mime_type, ext), MIME_MAP.items())

if __name__ == '__main__':
    beet_mover = BeetMover(pop_aws_auth_from_env())
    beet_mover.run_and_exit()
