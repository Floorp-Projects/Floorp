from multiprocessing.pool import ThreadPool
import os
import re
import sys

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.python import VirtualenvMixin, virtualenv_config_options
from mozharness.base.script import BaseScript


class ReleasePusher(BaseScript, VirtualenvMixin):
    config_options = [
        [["--product"], {
            "dest": "product",
            "help": "Product being released, eg: firefox, thunderbird",
        }],
        [["--version"], {
            "dest": "version",
            "help": "Version of release, eg: 39.0b5",
        }],
        [["--build-number"], {
            "dest": "build_number",
            "help": "Build number of release, eg: 2",
        }],
        [["--bucket-name"], {
            "dest": "bucket_name",
            "help": "Bucket to copy files from candidates/ to releases/",
        }],
        [["--credentials"], {
            "dest": "credentials",
            "help": "File containing access key and secret access key",
        }],
        [["--exclude"], {
            "dest": "excludes",
            "default": [],
            "action": "append",
            "help": "List of patterns to exclude from copy. See script source for default.",
        }],
        [["-j", "--parallelization"], {
            "dest": "parallelization",
            "default": 20,
            "type": "int",
            "help": "Number of copy requests to run concurrently",
        }],
    ] + virtualenv_config_options

    def __init__(self):
        BaseScript.__init__(self,
            config_options=self.config_options,
            require_config_file=False,
            config={
                "virtualenv_modules": [
                    "boto",
                    "redo",
                ],
                "virtualenv_path": "venv",
            },
            all_actions=[
                "create-virtualenv",
                "activate-virtualenv",
                "push-to-releases",
            ],
            default_actions=[
                "create-virtualenv",
                "activate-virtualenv",
                "push-to-releases",
            ],
        )

        # set the env var for boto to read our special config file
        # rather than anything else we have at ~/.boto
        os.environ["BOTO_CONFIG"] = os.path.abspath(self.config["credentials"])

    def _pre_config_lock(self, rw_config):
        super(ReleasePusher, self)._pre_config_lock(rw_config)

        # This default is set here rather in the config because default
        # lists cannot be completely overidden, only appended to.
        if not self.config.get("excludes"):
            self.config["excludes"] = [
                r"^.*tests.*$",
                r"^.*crashreporter.*$",
                r"^.*\.zip(\.asc)?$",
                r"^.*\.log$",
                r"^.*\.txt$",
                r"^.*/partner-repacks.*$",
                r"^.*.checksums(\.asc)?$",
                r"^.*/logs/.*$",
                r"^.*/jsshell.*$",
                r"^.*json$",
                r"^.*/host.*$",
                r"^.*/mar-tools/.*$",
                r"^.*gecko-unsigned-unaligned.apk$",
                r"^.*robocop.apk$",
                r"^.*contrib.*"
            ]

    def _get_candidates_prefix(self):
        return "pub/{}/candidates/{}-candidates/build{}".format(
            self.config['product'],
            self.config["version"],
            self.config["build_number"]
        )

    def _get_releases_prefix(self):
        return "pub/{}/releases/{}".format(
            self.config["product"],
            self.config["version"]
        )

    def _matches_exclude(self, keyname):
        for exclude in self.config["excludes"]:
            if re.search(exclude, keyname):
                return True
        return False

    def push_to_releases(self):
        """This step grabs the list of files in the candidates dir,
        filters out any unwanted files from within them, and copies
        the remainder."""
        from boto.s3.connection import S3Connection
        from boto.exception import S3CopyError, S3ResponseError
        from redo import retry

        # suppress boto debug logging, it's too verbose with --loglevel=debug
        import logging
        logging.getLogger('boto').setLevel(logging.INFO)

        self.info("Connecting to S3")
        conn = S3Connection()
        self.info("Getting bucket {}".format(self.config["bucket_name"]))
        bucket = conn.get_bucket(self.config["bucket_name"])

        # ensure the destination is empty
        self.info("Checking destination {} is empty".format(self._get_releases_prefix()))
        keys = [k for k in bucket.list(prefix=self._get_releases_prefix())]
        if keys:
            self.fatal("Destination already exists with %s keys, aborting" %
                       len(keys))

        def worker(item):
            source, destination = item

            self.info("Copying {} to {}".format(source, destination))
            return retry(bucket.copy_key,
                         args=(destination,
                               self.config["bucket_name"],
                               source),
                         sleeptime=5, max_sleeptime=60,
                         retry_exceptions=(S3CopyError, S3ResponseError))

        def find_release_files():
            candidates_prefix = self._get_candidates_prefix()
            release_prefix = self._get_releases_prefix()
            self.info("Getting key names from candidates")
            for key in bucket.list(prefix=candidates_prefix):
                keyname = key.name
                if self._matches_exclude(keyname):
                    self.debug("Excluding {}".format(keyname))
                else:
                    destination = keyname.replace(candidates_prefix,
                                                  release_prefix)
                    yield (keyname, destination)

        pool = ThreadPool(self.config["parallelization"])
        pool.map(worker, find_release_files())

if __name__ == "__main__":
    myScript = ReleasePusher()
    myScript.run_and_exit()
