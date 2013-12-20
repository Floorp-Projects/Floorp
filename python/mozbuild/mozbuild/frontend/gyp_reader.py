# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals
import gyp
import sys
import time
import os
import mozpack.path as mozpath
from mozpack.files import FileFinder
from .sandbox import (
    alphabetical_sorted,
    GlobalNamespace,
)
from .sandbox_symbols import VARIABLES

# Define this module as gyp.generator.mozbuild so that gyp can use it
# as a generator under the name "mozbuild".
sys.modules['gyp.generator.mozbuild'] = sys.modules[__name__]

# build/gyp_chromium does this:
#   script_dir = os.path.dirname(os.path.realpath(__file__))
#   chrome_src = os.path.abspath(os.path.join(script_dir, os.pardir))
#   sys.path.insert(0, os.path.join(chrome_src, 'tools', 'gyp', 'pylib'))
# We're not importing gyp_chromium, but we want both script_dir and
# chrome_src for the default includes, so go backwards from the pylib
# directory, which is the parent directory of gyp module.
chrome_src = mozpath.abspath(mozpath.join(mozpath.dirname(gyp.__file__),
    '../../../..'))
script_dir = mozpath.join(chrome_src, 'build')

# Default variables gyp uses when evaluating gyp files.
generator_default_variables = {
}
for dirname in ['INTERMEDIATE_DIR', 'SHARED_INTERMEDIATE_DIR', 'PRODUCT_DIR',
                'LIB_DIR', 'SHARED_LIB_DIR']:
  # Some gyp steps fail if these are empty(!).
  generator_default_variables[dirname] = b'dir'

for unused in ['RULE_INPUT_PATH', 'RULE_INPUT_ROOT', 'RULE_INPUT_NAME',
               'RULE_INPUT_DIRNAME', 'RULE_INPUT_EXT',
               'EXECUTABLE_PREFIX', 'EXECUTABLE_SUFFIX',
               'STATIC_LIB_PREFIX', 'STATIC_LIB_SUFFIX',
               'SHARED_LIB_PREFIX', 'SHARED_LIB_SUFFIX',
               'LINKER_SUPPORTS_ICF']:
  generator_default_variables[unused] = b''


class GypSandbox(GlobalNamespace):
    """Class mimicking MozbuildSandbox for processing of the data
    extracted from Gyp by a mozbuild backend.

    Inherits from GlobalNamespace because it doesn't need the extra
    functionality from Sandbox.
    """
    def __init__(self, main_path, dependencies_paths=[]):
        self.main_path = main_path
        self.all_paths = set([main_path]) | set(dependencies_paths)
        self.execution_time = 0
        GlobalNamespace.__init__(self, allowed_variables=VARIABLES)

    def get_affected_tiers(self):
        tiers = (VARIABLES[key][3] for key in self if key in VARIABLES)
        return set(tier for tier in tiers if tier)


def encode(value):
    if isinstance(value, unicode):
        return value.encode('utf-8')
    return value


