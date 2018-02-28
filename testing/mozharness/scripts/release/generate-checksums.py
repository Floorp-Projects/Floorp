import hashlib
import os
import re
import sys
from multiprocessing.pool import ThreadPool

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.python import VirtualenvMixin, virtualenv_config_options
from mozharness.base.script import BaseScript
from mozharness.mozilla.checksums import parse_checksums_file
from mozharness.mozilla.merkle import MerkleTree


class ChecksumsGenerator(BaseScript, VirtualenvMixin):
    config_options = [
        [["--stage-product"], {
            "dest": "stage_product",
            "help": "Name of product used in file server's directory structure, "
                    "e.g.: firefox, mobile",
        }],
        [["--version"], {
            "dest": "version",
            "help": "Version of release, e.g.: 59.0b5",
        }],
        [["--build-number"], {
            "dest": "build_number",
            "help": "Build number of release, e.g.: 2",
        }],
        [["--bucket-name"], {
            "dest": "bucket_name",
            "help": "Full bucket name e.g.: net-mozaws-prod-delivery-{firefox,archive}.",
        }],
        [["-j", "--parallelization"], {
            "dest": "parallelization",
            "default": 20,
            "type": int,
            "help": "Number of checksums file to download concurrently",
        }],
        [["--scm-level"], {
            "dest": "scm_level",
            "help": "dummy option",
        }],
        [["--branch"], {
            "dest": "branch",
            "help": "dummy option",
        }],
        [["--build-pool"], {
            "dest": "build_pool",
            "help": "dummy option",
        }],
    ] + virtualenv_config_options

    def __init__(self):
        BaseScript.__init__(self,
                            config_options=self.config_options,
                            require_config_file=False,
                            config={
                                "virtualenv_modules": [
                                    "pip==1.5.5",
                                    "boto",
                                ],
                                "virtualenv_path": "venv",
                            },
                            all_actions=[
                                "create-virtualenv",
                                "collect-individual-checksums",
                                "create-big-checksums",
                                "create-summary",
                            ],
                            default_actions=[
                                "create-virtualenv",
                                "collect-individual-checksums",
                                "create-big-checksums",
                                "create-summary",
                            ],
                            )

        self.checksums = {}
        self.file_prefix = self._get_file_prefix()

    def _pre_config_lock(self, rw_config):
        super(ChecksumsGenerator, self)._pre_config_lock(rw_config)

        # These defaults are set here rather in the config because default
        # lists cannot be completely overidden, only appended to.
        if not self.config.get("formats"):
            self.config["formats"] = ["sha512", "sha256"]

        if not self.config.get("includes"):
            self.config["includes"] = [
                r"^.*\.tar\.bz2$",
                r"^.*\.tar\.xz$",
                r"^.*\.dmg$",
                r"^.*\.bundle$",
                r"^.*\.mar$",
                r"^.*Setup.*\.exe$",
                r"^.*\.xpi$",
                r"^.*fennec.*\.apk$",
                r"^.*/jsshell.*$",
            ]

    def _get_file_prefix(self):
        return "pub/{}/candidates/{}-candidates/build{}/".format(
            self.config["stage_product"], self.config["version"], self.config["build_number"]
        )

    def _get_sums_filename(self, format_):
        return "{}SUMS".format(format_.upper())

    def _get_summary_filename(self, format_):
        return "{}SUMMARY".format(format_.upper())

    def _get_hash_function(self, format_):
        if format_ in ("sha256", "sha384", "sha512"):
            return getattr(hashlib, format_)
        else:
            self.fatal("Unsupported format {}".format(format_))

    def _get_bucket(self):
        self.activate_virtualenv()
        from boto import connect_s3
        self.info("Connecting to S3")
        conn = connect_s3(anon=True)
        self.info("Connecting to bucket {}".format(self.config["bucket_name"]))
        self.bucket = conn.get_bucket(self.config["bucket_name"])
        return self.bucket

    def collect_individual_checksums(self):
        """This step grabs all of the small checksums files for the release,
        filters out any unwanted files from within them, and adds the remainder
        to self.checksums for subsequent steps to use."""
        bucket = self._get_bucket()
        self.info("File prefix is: {}".format(self.file_prefix))

        # temporary holding place for checksums
        raw_checksums = []

        def worker(item):
            self.debug("Downloading {}".format(item))
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
                            self.fatal("Found duplicate checksum entry for {}, "
                                       "don't know which one to pick.".format(f))
                        if not set(self.config["formats"]) <= set(info["hashes"]):
                            self.fatal("Missing necessary format for file {}".format(f))
                        self.debug("Adding checksums for file: {}".format(f))
                        self.checksums[f] = info
                        break
                else:
                    self.debug("Ignoring checksums for file: {}".format(f))

    def create_summary(self):
        """
        This step computes a Merkle tree over the checksums for each format
        and writes a file containing the head of the tree and inclusion proofs
        for each file.
        """
        for fmt in self.config["formats"]:
            hash_fn = self._get_hash_function(fmt)
            files = [fn for fn in sorted(self.checksums)]
            data = [self.checksums[fn]["hashes"][fmt] for fn in files]

            tree = MerkleTree(hash_fn, data)
            head = tree.head().encode("hex")
            proofs = [tree.inclusion_proof(i).to_rfc6962_bis().encode("hex")
                      for i in range(len(files))]

            summary = self._get_summary_filename(fmt)
            self.info("Creating summary file: {}".format(summary))

            content = "{} TREE_HEAD\n".format(head)
            for i in range(len(files)):
                content += "{} {}\n".format(proofs[i], files[i])

            self.write_to_file(summary, content)

    def create_big_checksums(self):
        for fmt in self.config["formats"]:
            sums = self._get_sums_filename(fmt)
            self.info("Creating big checksums file: {}".format(sums))
            with open(sums, "w+") as output_file:
                for fn in sorted(self.checksums):
                    output_file.write("{}  {}\n".format(self.checksums[fn]["hashes"][fmt], fn))


if __name__ == "__main__":
    myScript = ChecksumsGenerator()
    myScript.run_and_exit()
