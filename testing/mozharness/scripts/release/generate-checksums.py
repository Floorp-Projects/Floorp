from multiprocessing.pool import ThreadPool
import os
from os import path
import re
import sys

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.python import VirtualenvMixin, virtualenv_config_options
from mozharness.base.script import BaseScript
from mozharness.base.vcs.vcsbase import VCSMixin
from mozharness.mozilla.checksums import parse_checksums_file
from mozharness.mozilla.signing import SigningMixin

class ChecksumsGenerator(BaseScript, VirtualenvMixin, SigningMixin, VCSMixin):
    config_options = [
        [["--stage-product"], {
            "dest": "stage_product",
            "help": "Name of product used in file server's directory structure, eg: firefox, mobile",
        }],
        [["--version"], {
            "dest": "version",
            "help": "Version of release, eg: 39.0b5",
        }],
        [["--build-number"], {
            "dest": "build_number",
            "help": "Build number of release, eg: 2",
        }],
        [["--bucket-name-prefix"], {
            "dest": "bucket_name_prefix",
            "help": "Prefix of bucket name, eg: net-mozaws-prod-delivery. This will be used to generate a full bucket name (such as net-mozaws-prod-delivery-{firefox,archive}.",
        }],
        [["-j", "--parallelization"], {
            "dest": "parallelization",
            "default": 20,
            "type": int,
            "help": "Number of checksums file to download concurrently",
        }],
        [["-f", "--format"], {
            "dest": "formats",
            "default": [],
            "action": "append",
            "help": "Format(s) to generate big checksums file for. Default: sha512",
        }],
        [["--include"], {
            "dest": "includes",
            "default": [],
            "action": "append",
            "help": "List of patterns to include in big checksums file. See script source for default.",
        }],
        [["--tools-repo"], {
            "dest": "tools_repo",
            "default": "https://hg.mozilla.org/build/tools",
        }],
        [["--gecko-repo"], {
            "dest": "gecko_repo",
            "help": "Gecko repo to get upload.py from, eg: https://hg.mozilla.org/releases/mozilla-beta",
        }],
        [["--gecko-revision"], {
            "dest": "gecko_revision",
            "help": "Revision of gecko repo to use when getting upload.py. Must be a valid hg revision identifier.",
        }],
        [["--upload-host"], {
            "dest": "upload_host",
            "help": "Host to upload big checksums file to, eg: upload.ffxbld.productdelivery.prod.mozaws.net",
        }],
        [["--upload-user"], {
            "dest": "upload_user",
            "help": "Username to use when uploading, eg: ffxbld",
        }],
        [["--upload-ssh-key"], {
            "dest": "upload_ssh_key",
            "help": "SSH Key to use when uploading, eg: ~/.ssh/ffxbld_dsa",
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
                "collect-individual-checksums",
                "create-big-checksums",
                "sign",
                "get-upload-script",
                "upload",
            ],
            default_actions=[
                "create-virtualenv",
                "activate-virtualenv",
                "collect-individual-checksums",
                "create-big-checksums",
                "sign",
                "get-upload-script",
                "upload",
            ],
        )

        self.checksums = {}

    def _pre_config_lock(self, rw_config):
        super(ChecksumsGenerator, self)._pre_config_lock(rw_config)

        # These defaults are set here rather in the config because default
        # lists cannot be completely overidden, only appended to.
        if not self.config.get("formats"):
            self.config["formats"] = ["sha512"]

        if not self.config.get("includes"):
            self.config["includes"] = [
                "^.*\.tar\.bz2$",
                "^.*\.dmg$",
                "^.*\.bundle$",
                "^.*\.mar$",
                "^.*Setup.*\.exe$",
                "^.*\.xpi$",
            ]

    def _get_bucket_name(self):
        suffix = "archive"
        # Firefox has a special bucket, per https://github.com/mozilla-services/product-delivery-tools/blob/master/bucketmap.go
        if self.config["stage_product"] == "firefox":
            suffix = "firefox"

        return "{}-{}".format(self.config["bucket_name_prefix"], suffix)

    def _get_file_prefix(self):
        return "pub/{}/candidates/{}-candidates/build{}".format(
            self.config["stage_product"], self.config["version"], self.config["build_number"]
        )

    def _get_sums_filename(self, format_):
        return "{}SUMS".format(format_.upper())

    def collect_individual_checksums(self):
        """This step grabs all of the small checksums files for the release,
        filters out any unwanted files from within them, and adds the remainder
        to self.checksums for subsequent steps to use."""
        from boto.s3.connection import S3Connection

        bucket_name = self._get_bucket_name()
        file_prefix = self._get_file_prefix()
        self.info("Bucket name is: {}".format(bucket_name))
        self.info("File prefix is: {}".format(file_prefix))

        self.info("Connecting to S3")
        conn = S3Connection(anon=True)
        self.debug("Successfully connected to S3")
        candidates = conn.get_bucket(bucket_name)

        # Temporary holding place for checksums
        raw_checksums = []
        def worker(item):
            self.debug("Downloading {}".format(item))
            # TODO: It would be nice to download the associated .asc file
            # and verify against it.
            sums = candidates.get_key(item).get_contents_as_string()
            raw_checksums.append(sums)

        def find_checksums_files():
            self.info("Getting key names from bucket")
            for key in candidates.list(prefix=file_prefix):
                if key.key.endswith(".checksums"):
                    self.debug("Found checksums file: {}".format(key.key))
                    yield key.key
                else:
                    self.debug("Ignoring non-checksums file: {}".format(key.key))

        pool = ThreadPool(self.config["parallelization"])
        pool.map(worker, find_checksums_files())

        for c in raw_checksums:
            for f, info in parse_checksums_file(c).iteritems():
                for pattern in self.config["includes"]:
                    if re.search(pattern, f):
                        if f in self.checksums:
                            self.fatal("Found duplicate checksum entry for {}, don't know which one to pick.".format(f))
                        if not set(self.config["formats"]) <= set(info["hashes"]):
                            self.fatal("Missing necessary format for file {}".format(f))
                        self.debug("Adding checksums for file: {}".format(f))
                        self.checksums[f] = info
                        break
                else:
                    self.debug("Ignoring checksums for file: {}".format(f))

    def create_big_checksums(self):
        for fmt in self.config["formats"]:
            sums = self._get_sums_filename(fmt)
            self.info("Creating big checksums file: {}".format(sums))
            with open(sums, "w+") as output_file:
                for fn in sorted(self.checksums):
                    output_file.write("{}  {}\n".format(self.checksums[fn]["hashes"][fmt], fn))

    def sign(self):
        dirs = self.query_abs_dirs()

        tools_dir = path.join(dirs["abs_work_dir"], "tools")
        self.vcs_checkout(
            repo=self.config["tools_repo"],
            vcs="hgtool",
            dest=tools_dir,
        )

        sign_cmd = self.query_moz_sign_cmd(formats=["gpg"])

        for fmt in self.config["formats"]:
            sums = self._get_sums_filename(fmt)
            self.info("Signing big checksums file: {}".format(sums))
            retval = self.run_command(sign_cmd + [sums])
            if retval != 0:
                self.fatal("Failed to sign {}".format(sums))

    def get_upload_script(self):
        remote_file = "{}/raw-file/{}/build/upload.py".format(self.config["gecko_repo"], self.config["gecko_revision"])
        self.download_file(remote_file, file_name="upload.py")

    def upload(self):
        dirs = self.query_abs_dirs()

        sys.path.append(path.dirname(path.abspath("upload.py")))
        from upload import UploadFiles

        files = []
        for fmt in self.config["formats"]:
            files.append(self._get_sums_filename(fmt))
            files.append("{}.asc".format(self._get_sums_filename(fmt)))

        post_upload_cmd = " ".join([
            "post_upload.py", "-p", self.config["stage_product"], "-n", self.config["build_number"],
            "-v", self.config["version"], "--release-to-candidates-dir", "--signed",
            "--bucket-prefix", self.config["bucket_name_prefix"],
        ])

        UploadFiles(
            self.config["upload_user"],
            self.config["upload_host"],
            None,
            files,
            ssh_key=self.config["upload_ssh_key"],
            base_path=dirs["abs_work_dir"],
            upload_to_temp_dir=True,
            post_upload_command=post_upload_cmd,
        )

if __name__ == "__main__":
    myScript = ChecksumsGenerator()
    myScript.run_and_exit()