def read_from_gyp(config, path, output, vars):
    """Read a gyp configuration and emits GypSandboxes for the backend to
    process.

    config is a ConfigEnvironment, path is the path to a root gyp configuration
    file, output is the base path under which the objdir for the various gyp
    dependencies will be, and vars a dict of variables to pass to the gyp
    processor.
    """

    time_start = time.time()

    # gyp expects plain str instead of unicode. The frontend code gives us
    # unicode strings, so convert them.
    path = encode(path)
    str_vars = dict((name, encode(value)) for name, value in vars.items())

    params = {
        b'parallel': False,
        b'generator_flags': {},
        b'build_files': [path],
    }

    # Files that gyp_chromium always includes
    includes = [encode(mozpath.join(script_dir, 'common.gypi'))]
    finder = FileFinder(chrome_src, find_executables=False)
    includes.extend(encode(mozpath.join(chrome_src, name))
        for name, _ in finder.find('*/supplement.gypi'))

    # Read the given gyp file and its dependencies.
    generator, flat_list, targets, data = \
        gyp.Load([path], format=b'mozbuild',
            default_variables=str_vars,
            includes=includes,
            depth=encode(mozpath.dirname(path)),
            params=params)

    # Process all targets from the given gyp files and its dependencies.
    # The path given to AllTargets needs to use os.sep, while the frontend code
    # gives us paths normalized with forward slash separator.
    for target in gyp.common.AllTargets(flat_list, targets, path.replace(b'/', os.sep)):
        build_file, target_name, toolset = gyp.common.ParseQualifiedTarget(target)
        # The list of included files returned by gyp are relative to build_file
        included_files = [mozpath.abspath(mozpath.join(mozpath.dirname(build_file), f))
                          for f in data[build_file]['included_files']]
        # Emit a sandbox for each target.
        sandbox = GypSandbox(mozpath.abspath(build_file), included_files)

        with sandbox.allow_all_writes() as d:
            topsrcdir = d['TOPSRCDIR'] = config.topsrcdir
            d['TOPOBJDIR'] = config.topobjdir
            relsrcdir = d['RELATIVEDIR'] = mozpath.relpath(mozpath.dirname(build_file), config.topsrcdir)
            d['SRCDIR'] = mozpath.join(topsrcdir, relsrcdir)

            # Each target is given its own objdir. The base of that objdir
            # is derived from the relative path from the root gyp file path
            # to the current build_file, placed under the given output
            # directory. Since several targets can be in a given build_file,
            # separate them in subdirectories using the build_file basename
            # and the target_name.
            reldir  = mozpath.relpath(mozpath.dirname(build_file),
                                      mozpath.dirname(path))
            subdir = '%s_%s' % (
                mozpath.splitext(mozpath.basename(build_file))[0],
                target_name,
            )
            d['OBJDIR'] = mozpath.join(output, reldir, subdir)
            d['IS_GYP_DIR'] = True

        spec = targets[target]

        # Derive which gyp configuration to use based on MOZ_DEBUG.
        c = 'Debug' if config.substs['MOZ_DEBUG'] else 'Release'
        if c not in spec['configurations']:
            raise RuntimeError('Missing %s gyp configuration for target %s '
                               'in %s' % (c, target_name, build_file))
        target_conf = spec['configurations'][c]

        if spec['type'] == 'none':
            continue
        elif spec['type'] == 'static_library':
            sandbox['FORCE_STATIC_LIB'] = True
            # Remove leading 'lib' from the target_name if any, and use as
            # library name.
            name = spec['target_name']
            if name.startswith('lib'):
                name = name[3:]
            # The sandbox expects an unicode string.
            sandbox['LIBRARY_NAME'] = name.decode('utf-8')
            # The sandbox expects alphabetical order when adding sources
            sources = alphabetical_sorted(spec.get('sources', []))
            # gyp files contain headers in sources lists.
            sandbox['SOURCES'] = \
                [f for f in sources if mozpath.splitext(f)[-1] != '.h']

            for define in target_conf.get('defines', []):
                if '=' in define:
                    name, value = define.split('=', 1)
                    sandbox['DEFINES'][name] = value
                else:
                    sandbox['DEFINES'][define] = True

            for include in target_conf.get('include_dirs', []):
                sandbox['LOCAL_INCLUDES'] += [include]

            with sandbox.allow_all_writes() as d:
                d['EXTRA_ASSEMBLER_FLAGS'] = target_conf.get('asflags_mozilla', [])
                d['EXTRA_COMPILE_FLAGS'] = target_conf.get('cflags_mozilla', [])
        else:
            # Ignore other types than static_library because we don't have
            # anything using them, and we're not testing them. They can be
            # added when that becomes necessary.
            raise NotImplementedError('Unsupported gyp target type: %s' % spec['type'])

        sandbox.execution_time = time.time() - time_start
        yield sandbox
        time_start = time.time()
