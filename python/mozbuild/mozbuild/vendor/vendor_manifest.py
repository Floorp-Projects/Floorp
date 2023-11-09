# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import functools
import glob
import logging
import os
import re
import shutil
import stat
import sys
import tarfile
import tempfile
from collections import defaultdict

import mozfile
import mozpack.path as mozpath
import requests

from mozbuild.base import MozbuildObject
from mozbuild.vendor.rewrite_mozbuild import (
    MozBuildRewriteException,
    add_file_to_moz_build_file,
    remove_file_from_moz_build_file,
)

DEFAULT_EXCLUDE_FILES = [".git*", ".git*/**"]
DEFAULT_KEEP_FILES = ["**/moz.build", "**/moz.yaml"]
DEFAULT_INCLUDE_FILES = []


def iglob_hidden(*args, **kwargs):
    # glob._ishidden exists from 3.5 up to 3.12 (and beyond?)
    old_ishidden = glob._ishidden
    glob._ishidden = lambda x: False
    try:
        yield from glob.iglob(*args, **kwargs)
    finally:
        glob._ishidden = old_ishidden


def throwe():
    raise Exception


def _replace_in_file(file, pattern, replacement, regex=False):
    def replacer(matchobj: re.Match):
        if matchobj.group(0) == replacement:
            print(f"WARNING: {action} replaced '{matchobj.group(0)}' with same.")
        return replacement

    with open(file) as f:
        contents = f.read()

    action = "replace-in-file-regex"
    if not regex:
        pattern = re.escape(pattern)
        action = "replace-in-file"

    newcontents, count = re.subn(pattern, replacer, contents)
    if count < 1:
        raise Exception(
            f"{action} could not find '{pattern}' in {file} to replace with '{replacement}'."
        )

    with open(file, "w") as f:
        f.write(newcontents)


def list_of_paths_to_readable_string(paths):
    # From https://stackoverflow.com/a/41578071
    dic = defaultdict(list)
    for item in paths:
        if os.path.isdir(item):  # To check path is a directory
            _ = dic[item]  # will set default value as empty list
        else:
            path, file = os.path.split(item)
            dic[path].append(file)

    final_string = "["
    for key, val in dic.items():
        if len(val) == 0:
            final_string += key + ", "
        elif len(val) < 3:
            final_string += ", ".join([os.path.join(key, v) for v in val]) + ", "
        elif len(val) < 10:
            final_string += "%s items in %s: %s and %s, " % (
                len(val),
                key,
                ", ".join(val[0:-1]),
                val[-1],
            )
        else:
            final_string += "%s (omitted) items in %s, " % (len(val), key)

    if final_string[-2:] == ", ":
        final_string = final_string[:-2]

    return final_string + "]"


