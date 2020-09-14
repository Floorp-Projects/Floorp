# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import re
import sys
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
            print("%s %s" % (commit, timestamp))
            return

        self.fetch_and_unpack(commit)

        self.log(logging.INFO, "vendor", {}, "Removing unnecessary files.")
        self.clean_upstream()

        self.log(logging.INFO, "vendor", {}, "Updating moz.yaml.")
        self.update_yaml(yaml_file, commit, timestamp)

        self.log(logging.INFO, "vendor", {}, "Updating files")
        self.update_files(commit, yaml_file)

        self.log(
            logging.INFO, "vendor", {}, "Registering changes with version control.",
        )
        self.repository.add_remove_files(
            self.manifest["vendoring"]["vendor-directory"], os.path.dirname(yaml_file)
        )

        self.log(logging.INFO, "vendor", {}, "Updating moz.build files")
        self.update_moz_build(yaml_file)

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
        elif self.manifest["vendoring"]["source-hosting"] == "github":
            from mozbuild.vendor.host_github import GitHubHost

            return GitHubHost(self.manifest)
        elif self.manifest["vendoring"]["source-hosting"] == "googlesource":
            from mozbuild.vendor.host_googlesource import GoogleSourceHost

            return GoogleSourceHost(self.manifest)
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
                "vendor",
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
            "vendor",
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

                self.log(
                    logging.DEBUG,
                    "vendor",
                    {"src": src, "dst": dst},
                    "Performing copy-file action src: {src} dst: {dst}",
                )

                with open(src) as f:
                    contents = f.read()
                with open(dst, "w") as f:
                    f.write(contents)
            elif update["action"] == "replace-in-file":
                file = get_full_path(update["file"])

                self.log(
                    logging.DEBUG,
                    "vendor",
                    {"file": file},
                    "Performing replace-in-file action file: {file}",
                )

                with open(file) as f:
                    contents = f.read()

                replacement = update["with"].replace("{revision}", revision)
                contents = contents.replace(update["pattern"], replacement)

                with open(file, "w") as f:
                    f.write(contents)
            elif update["action"] == "delete-path":
                path = get_full_path(update["path"])
                self.log(
                    logging.DEBUG,
                    "vendor",
                    {"path": path},
                    "Performing delete-path action path: {path}",
                )
                mozfile.remove(path)
            elif update["action"] == "run-script":
                script = get_full_path(update["script"], support_cwd=True)
                run_dir = get_full_path(update["cwd"])
                self.log(
                    logging.DEBUG,
                    "vendor",
                    {"script": script, "run_dir": run_dir},
                    "Performing run-script action script: {script} working dir: {run_dir}",
                )
                self.run_process(
                    args=[script], cwd=run_dir, log_name=script,
                )
            else:
                assert False, "Unknown action supplied (how did this pass validation?)"

    def update_moz_build(self, yaml_file):
        def load_file_into_list(path):
            from runpy import run_path

            return run_path(path)["sources"]

        source_suffixes = [".cc", ".c", ".cpp", ".h", ".hpp", ".S", ".asm"]

        files_removed = self.repository.get_changed_files(diff_filter="D")
        files_added = self.repository.get_changed_files(diff_filter="A")

        # Filter the files added to just source files we track in moz.build files.
        files_added = [
            f for f in files_added if any([f.endswith(s) for s in source_suffixes])
        ]
        map_added_file_to_mozbuild = {
            f: None
            for f in files_added
            if any([f.endswith(s) for s in source_suffixes])
        }

        self.log(
            logging.DEBUG,
            "vendor",
            {"added": len(files_added), "removed": len(files_removed)},
            "Found {added} files added and {removed} files removed.",
        )

        # Identify all the autovendored_sources.mozbuild files for this library,
        # which are required for Updatebot.
        moz_build_filenames = []
        for root, dirs, files in os.walk(os.path.dirname(yaml_file), topdown=False):
            if "autovendored_sources.mozbuild" in files:
                moz_build_filenames.append(
                    mozpath.join(root, "autovendored_sources.mozbuild")
                )

        # For each of the files, look for any removed files, and omit them if found.
        # At the same time, identify the correct moz.build file for any files added
        # by looking for the first moz.build match. Because we traverse bottom-up this
        # will be the best match.  (Until we find a library where it isn't and we
        # refactor.)
        def is_file_removed(l):
            l = l.strip(",'\" \n\r")
            for f in files_removed:
                if l.endswith(f):
                    return True
            return False

        should_abort = False

        for filename in moz_build_filenames:
            # Process deletions
            # Additions may not always go into the autovendored_sources.mozbuild file, but
            # it's kind of the best we can reasonably attempt to automate. However deletions
            # in the more complicated moz.build file we can easily address.
            files_to_check = [
                filename,
                filename.replace("autovendored_sources.mozbuild", "moz.build"),
            ]
            for deletion_filename in files_to_check:
                with open(deletion_filename, "r") as f:
                    moz_build_contents = f.readlines()

                new_moz_build_contents = [
                    l for l in moz_build_contents if not is_file_removed(l)
                ]

                if len(new_moz_build_contents) != len(moz_build_contents):
                    self.log(
                        logging.INFO,
                        "vendor",
                        {
                            "filename": deletion_filename,
                            "lines": (
                                len(moz_build_contents) - len(new_moz_build_contents)
                            ),
                        },
                        "Rewriting {filename} to remove {lines} lines.",
                    )
                    with open(deletion_filename, "w") as f:
                        f.write("".join(new_moz_build_contents))

            # See if this autovendored_sources file is a good fit for any of the files added
            for added_file in files_added:
                if map_added_file_to_mozbuild[added_file]:
                    continue
                dirname_of_added_file = os.path.dirname(added_file)

                if any([l for l in moz_build_contents if dirname_of_added_file in l]):
                    map_added_file_to_mozbuild[added_file] = filename

        # Now go through all the additions and add them in the correct place
        # in the correct autovendored_sources.mozbuild file
        for added_file, moz_build_filename in map_added_file_to_mozbuild.items():
            if not moz_build_filename:
                should_abort = True
                self.log(
                    logging.ERROR,
                    "vendor",
                    {"file": added_file},
                    "Could not identify the autovendored_sources.mozbuild file to add {file} to.",
                )
                continue

            # Load the sources variable as a python variable
            sources_file_contents = load_file_into_list(moz_build_filename)

            # Find a full matching path within the list - we're going to use this to
            # figure out the full path prefix we need
            prefix = None
            dirname_of_added_file = os.path.dirname(added_file)
            for l in sources_file_contents:
                if dirname_of_added_file in l:
                    # Grab the prefix and suffix
                    prefix = l[: l.index(dirname_of_added_file)]
                    break
            if not prefix:
                self.log(
                    logging.ERROR,
                    "vendor",
                    {"build_file": moz_build_filename, "added_file": added_file},
                    "Could not find a valid prefix in {build_file} for {added_file}.",
                )
                should_abort = True
                continue

            # Get just the filenames, add our added file, sort it
            sources_file_contents.append(prefix + added_file)
            sources_file_contents = sorted(sources_file_contents)

            # Write out the new file, tacking on the declaration and closing bracket
            self.log(
                logging.INFO,
                "vendor",
                {"build_file": moz_build_filename, "added_file": added_file},
                "Updating {build_file} to add {added_file}.",
            )
            with open(moz_build_filename, "w") as f:
                f.write("sources = [\n")
                for l in sources_file_contents:
                    newline = "'" + l + "',\n"
                    f.write(newline)
                f.write("]\n")

        if should_abort:
            self.log(
                logging.ERROR,
                "vendor",
                {},
                "This is a deficiency in ./mach vendor and should be reported to the "
                + "Updatebot maintainers.",
            )
            sys.exit(1)
