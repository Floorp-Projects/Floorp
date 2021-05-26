# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

# Utility package for working with moz.yaml files.
#
# Requires `pyyaml` and `voluptuous`
# (both are in-tree under third_party/python)

from __future__ import absolute_import, print_function, unicode_literals

import errno
import os
import re
import sys

HERE = os.path.abspath(os.path.dirname(__file__))
lib_path = os.path.join(HERE, "..", "..", "..", "third_party", "python")
sys.path.append(os.path.join(lib_path, "voluptuous"))
sys.path.append(os.path.join(lib_path, "pyyaml", "lib"))

import voluptuous
import yaml
from voluptuous import (
    All,
    Boolean,
    FqdnUrl,
    Length,
    Match,
    Msg,
    Required,
    Schema,
    Unique,
    In,
    Invalid,
)
from yaml.error import MarkedYAMLError

# TODO ensure this matches the approved list of licenses
VALID_LICENSES = [
    # Standard Licenses (as per https://spdx.org/licenses/)
    "Apache-2.0",
    "BSD-2-Clause",
    "BSD-3-Clause",
    "BSD-3-Clause-Clear",
    "CC0-1.0",
    "ISC",
    "ICU",
    "LGPL-2.1",
    "LGPL-3.0",
    "MIT",
    "MPL-1.1",
    "MPL-2.0",
    "Unlicense",
    "WTFPL",
    "Zlib",
    # Unique Licenses
    "ACE",  # http://www.cs.wustl.edu/~schmidt/ACE-copying.html
    "Anti-Grain-Geometry",  # http://www.antigrain.com/license/index.html
    "JPNIC",  # https://www.nic.ad.jp/ja/idn/idnkit/download/index.html
    "Khronos",  # https://www.khronos.org/openmaxdl
    "libpng",  # http://www.libpng.org/pub/png/src/libpng-LICENSE.txt
    "Unicode",  # http://www.unicode.org/copyright.html
]

VALID_SOURCE_HOSTS = ["gitlab", "googlesource", "github"]

