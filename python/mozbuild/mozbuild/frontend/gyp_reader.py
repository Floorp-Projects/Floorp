# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import gyp
import sys
import os
import types
import mozpack.path as mozpath
from mozpack.files import FileFinder
from .sandbox import alphabetical_sorted
from .context import (
    SourcePath,
    TemplateContext,
    VARIABLES,
)
from mozbuild.util import (
    expand_variables,
    List,
    memoize,
)
from .reader import SandboxValidationError

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


class GypContext(TemplateContext):
    """Specialized Context for use with data extracted from Gyp.

    config is the ConfigEnvironment for this context.
    relobjdir is the object directory that will be used for this context,
    relative to the topobjdir defined in the ConfigEnvironment.
    """
    def __init__(self, config, relobjdir):
        self._relobjdir = relobjdir
        TemplateContext.__init__(self, template='Gyp',
            allowed_variables=VARIABLES, config=config)


def encode(value):
    if isinstance(value, unicode):
        return value.encode('utf-8')
    return value


def read_from_gyp(config, path, output, vars, non_unified_sources = set()):
    """Read a gyp configuration and emits GypContexts for the backend to
    process.

    config is a ConfigEnvironment, path is the path to a root gyp configuration
    file, output is the base path under which the objdir for the various gyp
    dependencies will be, and vars a dict of variables to pass to the gyp
    processor.
    """

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
        # Emit a context for each target.
        context = GypContext(config, mozpath.relpath(
            mozpath.join(output, reldir, subdir), config.topobjdir))
        context.add_source(mozpath.abspath(build_file))
        # The list of included files returned by gyp are relative to build_file
        for f in data[build_file]['included_files']:
            context.add_source(mozpath.abspath(mozpath.join(
                mozpath.dirname(build_file), f)))

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
            # Remove leading 'lib' from the target_name if any, and use as
            # library name.
            name = spec['target_name']
            if name.startswith('lib'):
                name = name[3:]
            # The context expects an unicode string.
            context['LIBRARY_NAME'] = name.decode('utf-8')
            # gyp files contain headers and asm sources in sources lists.
            sources = []
            unified_sources = []
            extensions = set()
            for f in spec.get('sources', []):
                ext = mozpath.splitext(f)[-1]
                extensions.add(ext)
                s = SourcePath(context, f)
                if ext == '.h':
                    continue
                if ext != '.S' and s not in non_unified_sources:
                    unified_sources.append(s)
                else:
                    sources.append(s)

            # The context expects alphabetical order when adding sources
            context['SOURCES'] = alphabetical_sorted(sources)
            context['UNIFIED_SOURCES'] = alphabetical_sorted(unified_sources)

            for define in target_conf.get('defines', []):
                if '=' in define:
                    name, value = define.split('=', 1)
                    context['DEFINES'][name] = value
                else:
                    context['DEFINES'][define] = True

            for include in target_conf.get('include_dirs', []):
                # moz.build expects all LOCAL_INCLUDES to exist, so ensure they do.
                #
                # NB: gyp files sometimes have actual absolute paths (e.g.
                # /usr/include32) and sometimes paths that moz.build considers
                # absolute, i.e. starting from topsrcdir. There's no good way
                # to tell them apart here, and the actual absolute paths are
                # likely bogus. In any event, actual absolute paths will be
                # filtered out by trying to find them in topsrcdir.
                if include.startswith('/'):
                    resolved = mozpath.abspath(mozpath.join(config.topsrcdir, include[1:]))
                else:
                    resolved = mozpath.abspath(mozpath.join(mozpath.dirname(build_file), include))
                if not os.path.exists(resolved):
                    continue
                context['LOCAL_INCLUDES'] += [include]

            context['ASFLAGS'] = target_conf.get('asflags_mozilla', [])
            flags = target_conf.get('cflags_mozilla', [])
            if flags:
                suffix_map = {
                    '.c': 'CFLAGS',
                    '.cpp': 'CXXFLAGS',
                    '.cc': 'CXXFLAGS',
                    '.m': 'CMFLAGS',
                    '.mm': 'CMMFLAGS',
                }
                variables = (
                    suffix_map[e]
                    for e in extensions if e in suffix_map
                )
                for var in variables:
                    for f in flags:
                        # We may be getting make variable references out of the
                        # gyp data, and we don't want those in emitted data, so
                        # substitute them with their actual value.
                        f = expand_variables(f, config.substs)
                        if not f:
                            continue
                        # the result may be a string or a list.
                        if isinstance(f, types.StringTypes):
                            context[var].append(f)
                        else:
                            context[var].extend(f)
        else:
            # Ignore other types than static_library because we don't have
            # anything using them, and we're not testing them. They can be
            # added when that becomes necessary.
            raise NotImplementedError('Unsupported gyp target type: %s' % spec['type'])

        # Add some features to all contexts. Put here in case LOCAL_INCLUDES
        # order matters.
        context['LOCAL_INCLUDES'] += [
            '!/ipc/ipdl/_ipdlheaders',
            '/ipc/chromium/src',
            '/ipc/glue',
        ]
        # These get set via VC project file settings for normal GYP builds.
        if config.substs['OS_TARGET'] == 'WINNT':
            context['DEFINES']['UNICODE'] = True
            context['DEFINES']['_UNICODE'] = True
        context['DISABLE_STL_WRAPPING'] = True

        yield context
