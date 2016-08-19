from multiprocessing.pool import ThreadPool
import os
from os import path
import re
import sys
import posixpath

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.python import VirtualenvMixin, virtualenv_config_options
from mozharness.base.script import BaseScript
from mozharness.base.vcs.vcsbase import VCSMixin
from mozharness.mozilla.checksums import parse_checksums_file
from mozharness.mozilla.signing import SigningMixin
from mozharness.mozilla.buildbot import BuildbotMixin

class ChecksumsGenerator(BaseScript, VirtualenvMixin, SigningMixin, VCSMixin, BuildbotMixin):
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
        [["--bucket-name-full"], {
            "dest": "bucket_name_full",
            "help": "Full bucket name, eg: net-mozaws-prod-delivery-firefox",
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
        [["--credentials"], {
            "dest": "credentials",
            "help": "File containing access key and secret access key for S3",
        }],
    ] + virtualenv_config_options

    def __init__(self):
        BaseScript.__init__(self,
            config_options=self.config_options,
            require_config_file=False,
            config={
                "virtualenv_modules": [
                    "boto",
                ],
                "virtualenv_path": "venv",
                'buildbot_json_path': 'buildprops.json',
            },
            all_actions=[
                "create-virtualenv",
                "collect-individual-checksums",
                "create-big-checksums",
                "sign",
                "upload",
                "copy-info-files",
            ],
            default_actions=[
                "create-virtualenv",
                "collect-individual-checksums",
                "create-big-checksums",
                "sign",
                "upload",
            ],
        )

        self.checksums = {}
        self.bucket = None
        self.bucket_name = self._get_bucket_name()
        self.file_prefix = self._get_file_prefix()
        # set the env var for boto to read our special config file
        # rather than anything else we have at ~/.boto
        os.environ["BOTO_CONFIG"] = os.path.abspath(self.config["credentials"])

    def _pre_config_lock(self, rw_config):
        super(ChecksumsGenerator, self)._pre_config_lock(rw_config)

        # override properties from buildbot properties here as defined by
        # taskcluster properties
        self.read_buildbot_config()
        if not self.buildbot_config:
            self.warning("Skipping buildbot properties overrides")
            return
        # TODO: version should come from repo
        props = self.buildbot_config["properties"]
        for prop in ['version', 'build_number']:
            if props.get(prop):
                self.info("Overriding %s with %s" % (prop, props[prop]))
                self.config[prop] = props.get(prop)

        # These defaults are set here rather in the config because default
        # lists cannot be completely overidden, only appended to.
        if not self.config.get("formats"):
            self.config["formats"] = ["sha512"]

        if not self.config.get("includes"):
            self.config["includes"] = [
                r"^.*\.tar\.bz2$",
                r"^.*\.tar\.xz$",
                r"^.*\.dmg$",
                r"^.*\.bundle$",
                r"^.*\.mar$",
                r"^.*Setup.*\.exe$",
                r"^.*\.xpi$",
            ]

    def _get_bucket_name(self):
        if self.config.get('bucket_name_full'):
            return self.config['bucket_name_full']

        suffix = "archive"
        # Firefox has a special bucket, per https://github.com/mozilla-services/product-delivery-tools/blob/master/bucketmap.go
        if self.config["stage_product"] == "firefox":
            suffix = "firefox"

        return "{}-{}".format(self.config["bucket_name_prefix"], suffix)

    def _get_file_prefix(self):
        return "pub/{}/candidates/{}-candidates/build{}/".format(
            self.config["stage_product"], self.config["version"], self.config["build_number"]
        )

    def _get_sums_filename(self, format_):
        return "{}SUMS".format(format_.upper())

    def _get_bucket(self):
        if not self.bucket:
            self.activate_virtualenv()
            from boto.s3.connection import S3Connection

            self.info("Connecting to S3")
            conn = S3Connection()
            self.debug("Successfully connected to S3")
            self.info("Connecting to bucket {}".format(self.bucket_name))
            self.bucket = conn.get_bucket(self.bucket_name)
        return self.bucket

    def collect_individual_checksums(self):
        """This step grabs all of the small checksums files for the release,
        filters out any unwanted files from within them, and adds the remainder
        to self.checksums for subsequent steps to use."""
        bucket = self._get_bucket()
        self.info("File prefix is: {}".format(self.file_prefix))

        # Temporary holding place for checksums
        raw_checksums = []
        def worker(item):
            self.debug("Downloading {}".format(item))
            # TODO: It would be nice to download the associated .asc file
            # and verify against it.
            sums = bucket.get_key(item).get_contents_as_string()
            raw_checksums.append(sums)

        def find_checksums_files():
            self.info("Getting key names from bucket")
            checksum_files = {"beets": [], "checksums": []}
            for key in bucket.list(prefix=self.file_prefix):
                if key.key.endswith(".checksums"):
                    self.debug("Found checksums file: {}".format(key.key))
                    checksum_files["checksums"].append(key.key)
                elif key.key.endswith(".beet"):
                    self.debug("Found beet file: {}".format(key.key))
                    checksum_files["beets"].append(key.key)
                else:
                    self.debug("Ignoring non-checksums file: {}".format(key.key))
            if checksum_files["beets"]:
                self.log("Using beet format")
                return checksum_files["beets"]
            else:
                self.log("Using checksums format")
                return checksum_files["checksums"]

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
            branch="default",
            vcs="hg",
            dest=tools_dir,
        )

        sign_cmd = self.query_moz_sign_cmd(formats=["gpg"])

        for fmt in self.config["formats"]:
            sums = self._get_sums_filename(fmt)
            self.info("Signing big checksums file: {}".format(sums))
            retval = self.run_command(sign_cmd + [sums])
            if retval != 0:
                self.fatal("Failed to sign {}".format(sums))

    def upload(self):
        # we need to provide the public side of the gpg key so that people can
        # verify the detached signatures
        dirs = self.query_abs_dirs()
        tools_dir = path.join(dirs["abs_work_dir"], "tools")
        self.copyfile(os.path.join(tools_dir, 'scripts', 'release', 'KEY'),
                      'KEY')
        files = ['KEY']

        for fmt in self.config["formats"]:
            files.append(self._get_sums_filename(fmt))
            files.append("{}.asc".format(self._get_sums_filename(fmt)))

        bucket = self._get_bucket()
        for f in files:
            dest = posixpath.join(self.file_prefix, f)
            self.info("Uploading {} to {}".format(f, dest))
            key = bucket.new_key(dest)
            key.set_contents_from_filename(f, headers={'Content-Type': 'text/plain'})

    def copy_info_files(self):
        bucket = self._get_bucket()

        for key in bucket.list(prefix=self.file_prefix):
            if re.search(r'/en-US/android.*_info\.txt$', key.name):
                self.info("Found      {}".format(key.name))
                dest = posixpath.join(self.file_prefix, posixpath.basename(key.name))
                self.info("Copying to {}".format(dest))
                bucket.copy_key(new_key_name=dest,
                                src_bucket_name=self.bucket_name,
                                src_key_name=key.name,
                                metadata={'Content-Type': 'text/plain'})


if __name__ == "__main__":
    myScript = ChecksumsGenerator()
    myScript.run_and_exit()