class VendorManifest(MozbuildObject):
    def should_perform_step(self, step):
        return step not in self.manifest["vendoring"].get("skip-vendoring-steps", [])

    def vendor(
        self,
        command_context,
        yaml_file,
        manifest,
        revision,
        ignore_modified,
        check_for_update,
        force,
        add_to_exports,
        patch_mode,
    ):
        self.manifest = manifest
        self.yaml_file = yaml_file
        self._extract_directory = throwe
        self.logInfo = functools.partial(self.log, logging.INFO, "vendor")
        self.patch_mode = patch_mode
        if "vendor-directory" not in self.manifest["vendoring"]:
            self.manifest["vendoring"]["vendor-directory"] = os.path.dirname(
                self.yaml_file
            )

        # ==========================================================
        # If we're only patching; do that
        if "patches" in self.manifest["vendoring"] and patch_mode == "only":
            self.import_local_patches(
                self.manifest["vendoring"]["patches"],
                os.path.dirname(self.yaml_file),
                self.manifest["vendoring"]["vendor-directory"],
            )
            return

        # ==========================================================
        self.source_host = self.get_source_host()

        ref_type = self.manifest["vendoring"].get("tracking", "commit")
        flavor = self.manifest["vendoring"].get("flavor", "regular")
        # Individiual files are special

        if revision == "tip":
            # This case allows us to force-update a tag-tracking library to master
            new_revision, timestamp = self.source_host.upstream_commit("HEAD")
        elif ref_type == "tag":
            new_revision, timestamp = self.source_host.upstream_tag(revision)
        else:
            new_revision, timestamp = self.source_host.upstream_commit(revision)

        self.logInfo(
            {"ref_type": ref_type, "ref": new_revision, "timestamp": timestamp},
            "Latest {ref_type} is {ref} from {timestamp}",
        )

        # ==========================================================
        if not force and self.manifest["origin"]["revision"] == new_revision:
            # We're up to date, don't do anything
            self.logInfo({}, "Latest upstream matches in-tree.")
            return
        elif flavor != "individual-file" and check_for_update:
            # Only print the new revision to stdout
            print("%s %s" % (new_revision, timestamp))
            return

        # ==========================================================
        if flavor == "regular":
            self.process_regular(
                new_revision, timestamp, ignore_modified, add_to_exports
            )
        elif flavor == "individual-files":
            self.process_individual(
                new_revision, timestamp, ignore_modified, add_to_exports
            )
        elif flavor == "rust":
            self.process_rust(
                command_context,
                self.manifest["origin"]["revision"],
                new_revision,
                timestamp,
                ignore_modified,
            )
        else:
            raise Exception("Unknown flavor")

    def process_rust(
        self, command_context, old_revision, new_revision, timestamp, ignore_modified
    ):
        # First update the Cargo.toml
        cargo_file = os.path.join(os.path.dirname(self.yaml_file), "Cargo.toml")
        try:
            _replace_in_file(cargo_file, old_revision, new_revision)
        except Exception:
            # If we can't find it the first time, try again with a short hash
            _replace_in_file(cargo_file, old_revision[:8], new_revision)

        # Then call ./mach vendor rust
        from mozbuild.vendor.vendor_rust import VendorRust

        vendor_command = command_context._spawn(VendorRust)
        vendor_command.vendor(ignore_modified=True)

        self.update_yaml(new_revision, timestamp)

    def fetch_individual(self, new_revision):
        # This design is used because there is no github API to query
        # for the last commit that modified a file; nor a way to get file
        # blame.  So really all we can do is just download and replace the
        # files and see if they changed...

        def download_and_write_file(url, destination):
            self.logInfo(
                {"local_file": destination, "url": url},
                "Downloading {local_file} from {url}...",
            )

            with mozfile.NamedTemporaryFile() as tmpfile:
                try:
                    req = requests.get(url, stream=True)
                    for data in req.iter_content(4096):
                        tmpfile.write(data)
                    tmpfile.seek(0)

                    shutil.copy2(tmpfile.name, destination)
                except Exception as e:
                    raise (e)

        # Only one of these loops will have content, so just do them both
        for f in self.manifest["vendoring"].get("individual-files", []):
            url = self.source_host.upstream_path_to_file(new_revision, f["upstream"])
            destination = self.get_full_path(f["destination"])
            download_and_write_file(url, destination)

        for f in self.manifest["vendoring"].get("individual-files-list", []):
            url = self.source_host.upstream_path_to_file(
                new_revision,
                self.manifest["vendoring"]["individual-files-default-upstream"] + f,
            )
            destination = self.get_full_path(
                self.manifest["vendoring"]["individual-files-default-destination"] + f
            )
            download_and_write_file(url, destination)

    def process_regular_or_individual(
        self, is_individual, new_revision, timestamp, ignore_modified, add_to_exports
    ):
        if self.should_perform_step("fetch"):
            if is_individual:
                self.fetch_individual(new_revision)
            else:
                self.fetch_and_unpack(new_revision)
        else:
            self.logInfo({}, "Skipping fetching upstream source.")

        self.logInfo({}, "Checking for update actions")
        self.update_files(new_revision)

        if self.patch_mode == "check":
            self.import_local_patches(
                self.manifest["vendoring"].get("patches", []),
                os.path.dirname(self.yaml_file),
                self.manifest["vendoring"]["vendor-directory"],
            )
        elif "patches" in self.manifest["vendoring"]:
            # Remind the user
            self.log(
                logging.CRITICAL,
                "vendor",
                {},
                "Patches present in manifest!!! Please run "
                "'./mach vendor --patch-mode only' after commiting changes.",
            )

        if self.should_perform_step("hg-add"):
            self.logInfo({}, "Registering changes with version control.")
            self.repository.add_remove_files(
                self.manifest["vendoring"]["vendor-directory"],
                os.path.dirname(self.yaml_file),
            )
        else:
            self.logInfo({}, "Skipping registering changes.")

        if self.should_perform_step("spurious-check"):
            self.logInfo({}, "Checking for a spurious update.")
            self.spurious_check(new_revision, ignore_modified)
        else:
            self.logInfo({}, "Skipping the spurious update check.")

        if self.should_perform_step("update-moz-yaml"):
            self.logInfo({}, "Updating moz.yaml.")
            self.update_yaml(new_revision, timestamp)
        else:
            self.logInfo({}, "Skipping updating the moz.yaml file.")

        # individual flavor does not need this step, but performing it should
        # always be a no-op
        if self.should_perform_step("update-moz-build"):
            self.logInfo({}, "Updating moz.build files")
            self.update_moz_build(
                self.manifest["vendoring"]["vendor-directory"],
                os.path.dirname(self.yaml_file),
                add_to_exports,
            )
        else:
            self.logInfo({}, "Skipping update of moz.build files")

        self.logInfo({"rev": new_revision}, "Updated to '{rev}'.")

    def process_regular(self, new_revision, timestamp, ignore_modified, add_to_exports):
        is_individual = False
        self.process_regular_or_individual(
            is_individual, new_revision, timestamp, ignore_modified, add_to_exports
        )

    def process_individual(
        self, new_revision, timestamp, ignore_modified, add_to_exports
    ):
        is_individual = True
        self.process_regular_or_individual(
            is_individual, new_revision, timestamp, ignore_modified, add_to_exports
        )

    def get_source_host(self):
        if self.manifest["vendoring"]["source-hosting"] == "gitlab":
            from mozbuild.vendor.host_gitlab import GitLabHost

            return GitLabHost(self.manifest)
        elif self.manifest["vendoring"]["source-hosting"] == "github":
            from mozbuild.vendor.host_github import GitHubHost

            return GitHubHost(self.manifest)
        elif self.manifest["vendoring"]["source-hosting"] == "git":
            from mozbuild.vendor.host_git import GitHost

            return GitHost(self.manifest)
        elif self.manifest["vendoring"]["source-hosting"] == "googlesource":
            from mozbuild.vendor.host_googlesource import GoogleSourceHost

            return GoogleSourceHost(self.manifest)
        elif self.manifest["vendoring"]["source-hosting"] == "angle":
            from mozbuild.vendor.host_angle import AngleHost

            return AngleHost(self.manifest)
        elif self.manifest["vendoring"]["source-hosting"] == "codeberg":
            from mozbuild.vendor.host_codeberg import CodebergHost

            return CodebergHost(self.manifest)
        else:
            raise Exception(
                "Unknown source host: " + self.manifest["vendoring"]["source-hosting"]
            )

    def get_full_path(self, path, support_cwd=False):
        if support_cwd and path[0:5] == "{cwd}":
            path = path.replace("{cwd}", ".")
        elif "{tmpextractdir}" in path:
            # _extract_directory() will throw an exception if it is invalid to use it
            path = path.replace("{tmpextractdir}", self._extract_directory())
        elif "{yaml_dir}" in path:
            path = path.replace("{yaml_dir}", os.path.dirname(self.yaml_file))
        elif "{vendor_dir}" in path:
            path = path.replace(
                "{vendor_dir}", self.manifest["vendoring"]["vendor-directory"]
            )
        else:
            path = mozpath.join(self.manifest["vendoring"]["vendor-directory"], path)
        return os.path.abspath(path)

    def convert_patterns_to_paths(self, directory, patterns):
        # glob.iglob uses shell-style wildcards for path name completion.
        # "recursive=True" enables the double asterisk "**" wildcard which matches
        # for nested directories as well as the directory we're searching in.
        paths = []
        for pattern in patterns:
            pattern_full_path = mozpath.join(directory, pattern)
            # If pattern is a directory recursively add contents of directory
            if os.path.isdir(pattern_full_path):
                # Append double asterisk to the end to make glob.iglob recursively match
                # contents of directory
                paths.extend(
                    iglob_hidden(mozpath.join(pattern_full_path, "**"), recursive=True)
                )
            # Otherwise pattern is a file or wildcard expression so add it without altering it
            else:
                paths.extend(iglob_hidden(pattern_full_path, recursive=True))
        # Remove folder names from list of paths in order to avoid prematurely
        # truncating directories elsewhere
        # Sort the final list to ensure we preserve 01_, 02_ ordering for e.g. *.patch globs
        final_paths = sorted(
            [mozpath.normsep(path) for path in paths if not os.path.isdir(path)]
        )
        return final_paths

    def fetch_and_unpack(self, revision):
        """Fetch and unpack upstream source"""

        def validate_tar_member(member, path):
            def is_within_directory(directory, target):
                real_directory = os.path.realpath(directory)
                real_target = os.path.realpath(target)
                prefix = os.path.commonprefix([real_directory, real_target])
                return prefix == real_directory

            member_path = os.path.join(path, member.name)
            if not is_within_directory(path, member_path):
                raise Exception("Attempted path traversal in tar file: " + member.name)
            if member.issym():
                link_path = os.path.join(os.path.dirname(member_path), member.linkname)
                if not is_within_directory(path, link_path):
                    raise Exception(
                        "Attempted link path traversal in tar file: " + member.name
                    )
            if member.mode & (stat.S_ISUID | stat.S_ISGID):
                raise Exception(
                    "Attempted setuid or setgid in tar file: " + member.name
                )

        def safe_extract(tar, path=".", *, numeric_owner=False):
            def _files(tar, path):
                for member in tar:
                    validate_tar_member(member, path)
                    yield member

            tar.extractall(path, members=_files(tar, path), numeric_owner=numeric_owner)

        release_artifact = self.manifest["vendoring"].get("release-artifact", False)

        if release_artifact:
            url = self.source_host.upstream_release_artifact(revision, release_artifact)
        else:
            url = self.source_host.upstream_snapshot(revision)

        self.logInfo({"url": url}, "Fetching code archive from {url}")

        with mozfile.NamedTemporaryFile() as tmptarfile:
            tmpextractdir = tempfile.TemporaryDirectory()
            try:
                if url.startswith("file://"):
                    with open(url[len("file://") :], "rb") as tarinput:
                        tmptarfile.write(tarinput.read())
                else:
                    req = requests.get(url, stream=True)
                    for data in req.iter_content(4096):
                        tmptarfile.write(data)
                tmptarfile.seek(0)

                vendor_dir = mozpath.normsep(
                    self.manifest["vendoring"]["vendor-directory"]
                )
                if self.should_perform_step("keep"):
                    self.logInfo({}, "Retaining wanted in-tree files.")
                    to_keep = self.convert_patterns_to_paths(
                        vendor_dir,
                        self.manifest["vendoring"].get("keep", [])
                        + DEFAULT_KEEP_FILES
                        + self.manifest["vendoring"].get("patches", []),
                    )
                else:
                    self.logInfo({}, "Skipping retention of in-tree files.")
                    to_keep = []

                self.logInfo({"vd": vendor_dir}, "Cleaning {vd} to import changes.")
                # We use double asterisk wildcard here to get complete list of recursive contents
                for file in self.convert_patterns_to_paths(vendor_dir, ["**"]):
                    file = mozpath.normsep(file)
                    if file not in to_keep:
                        mozfile.remove(file)

                self.logInfo({"vd": vendor_dir}, "Unpacking upstream files for {vd}.")
                with tarfile.open(tmptarfile.name) as tar:
                    safe_extract(tar, tmpextractdir.name)

                    def get_first_dir(p):
                        halves = os.path.split(p)
                        return get_first_dir(halves[0]) if halves[0] else halves[1]

                    one_prefix = get_first_dir(tar.getnames()[0])
                    has_prefix = all(
                        map(lambda name: name.startswith(one_prefix), tar.getnames())
                    )

                # GitLab puts everything down a directory; move it up.
                if has_prefix:
                    tardir = mozpath.join(tmpextractdir.name, one_prefix)
                    mozfile.copy_contents(
                        tardir, tmpextractdir.name, ignore_dangling_symlinks=True
                    )
                    mozfile.remove(tardir)

                if self.should_perform_step("include"):
                    self.logInfo({}, "Retaining wanted files from upstream changes.")
                    to_include = self.convert_patterns_to_paths(
                        tmpextractdir.name,
                        self.manifest["vendoring"].get("include", [])
                        + DEFAULT_INCLUDE_FILES,
                    )
                else:
                    self.logInfo({}, "Skipping retention of included files.")
                    to_include = []

                if self.should_perform_step("exclude"):
                    self.logInfo({}, "Removing excluded files from upstream changes.")
                    to_exclude = self.convert_patterns_to_paths(
                        tmpextractdir.name,
                        self.manifest["vendoring"].get("exclude", [])
                        + DEFAULT_EXCLUDE_FILES,
                    )
                else:
                    self.logInfo({}, "Skipping removing excluded files.")
                    to_exclude = []

                # If we have files that match both patterns, figure out the _longer_
                # pattern that it matches. (We hope this will be the more precise/stricter one)
                conflicts = list(set(to_exclude).intersection(set(to_include)))
                if conflicts:
                    remove_from_include = []
                    remove_from_exclude = []

                    for c in conflicts:
                        longest_exclude = ""
                        longest_include = ""

                        for pattern in (
                            self.manifest["vendoring"].get("exclude", [])
                            + DEFAULT_EXCLUDE_FILES
                        ):
                            if c in self.convert_patterns_to_paths(
                                tmpextractdir.name,
                                [pattern],
                            ):
                                if len(pattern) > len(longest_exclude):
                                    longest_exclude = pattern

                        for pattern in (
                            self.manifest["vendoring"].get("include", [])
                            + DEFAULT_INCLUDE_FILES
                        ):
                            if c in self.convert_patterns_to_paths(
                                tmpextractdir.name,
                                [pattern],
                            ):
                                if len(pattern) > len(longest_include):
                                    longest_include = pattern

                        if len(longest_include) == len(longest_exclude):
                            # If it's a tie, give 'include' precedence'
                            remove_from_exclude.append(c)
                        elif len(longest_include) == 0 or len(longest_exclude) == 0:
                            raise Exception("Pattern didn't match both.")
                        elif len(longest_include) > len(longest_exclude):
                            remove_from_exclude.append(c)
                        else:
                            remove_from_include.append(c)

                    to_exclude = list(set(to_exclude) - set(remove_from_exclude))
                    to_include = list(set(to_include) - set(remove_from_include))

                if to_exclude:
                    self.logInfo(
                        {"files": list_of_paths_to_readable_string(to_exclude)},
                        "Removing: {files}",
                    )
                    for exclusion in to_exclude:
                        mozfile.remove(exclusion)

                # Clear out empty directories
                # removeEmpty() won't remove directories containing only empty directories
                #   so just keep callign it as long as it's doing something
                def removeEmpty(tmpextractdir):
                    removed = False
                    folders = list(os.walk(tmpextractdir))[1:]
                    for folder in folders:
                        if not folder[2]:
                            try:
                                os.rmdir(folder[0])
                                removed = True
                            except Exception:
                                pass
                    return removed

                while removeEmpty(tmpextractdir.name):
                    pass

                # Then copy over the directories
                if self.should_perform_step("move-contents"):
                    self.logInfo({"d": vendor_dir}, "Copying to {d}.")
                    mozfile.copy_contents(tmpextractdir.name, vendor_dir)
                else:
                    self.logInfo({}, "Skipping copying contents into tree.")
                    self._extract_directory = lambda: tmpextractdir.name
            except Exception as e:
                tmpextractdir.cleanup()
                raise e

    def update_yaml(self, revision, timestamp):
        with open(self.yaml_file) as f:
            yaml = f.readlines()

        replaced = 0
        replacements = [
            ["  release:", " %s (%s)." % (revision, timestamp)],
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

        with open(self.yaml_file, "wb") as f:
            f.write(("".join(yaml)).encode("utf-8"))

    def spurious_check(self, revision, ignore_modified):
        changed_files = set(
            [
                os.path.abspath(f)
                for f in self.repository.get_changed_files(mode="staged")
            ]
        )
        generated_files = set(
            [
                self.get_full_path(f)
                for f in self.manifest["vendoring"].get("generated", [])
            ]
        )
        changed_files = set(changed_files) - generated_files
        if not changed_files:
            self.logInfo({"r": revision}, "Upstream {r} hasn't modified files locally.")
            # We almost certainly won't be here if ignore_modified was passed, because a modified
            # local file will show up as a changed_file, but we'll be safe anyway.
            if not ignore_modified and generated_files:
                for g in generated_files:
                    self.repository.clean_directory(g)
            elif generated_files:
                self.log(
                    logging.CRITICAL,
                    "vendor",
                    {"files": generated_files},
                    "Because you passed --ignore-modified we are not cleaning your"
                    + " working directory, but the following files were probably"
                    + " spuriously edited and can be reverted: {files}",
                )
            sys.exit(-2)

        self.logInfo(
            {"rev": revision, "num": len(changed_files)},
            "Version '{rev}' has changed {num} files.",
        )

    def update_files(self, revision):
        if "update-actions" not in self.manifest["vendoring"]:
            return

        for update in self.manifest["vendoring"]["update-actions"]:
            if update["action"] == "copy-file":
                src = self.get_full_path(update["from"])
                dst = self.get_full_path(update["to"])

                self.logInfo(
                    {"s": src, "d": dst}, "action: copy-file src: {s} dst: {d}"
                )

                with open(src) as f:
                    contents = f.read()
                with open(dst, "w") as f:
                    f.write(contents)
            elif update["action"] == "move-file":
                src = self.get_full_path(update["from"])
                dst = self.get_full_path(update["to"])

                self.logInfo(
                    {"s": src, "d": dst}, "action: move-file src: {s} dst: {d}"
                )

                shutil.move(src, dst)
            elif update["action"] == "move-dir":
                src = self.get_full_path(update["from"])
                dst = self.get_full_path(update["to"])

                self.logInfo(
                    {"src": src, "dst": dst}, "action: move-dir src: {src} dst: {dst}"
                )

                if not os.path.isdir(src):
                    raise Exception(
                        "Cannot move from a source directory %s that is not a directory"
                        % src
                    )
                os.makedirs(dst, exist_ok=True)

                def copy_tree(src, dst):
                    names = os.listdir(src)
                    os.makedirs(dst, exist_ok=True)

                    for name in names:
                        srcname = os.path.join(src, name)
                        dstname = os.path.join(dst, name)

                        if os.path.isdir(srcname):
                            copy_tree(srcname, dstname)
                        else:
                            shutil.copy2(srcname, dstname)

                copy_tree(src, dst)
                shutil.rmtree(src)

            elif update["action"] in ["replace-in-file", "replace-in-file-regex"]:
                file = self.get_full_path(update["file"])

                self.logInfo({"file": file}, "action: replace-in-file file: {file}")

                replacement = update["with"].replace("{revision}", revision)
                _replace_in_file(
                    file,
                    update["pattern"],
                    replacement,
                    regex=update["action"] == "replace-in-file-regex",
                )
            elif update["action"] == "delete-path":
                path = self.get_full_path(update["path"])
                self.logInfo({"path": path}, "action: delete-path path: {path}")
                mozfile.remove(path)
            elif update["action"] in ["run-script", "run-command"]:
                if update["action"] == "run-script":
                    command = self.get_full_path(update["script"], support_cwd=True)
                else:
                    command = update["command"]

                run_dir = self.get_full_path(update["cwd"], support_cwd=True)

                args = []
                for a in update.get("args", []):
                    if a == "{revision}":
                        args.append(revision)
                    elif any(
                        s in a
                        for s in [
                            "{cwd}",
                            "{vendor_dir}",
                            "{yaml_dir}",
                            "{tmpextractdir}",
                        ]
                    ):
                        args.append(self.get_full_path(a, support_cwd=True))
                    else:
                        args.append(a)

                self.logInfo(
                    {
                        "command": command,
                        "run_dir": run_dir,
                        "args": args,
                        "type": update["action"],
                    },
                    "action: {type} command: {command} working dir: {run_dir} args: {args}",
                )
                extra_env = (
                    {"GECKO_PATH": os.getcwd()}
                    if "GECKO_PATH" not in os.environ
                    else {}
                )
                # We also add a signal to scripts that they are running under mach vendor
                extra_env["MACH_VENDOR"] = "1"
                self.run_process(
                    args=[command] + args,
                    cwd=run_dir,
                    log_name=command,
                    require_unix_environment=True,
                    append_env=extra_env,
                )
            else:
                assert False, "Unknown action supplied (how did this pass validation?)"

    def update_moz_build(self, vendoring_dir, moz_yaml_dir, add_to_exports):
        if vendoring_dir == moz_yaml_dir:
            vendoring_dir = moz_yaml_dir = None

        # If you edit this (especially for header files) you should double check
        # rewrite_mozbuild.py around 'assignment_type'
        source_suffixes = [".cc", ".c", ".cpp", ".S", ".asm"]
        header_suffixes = [".h", ".hpp"]

        files_removed = self.repository.get_changed_files(diff_filter="D")
        files_added = self.repository.get_changed_files(diff_filter="A")

        # Filter the files added to just source files we track in moz.build files.
        files_added = [
            f for f in files_added if any([f.endswith(s) for s in source_suffixes])
        ]
        header_files_to_add = [
            f for f in files_added if any([f.endswith(s) for s in header_suffixes])
        ]
        if add_to_exports:
            files_added += header_files_to_add
        elif header_files_to_add:
            self.log(
                logging.WARNING,
                "header_files_warning",
                {},
                (
                    "We found %s header files in the update, pass --add-to-exports if you want"
                    + " to attempt to include them in EXPORTS blocks: %s"
                )
                % (len(header_files_to_add), header_files_to_add),
            )

        self.logInfo(
            {"added": len(files_added), "removed": len(files_removed)},
            "Found {added} files added and {removed} files removed.",
        )

        should_abort = False
        for f in files_added:
            try:
                add_file_to_moz_build_file(f, moz_yaml_dir, vendoring_dir)
            except MozBuildRewriteException:
                self.log(
                    logging.ERROR,
                    "vendor",
                    {},
                    "Could not add %s to the appropriate moz.build file" % f,
                )
                should_abort = True

        for f in files_removed:
            try:
                remove_file_from_moz_build_file(f, moz_yaml_dir, vendoring_dir)
            except MozBuildRewriteException:
                self.log(
                    logging.ERROR,
                    "vendor",
                    {},
                    "Could not remove %s from the appropriate moz.build file" % f,
                )
                should_abort = True

        if should_abort:
            self.log(
                logging.ERROR,
                "vendor",
                {},
                "This is a deficiency in ./mach vendor . "
                + "Please review the affected files before committing.",
            )
            # Exit with -1 to distinguish this from the Exception case of exiting with 1
            sys.exit(-1)

    def import_local_patches(self, patches, yaml_dir, vendor_dir):
        self.logInfo({}, "Importing local patches...")
        for patch in self.convert_patterns_to_paths(yaml_dir, patches):
            script = [
                "patch",
                "-p1",
                "--directory",
                vendor_dir,
                "--input",
                os.path.abspath(patch),
                "--no-backup-if-mismatch",
            ]
            self.run_process(
                args=script,
                log_name=script,
            )
