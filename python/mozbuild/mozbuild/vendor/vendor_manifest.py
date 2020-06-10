# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import re
import glob
import logging
import tarfile
import requests

import mozfile
import mozpack.path as mozpath

from mozbuild.base import MozbuildObject

DEFAULT_EXCLUDE_FILES = [
    ".git*",
]


class VendorManifest(MozbuildObject):
    def vendor(self, yaml_file, manifest, revision, check_for_update):
        self.manifest = manifest
        if "vendor-directory" not in self.manifest["vendoring"]:
            self.manifest["vendoring"]["vendor-directory"] = os.path.dirname(yaml_file)

        self.source_host = self.get_source_host()

        commit, timestamp = self.source_host.upstream_commit(revision)
        self.log(
            logging.INFO,
            "vendor",
            {"commit": commit, "timestamp": timestamp},
            "Latest commit is {commit} from {timestamp}",
        )

        if self.manifest["origin"]["revision"] == commit:
            self.log(
                logging.INFO,
                "vendor",
                {},
                "Latest upstream commit matches commit in-tree. Returning.",
            )
            return
        elif check_for_update:
            self.log(logging.ERROR, "vendor", {}, commit)
            return

        self.fetch_and_unpack(commit)

        self.log(logging.INFO, "clean_upstream", {}, "Removing unnecessary files.")
        self.clean_upstream()

        self.log(logging.INFO, "update_moz.yaml", {}, "Updating moz.yaml.")
        self.update_yaml(yaml_file, commit, timestamp)

        self.log(logging.INFO, "update_files", {}, "Updating files")
        self.update_files(commit, yaml_file)

        self.log(
            logging.INFO,
            "add_remove_files",
            {},
            "Registering changes with version control.",
        )
        self.repository.add_remove_files(
            self.manifest["vendoring"]["vendor-directory"], os.path.dirname(yaml_file)
        )

        self.log(
            logging.INFO,
            "done",
            {"revision": revision},
            "Update to version '{revision}' ready to commit.",
        )

    def get_source_host(self):
        if self.manifest["vendoring"]["source-hosting"] == "gitlab":
            from mozbuild.vendor.host_gitlab import GitLabHost

            return GitLabHost(self.manifest)
        else:
            raise Exception(
                "Unknown source host: " + self.manifest["vendoring"]["source-hosting"]
            )

    def fetch_and_unpack(self, revision):
        """Fetch and unpack upstream source"""
        url = self.source_host.upstream_snapshot(revision)
        self.log(
            logging.INFO,
            "vendor",
            {"revision_url": url},
            "Fetching code archive from {revision_url}",
        )

        prefix = self.manifest["origin"]["name"] + "-" + revision
        with mozfile.NamedTemporaryFile() as tmptarfile:
            req = requests.get(url, stream=True)
            for data in req.iter_content(4096):
                tmptarfile.write(data)
            tmptarfile.seek(0)

            tar = tarfile.open(tmptarfile.name)

            if any(
                [
                    name
                    for name in tar.getnames()
                    if name.startswith("/") or ".." in name
                ]
            ):
                raise Exception(
                    "Tar archive contains non-local paths," "e.g. '%s'" % bad_paths[0]
                )

            vendor_dir = self.manifest["vendoring"]["vendor-directory"]
            self.log(logging.INFO, "rm_vendor_dir", {}, "rm -rf %s" % vendor_dir)
            mozfile.remove(vendor_dir)

            self.log(
                logging.INFO,
                "unpack",
                {"vendor_dir": vendor_dir},
                "Unpacking upstream files from {vendor_dir}.",
            )
            tar.extractall(vendor_dir)

            # GitLab puts everything properly down a directory; move it up.
            if all(map(lambda name: name.startswith(prefix), tar.getnames())):
                tardir = mozpath.join(vendor_dir, prefix)
                mozfile.copy_contents(tardir, vendor_dir)
                mozfile.remove(tardir)

    def clean_upstream(self):
        """Remove files we don't want to import."""
        to_exclude = []
        vendor_dir = self.manifest["vendoring"]["vendor-directory"]
        for pattern in self.manifest["vendoring"]["exclude"] + DEFAULT_EXCLUDE_FILES:
            if "*" in pattern:
                to_exclude.extend(glob.iglob(mozpath.join(vendor_dir, pattern)))
            else:
                to_exclude.append(mozpath.join(vendor_dir, pattern))
        self.log(
            logging.INFO,
            "clean_upstream",
            {"files": to_exclude},
            "Removing: " + str(to_exclude),
        )
        for f in to_exclude:
            mozfile.remove(f)

    def update_yaml(self, yaml_file, revision, timestamp):
        with open(yaml_file) as f:
            yaml = f.readlines()

        replaced = 0
        replacements = [
            ["  release: commit", " %s (%s)." % (revision, timestamp)],
            ["  revision:", " %s" % (revision)],
        ]

        for i in range(0, len(yaml)):
            l = yaml[i]

            for r in replacements:
                if r[0] in l:
                    print("Found " + l)
                    replaced += 1
                    yaml[i] = re.sub(r[0] + " [v\.a-f0-9]+.*$", r[0] + r[1], yaml[i])

        assert len(replacements) == replaced

        with open(yaml_file, "w") as f:
            f.write("".join(yaml))

    def update_files(self, revision, yaml_file):
        def get_full_path(path, support_cwd=False):
            if support_cwd and path[0:5] == "{cwd}":
                path = path.replace("{cwd}", ".")
            elif "{yaml_dir}" in path:
                path = path.replace("{yaml_dir}", os.path.dirname(yaml_file))
            else:
                path = mozpath.join(
                    self.manifest["vendoring"]["vendor-directory"], path
                )
            return path

        if "update-actions" not in self.manifest["vendoring"]:
            return

        for update in self.manifest["vendoring"]["update-actions"]:
            if update["action"] == "copy-file":
                src = get_full_path(update["from"])
                dst = get_full_path(update["to"])

                with open(src) as f:
                    contents = f.read()
                with open(dst, "w") as f:
                    f.write(contents)
            elif update["action"] == "replace-in-file":
                file = get_full_path(update["file"])
                with open(file) as f:
                    contents = f.read()

                replacement = update["with"].replace("{revision}", revision)
                contents = contents.replace(update["pattern"], replacement)

                with open(file, "w") as f:
                    f.write(contents)
            elif update["action"] == "delete-path":
                path = get_full_path(update["path"])
                mozfile.remove(path)
            elif update["action"] == "run-script":
                script = get_full_path(update["script"], support_cwd=True)
                run_dir = get_full_path(update["cwd"])
                self.run_process(
                    args=[script], cwd=run_dir, log_name=script,
                )
            else:
                assert False, "Unknown action supplied (how did this pass validation?)"