"""
---
# Third-Party Library Template
# All fields are mandatory unless otherwise noted

# Version of this schema
schema: 1

bugzilla:
  # Bugzilla product and component for this directory and subdirectories
  product: product name
  component: component name

# Document the source of externally hosted code
origin:

  # Short name of the package/library
  name: name of the package

  description: short (one line) description

  # Full URL for the package's homepage/etc
  # Usually different from repository url
  url: package's homepage url

  # Human-readable identifier for this version/release
  # Generally "version NNN", "tag SSS", "bookmark SSS"
  release: identifier

  # Revision to pull in
  # Must be a long or short commit SHA (long preferred)
  revision: sha

  # The package's license, where possible using the mnemonic from
  # https://spdx.org/licenses/
  # Multiple licenses can be specified (as a YAML list)
  # A "LICENSE" file must exist containing the full license text
  license: MPL-2.0

  # If the package's license is specified in a particular file,
  # this is the name of the file.
  # optional
  license-file: COPYING

# Configuration for automatic updating system.
# optional
updatebot:

  # TODO: allow multiple users to be specified
  # Phabricator username for a maintainer of the library, used for assigning
  # reviewers
  maintainer-phab: tjr

  # Bugzilla email address for a maintainer of the library, used for needinfos
  maintainer-bz: tom@mozilla.com

  # The tasks that Updatebot can run. Only one of each task is currently permitted
  # optional
  tasks:
    - type: commit-alert
      branch: upstream-branch-name
      cc: ["bugzilla@email.address", "another@example.com"]
      needinfo: ["bugzilla@email.address", "another@example.com"]
      enabled: True
      filter: security
      frequency: every
    - type: vendoring
      branch: master
      enabled: False
      frequency: 2 weeks

# Configuration for the automated vendoring system.
# optional
vendoring:

  # Repository URL to vendor from
  # eg. https://github.com/kinetiknz/nestegg.git
  # Any repository host can be specified here, however initially we'll only
  # support automated vendoring from selected sources initially.
  url: source url (generally repository clone url)

  # Type of hosting for the upstream repository
  # Valid values are 'gitlab', 'github'
  source-hosting: gitlab

  # Base directory of the location where the source files will live in-tree.
  # If omitted, will default to the location the moz.yaml file is in.
  vendor-directory: third_party/directory

  # List of patch files to apply after vendoring. Applied in the order
  # specified, and alphabetically if globbing is used. Patches must apply
  # cleanly before changes are pushed
  # All patch files are implicitly added to the keep file list.
  # optional
  patches:
    - file
    - path/to/file
    - path/*.patch

  # List of files that are not deleted while vendoring
  # Implicitly contains "moz.yaml", any files referenced as patches
  # optional
  keep:
    - file
    - path/to/file
    - another/path
    - *.mozilla

  # Files/paths that will not be vendored from source repository
  # Implicitly contains ".git", and ".gitignore"
  # optional
  exclude:
    - file
    - path/to/file
    - another/path
    - docs
    - src/*.test

  # Files/paths that will always be vendored, even if they would
  # otherwise be excluded by "exclude".
  # optional
  include:
    - file
    - path/to/file
    - another/path
    - docs/LICENSE.*

  # If neither "exclude" or "include" are set, all files will be vendored
  # Files/paths in "include" will always be vendored, even if excluded
  # eg. excluding "docs/" then including "docs/LICENSE" will vendor just the
  #     LICENSE file from the docs directory

  # All three file/path parameters ("keep", "exclude", and "include") support
  # filenames, directory names, and globs/wildcards.

  # Actions to take after updating. Applied in order.
  # The action subfield is required. It must be one of:
  #   - copy-file
  #   - replace-in-file
  #   - delete-path
  #   - run-script
  # Unless otherwise noted, all subfields of action are required.
  #
  # If the action is copy-file:
  #   from is the source file
  #   to is the destination
  #
  # If the action is replace-in-file:
  #   pattern is what in the file to search for. It is an exact strng match.
  #   with is the string to replace it with. Accepts the special keyword
  #     '{revision}' for the commit we are updating to.
  #   File is the file to replace it in.
  #
  # If the action is delete-path
  #   path is the file or directory to recursively delete
  #
  # If the action is run-script:
  #   script is the script to run
  #   cwd is the directory the script should run with as its cwd
  #
  # Unless specified otherwise, all files/directories are relative to the
  #     vendor-directory. If the vendor-directory is different from the
  #     directory of the yaml file, the keyword '{yaml_dir}' may be used
  #     to make the path relative to that directory.
  # 'run-script' supports the addictional keyword {cwd} which, if used,
  #     must only be used at the beginning of the path.
  #
  # optional
  update-actions:
    - action: copy-file
      from: include/vcs_version.h.in
      to: '{yaml_dir}/vcs_version.h'

    - action: replace-in-file
      pattern: '@VCS_TAG@'
      with: '{revision}'
      file: '{yaml_dir}/vcs_version.h'

    - action: delete-path
      path: '{yaml_dir}/config'

    - action: run-script
      script: '{cwd}/generate_sources.sh'
      cwd: '{yaml_dir}'
"""

RE_SECTION = re.compile(r"^(\S[^:]*):").search
RE_FIELD = re.compile(r"^\s\s([^:]+):\s+(\S+)$").search


class MozYamlVerifyError(Exception):
    def __init__(self, filename, error):
        self.filename = filename
        self.error = error

    def __str__(self):
        return "%s: %s" % (self.filename, self.error)


def load_moz_yaml(filename, verify=True, require_license_file=True):
    """Loads and verifies the specified manifest."""

    # Load and parse YAML.
    try:
        with open(filename, "r") as f:
            manifest = yaml.safe_load(f)
    except IOError as e:
        if e.errno == errno.ENOENT:
            raise MozYamlVerifyError(filename, "Failed to find manifest: %s" % filename)
        raise
    except MarkedYAMLError as e:
        raise MozYamlVerifyError(filename, e)

    if not verify:
        return manifest

    # Verify schema.
    if "schema" not in manifest:
        raise MozYamlVerifyError(filename, 'Missing manifest "schema"')
    if manifest["schema"] == 1:
        schema = _schema_1()
        schema_additional = _schema_1_additional
    else:
        raise MozYamlVerifyError(filename, "Unsupported manifest schema")

    try:
        schema(manifest)
        schema_additional(filename, manifest, require_license_file=require_license_file)
    except (voluptuous.Error, ValueError) as e:
        raise MozYamlVerifyError(filename, e)

    return manifest


