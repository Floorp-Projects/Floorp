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
lib_path = os.path.join(HERE, '..', '..', '..', 'third_party', 'python')
sys.path.append(os.path.join(lib_path, 'voluptuous'))
sys.path.append(os.path.join(lib_path, 'pyyaml', 'lib'))

import voluptuous
import yaml
from voluptuous import (All, FqdnUrl, Length, Match, Msg, Required, Schema,
                        Unique, )
from yaml.error import MarkedYAMLError

# TODO ensure this matches the approved list of licenses
VALID_LICENSES = [
    # Standard Licenses (as per https://spdx.org/licenses/)
    'Apache-2.0',
    'BSD-2-Clause',
    'BSD-3-Clause-Clear',
    'GPL-3.0',
    'ISC',
    'ICU',
    'LGPL-2.1',
    'LGPL-3.0',
    'MIT',
    'MPL-1.1',
    'MPL-2.0',
    # Unique Licenses
    'ACE',  # http://www.cs.wustl.edu/~schmidt/ACE-copying.html
    'Anti-Grain-Geometry',  # http://www.antigrain.com/license/index.html
    'JPNIC',  # https://www.nic.ad.jp/ja/idn/idnkit/download/index.html
    'Khronos',  # https://www.khronos.org/openmaxdl
    'Unicode',  # http://www.unicode.org/copyright.html
]

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

  # The package's license, where possible using the mnemonic from
  # https://spdx.org/licenses/
  # Multiple licenses can be specified (as a YAML list)
  # A "LICENSE" file must exist containing the full license text
  license: MPL-2.0

# Configuration for the automated vendoring system.
# Files are always vendored into a directory structure that matches the source
# repository, into the same directory as the moz.yaml file
# optional
vendoring:

  # Repository URL to vendor from
  # eg. https://github.com/kinetiknz/nestegg.git
  # Any repository host can be specified here, however initially we'll only
  # support automated vendoring from selected sources initiall.
  url: source url (generally repository clone url)

  # Revision to pull in
  # Must be a long or short commit SHA (long preferred)
  revision: sha

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

  # In-tree scripts to be executed after vendoring but before pushing.
  # optional
  run_after:
    - script
    - another script
"""

RE_SECTION = re.compile(r'^(\S[^:]*):').search
RE_FIELD = re.compile(r'^\s\s([^:]+):\s+(\S+)$').search


class VerifyError(Exception):
    def __init__(self, filename, error):
        self.filename = filename
        self.error = error

    def __str__(self):
        return '%s: %s' % (self.filename, self.error)


def load_moz_yaml(filename, verify=True, require_license_file=True):
    """Loads and verifies the specified manifest."""

    # Load and parse YAML.
    try:
        with open(filename, 'r') as f:
            manifest = yaml.safe_load(f)
    except IOError as e:
        if e.errno == errno.ENOENT:
            raise VerifyError(filename,
                              'Failed to find manifest: %s' % filename)
        raise
    except MarkedYAMLError as e:
        raise VerifyError(filename, e)

    if not verify:
        return manifest

    # Verify schema.
    if 'schema' not in manifest:
        raise VerifyError(filename, 'Missing manifest "schema"')
    if manifest['schema'] == 1:
        schema = _schema_1()
        schema_additional = _schema_1_additional
    else:
        raise VerifyError(filename, 'Unsupported manifest schema')

    try:
        schema(manifest)
        schema_additional(filename, manifest,
                          require_license_file=require_license_file)
    except (voluptuous.Error, ValueError) as e:
        raise VerifyError(filename, e)

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
                    if section == 'origin' and name == 'release':
                        line = '  release: %s\n' % release
                        found_release = True
                    elif section == 'vendoring' and name == 'revision':
                        line = '  revision: %s\n' % revision
                        found_revision = True
            lines.append(line)

        if not found_release and found_revision:
            raise ValueError('Failed to find origin:release and '
                             'vendoring:revision')

    if write:
        with open(filename, 'w') as f:
            f.writelines(lines)


def _schema_1():
    """Returns Voluptuous Schema object."""
    return Schema({
        Required('schema'): 1,
        Required('bugzilla'): {
            Required('product'): All(str, Length(min=1)),
            Required('component'): All(str, Length(min=1)),
        },
        'origin': {
            Required('name'): All(str, Length(min=1)),
            Required('description'): All(str, Length(min=1)),
            Required('url'): FqdnUrl(),
            Required('license'): Msg(License(), msg='Unsupported License'),
            Required('release'): All(str, Length(min=1)),
        },
        'vendoring': {
            Required('url'): FqdnUrl(),
            Required('revision'): Match(r'^[a-fA-F0-9]{12,40}$'),
            'patches': Unique([str]),
            'keep': Unique([str]),
            'exclude': Unique([str]),
            'include': Unique([str]),
            'run_after': Unique([str]),
        },
    })


def _schema_1_additional(filename, manifest, require_license_file=True):
    """Additional schema/validity checks"""

    # LICENSE file must exist.
    if require_license_file and 'origin' in manifest:
        files = [f.lower() for f in os.listdir(os.path.dirname(filename))
                 if f.lower().startswith('license')]
        if not ('license' in files
                or 'license.txt' in files
                or 'license.rst' in files
                or 'license.html' in files
                or 'license.md' in files):
            license = manifest['origin']['license']
            if isinstance(license, list):
                license = '/'.join(license)
            raise ValueError('Failed to find %s LICENSE file' % license)

    # Cannot vendor without an origin.
    if 'vendoring' in manifest and 'origin' not in manifest:
        raise ValueError('"vendoring" requires an "origin"')

    # Check for a simple YAML file
    with open(filename, 'r') as f:
        has_schema = False
        for line in f.readlines():
            m = RE_SECTION(line)
            if m:
                if m.group(1) == 'schema':
                    has_schema = True
                    break
        if not has_schema:
            raise ValueError('Not simple YAML')

    # Verify YAML can be updated.
    if 'vendor' in manifest:
        update_moz_yaml(filename, '', '', verify=False, write=True)


class License(object):
    """Voluptuous validator which verifies the license(s) are valid as per our
    whitelist."""
    def __call__(self, values):
        if isinstance(values, str):
            values = [values]
        elif not isinstance(values, list):
            raise ValueError('Must be string or list')
        for v in values:
            if v not in VALID_LICENSES:
                raise ValueError('Bad License')
        return values

    def __repr__(self):
        return 'License'
