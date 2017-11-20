from multiprocessing.pool import ThreadPool
import os
import re
import sys


sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.python import VirtualenvMixin, virtualenv_config_options
from mozharness.base.script import BaseScript
from mozharness.mozilla.aws import pop_aws_auth_from_env


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
            "default": [
                r"^.*tests.*$",
                r"^.*crashreporter.*$",
                r"^(?!.*jsshell-).*\.zip(\.asc)?$",
                r"^.*\.log$",
                r"^.*\.txt$",
                r"^.*/partner-repacks.*$",
                r"^.*.checksums(\.asc)?$",
                r"^.*/logs/.*$",
                r"^.*json$",
                r"^.*/host.*$",
                r"^.*/mar-tools/.*$",
                r"^.*robocop.apk$",
                r"^.*contrib.*",
                r"^.*/beetmover-checksums/.*$",
            ],
            "action": "append",
            "help": "List of patterns to exclude from copy. The list can be "
                    "extended by passing multiple --exclude arguments.",
        }],
        [["-j", "--parallelization"], {
            "dest": "parallelization",
            "default": 20,
            "type": "int",
            "help": "Number of copy requests to run concurrently",
        }],
    ] + virtualenv_config_options

    def __init__(self, aws_creds):
        BaseScript.__init__(self,
                            config_options=self.config_options,
                            require_config_file=False,
                            config={
                                    "virtualenv_modules": [
                                        "pip==1.5.5",
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

        # validate aws credentials
        if not (all(aws_creds) or self.config.get('credentials')):
            self.fatal("aws creds not defined. please add them to your config or env.")
        if any(aws_creds) and self.config.get('credentials'):
            self.fatal("aws creds found in env and self.config. please declare in one place only.")

        # set aws credentials
        if all(aws_creds):
            self.aws_key_id, self.aws_secret_key = aws_creds
        else:  # use
            self.aws_key_id, self.aws_secret_key = None, None
            # set the env var for boto to read our special config file
            # rather than anything else we have at ~/.boto
            os.environ["BOTO_CONFIG"] = os.path.abspath(self.config["credentials"])

    def _get_candidates_prefix(self):
        return "pub/{}/candidates/{}-candidates/build{}/".format(
            self.config['product'],
            self.config["version"],
            self.config["build_number"]
        )

    def _get_releases_prefix(self):
        return "pub/{}/releases/{}/".format(
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
        conn = S3Connection(aws_access_key_id=self.aws_key_id,
                            aws_secret_access_key=self.aws_secret_key)
        self.info("Getting bucket {}".format(self.config["bucket_name"]))
        bucket = conn.get_bucket(self.config["bucket_name"])

        # ensure the destination is empty
        self.info("Checking destination {} is empty".format(self._get_releases_prefix()))
        keys = [k for k in bucket.list(prefix=self._get_releases_prefix())]
        if keys:
            self.warning("Destination already exists with %s keys" % len(keys))

        def worker(item):
            source, destination = item

            def copy_key():
                source_key = bucket.get_key(source)
                dest_key = bucket.get_key(destination)
                # According to
                # http://docs.aws.amazon.com/AmazonS3/latest/API/RESTCommonResponseHeaders.html
                # S3 key MD5 is represented as ETag, except when objects are
                # uploaded using multipart method. In this case objects's ETag
                # is constructed using its MD5, minus symbol, and number of
                # part. See http://stackoverflow.com/questions/12186993/what-is-the-algorithm-to-compute-the-amazon-s3-etag-for-a-file-larger-than-5gb#answer-19896823  # noqa
                source_md5 = source_key.etag.split("-")[0]
                if dest_key:
                    dest_md5 = dest_key.etag.split("-")[0]
                else:
                    dest_md5 = None

                if not dest_key:
                    self.info("Copying {} to {}".format(source, destination))
                    bucket.copy_key(destination, self.config["bucket_name"],
                                    source)
                elif source_md5 == dest_md5:
                    self.warning(
                        "{} already exists with the same content ({}), skipping copy".format(
                            destination, dest_md5))
                else:
                    self.fatal(
                        "{} already exists with the different content "
                        "(src ETag: {}, dest ETag: {}), aborting".format(
                            destination, source_key.etag, dest_key.etag))

            return retry(copy_key, sleeptime=5, max_sleeptime=60,
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
    myScript = ReleasePusher(pop_aws_auth_from_env())
    myScript.run_and_exit()
