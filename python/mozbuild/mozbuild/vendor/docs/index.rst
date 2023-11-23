================================
Vendoring Third Party Components
================================

The firefox source tree vendors many third party dependencies. The build system
provides a normalized way to keep track of:

1. The upstream source license, location and revision

2. (Optionally) The upstream source modification, including

   1. Mozilla-specific patches

   2. Custom update actions, such as excluding some files, moving files around
      etc.

This is done through a descriptive ``moz.yaml`` file added to the third
party sources, and the use of:

.. code-block:: sh

    ./mach vendor [options] ./path/to/moz.yaml

to interact with it.


Template ``moz.yaml`` file
==========================

.. code-block:: yaml


    # All fields are mandatory unless otherwise noted

    schema: 1
    # Version of this schema

    bugzilla:
        # Bugzilla product and component for this directory and subdirectories.
        product: product name
        component: component name

    origin:

        name: name of the package
        # eg. nestegg

        description: short (one line) description

        url: package's homepage url
        # Usually different from repository url

        release: identifier
        # Human-readable identifier for this version/release
        # Generally "version NNN", "tag SSS", "bookmark SSS"

        revision: sha
        # Revision to pull in
        # Must be a long or short commit SHA (long preferred)

        license: MPL-2.0
        # Multiple licenses can be specified (as a YAML list)
        # Where possible using the mnemonic from https://spdx.org/licenses/
        # From a list of approved licenses (to be determined)
        # A "LICENSE" file must exist in the destination directory after patches are applied

        license-file: COPYING
        # optional
        # explicit name of the License file if it's not one of the supported
        # hard-coded value.

    vendoring:
        # optional
        # Information needed to update the library automatically.

        url: source url (generally repository clone url)
        # eg. https://github.com/kinetiknz/nestegg.git
        # Any repository host can be specified here, however initially we'll only support
        # automated vendoring from github.com and googlesource.com, to be extended later.

        source-hosting: gitlab
        # name of the infrastructure used to host the sources. Can be one of
        # gitlab, angle, googlesource, codeberg, github or git. The later is
        # more generic but less efficient.

        patches:
            - file
            - path/to/file
            - path/*.patch
        # optional
        # List of patch files to apply after vendoring. Applied in the order specified, and
        # alphabetically if globbing is used. Patches must apply cleanly before changes are
        # pushed
        # All patch files are implicitly added to the keep file list.

        keep:
            - file
            - path/to/file
            - another/path
            - *.mozilla
        # optional
        # List of files in mozilla-central that are not deleted while vendoring
        # Implicitly contains "moz.yaml", any files referenced as patches

        exclude:
            - file
            - path/to/file
            - another/path
            - docs
            - src/*.test
        # optional
        # Files/paths that will not be vendored from source repository
        # Implicitly contains ".git", and ".gitignore"

        include:
            - file
            - path/to/file
            - another/path
            - docs/LICENSE.*
        # optional
        # Files/paths that will always be vendored, even if they would
        # otherwise be excluded by "exclude".

        # If neither "exclude" or "include" are set, all files will be vendored
        # Files/paths in "include" will always be vendored, even if excluded
        # eg. excluding "docs/" then including "docs/LICENSE" will vendor just the LICENSE file
        # from the docs directory

        # All three file/path parameters ("keep", "exclude", and "include") support filenames,
        # directory names, and globs/wildcards.

        update-actions:
            - action: move-file
              from: '{vendor_dir}/origin'
              to: '{vendor_dir}/dest'

            - action: move-dir
              from: '{vendor_dir}/origin'
              to: '{vendor_dir}/dest'

            - action: copy-file
              from: '{vendor_dir}/origin'
              to: '{vendor_dir}/dest'

            - action: delete-path
              path: "src/unused"

            - action: replace-in-file
              pattern: '@REVISION@'
              with: '{revision}'
              file: '{yaml_dir}/vcs_version.h'

            - action: replace-in-file-regex
              file: '{vendor_dir}/lib/arm/armopts.s'
              pattern: '@HAVE_ARM_ASM_((EDSP)|(MEDIA)|(NEON))@'
              with: '1'

            - action: run-script
              script: '{yaml_dir}/update.sh'
              args: ['{revision}']
              cwd: '{cwd}'

        # optional
        # In-tree actions to be executed after vendoring but before pushing.


Common Vendoring Operations
===========================


Update to the latest upstream revision:

.. code-block:: sh

   ./mach vendor /path/to/moz.yaml


Check for latest revision, returning no output if it is up-to-date, and a
version identifier if it needs to be updated:

.. code-block:: sh

   ./mach vendor /path/to/moz.yaml --check-for-update

Vendor a specific revision:

.. code-block:: sh

   ./mach vendor /path/to/moz.yaml -r $REVISION --force


In the presence of patches, two steps are needed:

1. Vendor without applying patches (patches are applied *after*
   ``update-actions``) through ``--patch-mode none``

2. Apply patches on updated sources through ``--patch -mode only``

In the absence of patches, a single step is needed, and no extra argument is
required.
