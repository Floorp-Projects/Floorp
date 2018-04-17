from multiprocessing.pool import ThreadPool
import os
import re
import sys
import shutil

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.python import VirtualenvMixin, virtualenv_config_options
from mozharness.base.script import BaseScript


class AntivirusScan(BaseScript, VirtualenvMixin):
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
            "help": "S3 Bucket to retrieve files from",
        }],
        [["--exclude"], {
            "dest": "excludes",
            "action": "append",
            "help": "List of filename patterns to exclude. See script source for default",
        }],
        [["-d", "--download-parallelization"], {
            "dest": "download_parallelization",
            "default": 6,
            "type": "int",
            "help": "Number of concurrent file downloads",
        }],
        [["-s", "--scan-parallelization"], {
            "dest": "scan_parallelization",
            "default": 4,
            "type": "int",
            "help": "Number of concurrent file scans",
        }],
        [["--tools-repo"], {
            "dest": "tools_repo",
            "default": "https://hg.mozilla.org/build/tools",
        }],
        [["--tools-revision"], {
            "dest": "tools_revision",
            "help": "Revision of tools repo to use when downloading extract_and_run_command.py",
        }],
        ] + virtualenv_config_options

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
        r"^.*json$",
        r"^.*/host.*$",
        r"^.*/mar-tools/.*$",
        r"^.*robocop.apk$",
        r"^.*contrib.*"
    ]
    CACHE_DIR = 'cache'

    def __init__(self):
        BaseScript.__init__(self,
                            config_options=self.config_options,
                            require_config_file=False,
                            config={
                                "virtualenv_modules": [
                                    "boto",
                                    "redo",
                                    "mar",
                                ],
                                "virtualenv_path": "venv",
                            },
                            all_actions=[
                                "create-virtualenv",
                                "activate-virtualenv",
                                "get-extract-script",
                                "get-files",
                                "scan-files",
                                "cleanup-cache",
                            ],
                            default_actions=[
                                "create-virtualenv",
                                "activate-virtualenv",
                                "get-extract-script",
                                "get-files",
                                "scan-files",
                                "cleanup-cache",
                            ],
                            )
        self.excludes = self.config.get('excludes', self.DEFAULT_EXCLUDES)
        self.dest_dir = self.CACHE_DIR

    def _get_candidates_prefix(self):
        return "pub/{}/candidates/{}-candidates/build{}/".format(
            self.config['product'],
            self.config["version"],
            self.config["build_number"]
        )

    def _matches_exclude(self, keyname):
        for exclude in self.excludes:
            if re.search(exclude, keyname):
                return True
        return False

    def get_extract_script(self):
        """Gets a copy of extract_and_run_command.py from tools, and the supporting mar.py,
        so that we can unpack various files for clam to scan them."""
        remote_file = "{}/raw-file/{}/stage/extract_and_run_command.py"\
                      .format(self.config["tools_repo"], self.config["tools_revision"])
        self.download_file(remote_file, file_name="extract_and_run_command.py")

    def get_files(self):
        """Pull the candidate files down from S3 for scanning, using parallel requests"""
        from boto.s3.connection import S3Connection
        from boto.exception import S3CopyError, S3ResponseError
        from redo import retry
        from httplib import HTTPException

        # suppress boto debug logging, it's too verbose with --loglevel=debug
        import logging
        logging.getLogger('boto').setLevel(logging.INFO)

        self.info("Connecting to S3")
        conn = S3Connection(anon=True)
        self.info("Getting bucket {}".format(self.config["bucket_name"]))
        bucket = conn.get_bucket(self.config["bucket_name"])

        if os.path.exists(self.dest_dir):
            self.info('Emptying {}'.format(self.dest_dir))
            shutil.rmtree(self.dest_dir)
        os.makedirs(self.dest_dir)

        def worker(item):
            source, destination = item

            self.info("Downloading {} to {}".format(source, destination))
            key = bucket.get_key(source)
            return retry(key.get_contents_to_filename,
                         args=(destination, ),
                         sleeptime=30, max_sleeptime=150,
                         retry_exceptions=(S3CopyError, S3ResponseError,
                                           IOError, HTTPException))

        def find_release_files():
            candidates_prefix = self._get_candidates_prefix()
            self.info("Getting key names from candidates")
            for key in bucket.list(prefix=candidates_prefix):
                keyname = key.name
                if self._matches_exclude(keyname):
                    self.debug("Excluding {}".format(keyname))
                else:
                    destination = os.path.join(self.dest_dir,
                                               keyname.replace(candidates_prefix, ''))
                    dest_dir = os.path.dirname(destination)
                    if not os.path.isdir(dest_dir):
                        os.makedirs(dest_dir)
                    yield (keyname, destination)

        pool = ThreadPool(self.config["download_parallelization"])
        pool.map(worker, find_release_files())

    def scan_files(self):
        """Scan the files we've collected. We do the download and scan concurrently to make
        it easier to have a coherent log afterwards. Uses the venv python."""
        self.run_command([self.query_python_path(), 'extract_and_run_command.py',
                          '-j{}'.format(self.config['scan_parallelization']),
                          'clamdscan', '-m', '--no-summary', '--', self.dest_dir])

    def cleanup_cache(self):
        """If we have simultaneous releases in flight an av slave may end up doing another
        av job before being recycled, and we need to make sure the full disk is available."""
        shutil.rmtree(self.dest_dir)


if __name__ == "__main__":
    myScript = AntivirusScan()
    myScript.run_and_exit()