def update_moz_yaml(filename, release, revision, verify=True, write=True):
    """Update origin:release and vendoring:revision without stripping
    comments or reordering fields."""

    if verify:
        load_moz_yaml(filename)

    lines = []
    with open(filename) as f:
        found_release = False
        found_revision = False
        section = None
        for line in f.readlines():
            m = RE_SECTION(line)
            if m:
                section = m.group(1)
            else:
                m = RE_FIELD(line)
                if m:
                    (name, value) = m.groups()
                    if section == "origin" and name == "release":
                        line = "  release: %s\n" % release
                        found_release = True
                    elif section == "origin" and name == "revision":
                        line = "  revision: %s\n" % revision
                        found_revision = True
            lines.append(line)

        if not found_release and found_revision:
            raise ValueError("Failed to find origin:release and " "origin:revision")

    if write:
        with open(filename, "w") as f:
            f.writelines(lines)


def _schema_1():
    """Returns Voluptuous Schema object."""
    return Schema(
        {
            Required("schema"): 1,
            Required("bugzilla"): {
                Required("product"): All(str, Length(min=1)),
                Required("component"): All(str, Length(min=1)),
            },
            "origin": {
                Required("name"): All(str, Length(min=1)),
                Required("description"): All(str, Length(min=1)),
                Required("url"): FqdnUrl(),
                Required("license"): Msg(License(), msg="Unsupported License"),
                "license-file": All(str, Length(min=1)),
                Required("release"): All(str, Length(min=1)),
                # The following regex defines a valid git reference
                # The first group [^ ~^:?*[\]] matches 0 or more times anything
                # that isn't a Space, ~, ^, :, ?, *, or ]
                # The second group [^ ~^:?*[\]\.]+ matches 1 or more times
                # anything that isn't a Space, ~, ^, :, ?, *, [, ], or .
                Required("revision"): Match(r"^[^ ~^:?*[\]]*[^ ~^:?*[\]\.]+$"),
            },
            "updatebot": {
                Required("maintainer-phab"): All(str, Length(min=1)),
                Required("maintainer-bz"): All(str, Length(min=1)),
                "tasks": All(
                    UpdatebotTasks(),
                    [
                        {
                            Required("type"): In(
                                [
                                    "vendoring",
                                    "commit-alert",
                                ],
                                msg="Invalid type specified in tasks",
                            ),
                            "branch": All(str, Length(min=1)),
                            "enabled": Boolean(),
                            "cc": Unique([str]),
                            "needinfo": Unique([str]),
                            "filter": In(
                                [
                                    "none",
                                    "security",
                                    "source-extensions",
                                ],
                                msg="Invalid filter value specified in tasks",
                            ),
                            "source-extensions": Unique([str]),
                            "frequency": Match(r"^(every|release|[0-9]+ weeks?)$"),
                        }
                    ],
                ),
            },
            "vendoring": {
                Required("url"): FqdnUrl(),
                Required("source-hosting"): All(
                    str,
                    Length(min=1),
                    In(VALID_SOURCE_HOSTS, msg="Unsupported Source Hosting"),
                ),
                "vendor-directory": All(str, Length(min=1)),
                "patches": Unique([str]),
                "keep": Unique([str]),
                "exclude": Unique([str]),
                "include": Unique([str]),
                "update-actions": All(
                    UpdateActions(),
                    [
                        {
                            Required("action"): In(
                                [
                                    "copy-file",
                                    "replace-in-file",
                                    "run-script",
                                    "delete-path",
                                ],
                                msg="Invalid action specified in update-actions",
                            ),
                            "from": All(str, Length(min=1)),
                            "to": All(str, Length(min=1)),
                            "pattern": All(str, Length(min=1)),
                            "with": All(str, Length(min=1)),
                            "file": All(str, Length(min=1)),
                            "script": All(str, Length(min=1)),
                            "cwd": All(str, Length(min=1)),
                            "path": All(str, Length(min=1)),
                        }
                    ],
                ),
            },
        }
    )


