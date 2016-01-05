#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""beet_mover.py.

downloads artifacts and uploads them to s3
"""
import hashlib
import sys
import os
import pprint

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))
from mozharness.base.log import FATAL
from mozharness.base.python import VirtualenvMixin
from mozharness.base.script import BaseScript


def get_hash(content, hash_type="md5"):
    h = hashlib.new(hash_type)
    h.update(content)
    return h.hexdigest()


def get_aws_auth():
    """
    retrieves aws creds and deletes them from os.environ if present.
    """
    aws_key_id = os.environ.get("AWS_ACCESS_KEY_ID")
    aws_secret_key = os.environ.get("AWS_SECRET_ACCESS_KEY")

    if aws_key_id and aws_secret_key:
        del os.environ['AWS_ACCESS_KEY_ID']
        del os.environ['AWS_SECRET_ACCESS_KEY']
    else:
        exit("could not determine aws credentials from os environment")

    return aws_key_id, aws_secret_key


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
    [["--build-num"], {
        "dest": "build_num",
        "help": "the release build identifier"
    }],
    [["--taskid"], {
        "dest": "taskid",
        "help": "taskcluster task id to download artifacts from",
    }],
    [["--production"], {
        "dest": "production",
        "default": False,
        "help": "taskcluster task id to download artifacts from",
    }],
]


class BeetMover(BaseScript, VirtualenvMixin, object):
    def __init__(self, aws_creds):
        beetmover_kwargs = {
            'config_options': CONFIG_OPTIONS,
            'all_actions': [
                # 'clobber',
                'create-virtualenv',
                'activate-virtualenv',
                'generate-candidates-manifest',
                'verify-bits',  # beets
                'upload-bits',  # beets
            ],
            'require_config_file': False,
            # Default configuration
            'config': {
                # base index url where to find taskcluster artifact based on taskid
                # TODO - find out if we need to support taskcluster run number other than 0.
                # e.g. maybe we could end up with artifacts in > 'run 0' in a re-trigger situation?
                "artifact_base_url": 'https://queue.taskcluster.net/v1/task/{taskid}/runs/0/artifacts/public/build',
                "virtualenv_modules": [
                    "boto",
                    "PyYAML",
                    "Jinja2",
                ],
                "virtualenv_path": "venv",
                'buckets': {
                    'development': "mozilla-releng-beet-mover-dev",
                    'production': "mozilla-releng-beet-mover",
                },
                'product': 'firefox',
            },
        }
        super(BeetMover, self).__init__(**beetmover_kwargs)

        c = self.config
        self.manifest = {}
        # assigned in _post_create_virtualenv
        self.virtualenv_imports = None
        self.bucket = c['buckets']['production'] if c['production'] else c['buckets']['development']
        self.aws_key_id, self.aws_secret_key = aws_creds

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
        template_vars = {
            "platform": self.config['platform'],
            "locales": self.config['locales'],
            "version": self.config['version'],
            "app_version": self.config['app_version'],
            "build_num": self.config['build_num'],
            # mirror current release folder structure
            "s3_prefix": 'pub/{}/candidates'.format(self.config['product']),
            "artifact_base_url": self.config['artifact_base_url'].format(
                    taskid=self.config['taskid']
            )
        }
        self.manifest = yaml.safe_load(template.render(**template_vars))

        self.log("manifest generated:")
        self.log(pprint.pformat(self.manifest['mapping']))

    def verify_bits(self):
        """
        inspects each artifact and verifies that they were created by trustworthy tasks
        """
        # TODO
        self.log('skipping verification. unimplemented...')

    def upload_bits(self):
        """
        downloads and uploads list of artifacts to s3 candidates dir based on a given manifest
        """
        self.log('downloading and uploading artifacts to s3...')

        # connect to s3
        boto = self.virtualenv_imports['boto']
        conn = boto.connect_s3(self.aws_key_id, self.aws_secret_key)
        bucket = conn.get_bucket(self.bucket)

        for locale in self.manifest['mapping']:
            for deliverable in self.manifest['mapping'][locale]:
                self.log("uploading '{}' deliverable for '{}' locale".format(deliverable, locale))
                self.upload_bit(
                    source=self.manifest['mapping'][locale][deliverable]['artifact'],
                    s3_key=self.manifest['mapping'][locale][deliverable]['s3_key'],
                    bucket=bucket,
                )
        self.log('Success!')

    def upload_bit(self, source, s3_key, bucket):
        # TODO - do we want to mirror/upload to more than one region?
        dirs = self.query_abs_dirs()
        boto = self.virtualenv_imports['boto']

        # download locally
        file_name = self.retry(self.download_file,
                               args=[source],
                               kwargs={'parent_dir': dirs['abs_work_dir']},
                               error_level=FATAL)

        self.info('uploading to s3 with key: {}'.format(s3_key))
        key = boto.s3.key.Key(bucket)  # create new key
        key.key = s3_key  # set key name

        self.info("Checking if `{}` already exists".format(s3_key))
        key = bucket.get_key(s3_key)
        if not key:
            self.info("Uploading to `{}`".format(s3_key))
            key = bucket.new_key(s3_key)

            # set key value
            self.retry(key.set_contents_from_filename, args=[file_name], error_level=FATAL),

            # key.make_public() may lead to race conditions, because
            # it doesn't pass version_id, so it may not set permissions
            bucket.set_canned_acl(acl_str='public-read', key_name=s3_key,
                                  version_id=key.version_id)
        else:
            if not get_hash(key.get_contents_as_string()) == get_hash(open(file_name).read()):
                # for now, let's halt. If necessary, we can revisit this and allow for overwrites
                #  to the same buildnum release with different bits
                self.fatal("`{}` already exists with different checksum.".format(s3_key))
            self.log("`{}` has the same MD5 checksum, not uploading".format(s3_key))



if __name__ == '__main__':
    beet_mover = BeetMover(get_aws_auth())
    beet_mover.run_and_exit()