def _schema_1_additional(filename, manifest, require_license_file=True):
    """Additional schema/validity checks"""

    vendor_directory = os.path.dirname(filename)
    if "vendoring" in manifest and "vendor-directory" in manifest["vendoring"]:
        vendor_directory = manifest["vendoring"]["vendor-directory"]

    # LICENSE file must exist.
    if require_license_file and "origin" in manifest:
        files = [f.lower() for f in os.listdir(vendor_directory)]
        if not (
            "license-file" in manifest["origin"]
            and manifest["origin"]["license-file"].lower() in files
        ) and not (
            "license" in files
            or "license.txt" in files
            or "license.rst" in files
            or "license.html" in files
            or "license.md" in files
        ):
            license = manifest["origin"]["license"]
            if isinstance(license, list):
                license = "/".join(license)
            raise ValueError("Failed to find %s LICENSE file" % license)

    # Cannot vendor without an origin.
    if "vendoring" in manifest and "origin" not in manifest:
        raise ValueError('"vendoring" requires an "origin"')

    # If there are Updatebot tasks, then certain fields must be present
    if "updatebot" in manifest and "tasks" in manifest["updatebot"]:
        if "vendoring" not in manifest or "url" not in manifest["vendoring"]:
            raise ValueError(
                "If Updatebot tasks are specified, a vendoring url must be included."
            )
        if "origin" not in manifest or "revision" not in manifest["origin"]:
            raise ValueError(
                "If Updatebot tasks are specified, an origin revision must be specified."
            )

    # Check for a simple YAML file
    with open(filename, "r") as f:
        has_schema = False
        for line in f.readlines():
            m = RE_SECTION(line)
            if m:
                if m.group(1) == "schema":
                    has_schema = True
                    break
        if not has_schema:
            raise ValueError("Not simple YAML")

    # Verify YAML can be updated.
    if "vendor" in manifest:
        update_moz_yaml(filename, "", "", verify=False, write=True)


class UpdateActions(object):
    """Voluptuous validator which verifies the update actions(s) are valid."""

    def __call__(self, values):
        for v in values:
            if "action" not in v:
                raise Invalid("All file-update entries must specify a valid action")
            if v["action"] == "copy-file":
                if "from" not in v or "to" not in v or len(v.keys()) != 3:
                    raise Invalid(
                        "copy-file action must (only) specify 'from' and 'to' keys"
                    )
            elif v["action"] == "replace-in-file":
                if (
                    "pattern" not in v
                    or "with" not in v
                    or "file" not in v
                    or len(v.keys()) != 4
                ):
                    raise Invalid(
                        "replace-in-file action must (only) specify "
                        + "'pattern', 'with', and 'file' keys"
                    )
            elif v["action"] == "delete-path":
                if "path" not in v or len(v.keys()) != 2:
                    raise Invalid(
                        "delete-path action must (only) specify the 'path' key"
                    )
            elif v["action"] == "run-script":
                if "script" not in v or "cwd" not in v or len(v.keys()) != 3:
                    raise Invalid(
                        "run-script action must (only) specify 'script' and 'cwd' keys"
                    )
            else:
                # This check occurs before the validator above, so the above is
                # redundant but we leave it to be verbose.
                raise Invalid("Supplied action " + v["action"] + " is invalid.")
        return values

    def __repr__(self):
        return "UpdateActions"


class UpdatebotTasks(object):
    """Voluptuous validator which verifies the updatebot task(s) are valid."""

    def __call__(self, values):
        seenTaskTypes = set()
        for v in values:
            if "type" not in v:
                raise Invalid("All updatebot tasks must specify a valid type")

            if v["type"] in seenTaskTypes:
                raise Invalid("Only one type of each task is currently supported")
            seenTaskTypes.add(v["type"])

            if v["type"] == "vendoring":
                if "filter" in v or "source-extensions" in v:
                    raise Invalid(
                        "'filter' and 'source-extensions' only valid for commit-alert task types"
                    )
            elif v["type"] == "commit-alert":
                pass
            else:
                # This check occurs before the validator above, so the above is
                # redundant but we leave it to be verbose.
                raise Invalid("Supplied type " + v["type"] + " is invalid.")
        return values

    def __repr__(self):
        return "UpdatebotTasks"


class License(object):
    """Voluptuous validator which verifies the license(s) are valid as per our
    allow list."""

    def __call__(self, values):
        if isinstance(values, str):
            values = [values]
        elif not isinstance(values, list):
            raise Invalid("Must be string or list")
        for v in values:
            if v not in VALID_LICENSES:
                raise Invalid("Bad License")
        return values

    def __repr__(self):
        return "License"
