# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

from collections import defaultdict
from copy import deepcopy
import glob
import json
import os
import subprocess
import sys
import types

from mozbuild.backend.base import BuildBackend
import mozpack.path as mozpath
from mozbuild.frontend.sandbox import alphabetical_sorted
from mozbuild.frontend.data import GnProjectData
from mozbuild.util import expand_variables, mkdir


license_header = """# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""

generated_header = """
  ### This moz.build was AUTOMATICALLY GENERATED from a GN config,  ###
  ### DO NOT edit it by hand.                                       ###
"""


class MozbuildWriter(object):
    def __init__(self, fh):
        self._fh = fh
        self.indent = ''
        self._indent_increment = 4

        # We need to correlate a small amount of state here to figure out
        # which library template to use ("Library()" or "SharedLibrary()")
        self._library_name = None
        self._shared_library = None

    def mb_serialize(self, v):
        if isinstance(v, (bool, list)):
            return repr(v)
        return '"%s"' % v

    def finalize(self):
        if self._library_name:
            self.write('\n')
            if self._shared_library:
                self.write_ln("SharedLibrary(%s)" % self.mb_serialize(self._library_name))
            else:
                self.write_ln("Library(%s)" % self.mb_serialize(self._library_name))

    def write(self, content):
        self._fh.write(content)

    def write_ln(self, line):
        self.write(self.indent)
        self.write(line)
        self.write('\n')

    def write_attrs(self, context_attrs):
        for k, v in context_attrs.iteritems():
            if isinstance(v, (list, set)):
                self.write_mozbuild_list(k, alphabetical_sorted(v))
            elif isinstance(v, dict):
                self.write_mozbuild_dict(k, v)
            else:
                self.write_mozbuild_value(k, v)

    def write_mozbuild_list(self, key, value):
        if value:
            self.write('\n')
            self.write(self.indent + key)
            self.write(' += [\n    ' + self.indent)
            self.write((',\n    ' + self.indent).join(self.mb_serialize(v) for v in value))
            self.write('\n')
            self.write_ln(']')

    def write_mozbuild_value(self, key, value):
        if value:
            if key == 'LIBRARY_NAME':
                self._library_name = value
            elif key == 'FORCE_SHARED_LIB':
                self._shared_library = True
            else:
                self.write('\n')
                self.write_ln('%s = %s' % (key, self.mb_serialize(value)))
                self.write('\n')

    def write_mozbuild_dict(self, key, value):
        # Templates we need to use instead of certain values.
        replacements = (
            (('COMPILE_FLAGS', '"WARNINGS_AS_ERRORS"', '[]'), 'AllowCompilerWarnings()'),
        )
        if value:
            self.write('\n')
            for k, v in value.iteritems():
                subst_vals = key, self.mb_serialize(k), self.mb_serialize(v)
                wrote_ln = False
                for flags, tmpl in replacements:
                    if subst_vals == flags:
                        self.write_ln(tmpl)
                        wrote_ln = True

                if not wrote_ln:
                    self.write_ln("%s[%s] = %s" % subst_vals)


    def write_condition(self, values):
        def mk_condition(k, v):
            if not v:
                return 'not CONFIG["%s"]' % k
            return 'CONFIG["%s"] == %s' % (k, self.mb_serialize(v))

        self.write('\n')
        self.write('if ')
        self.write(' and '.join(mk_condition(k, v) for k, v in values.items()))
        self.write(':\n')
        self.indent += ' ' * self._indent_increment

    def terminate_condition(self):
        assert len(self.indent) >= self._indent_increment
        self.indent = self.indent[self._indent_increment:]


def find_deps(all_targets, target):
    all_deps = set([target])
    for dep in all_targets[target]['deps']:
        if dep not in all_deps:
            all_deps |= find_deps(all_targets, dep)
    return all_deps


def filter_gn_config(gn_result, config, sandbox_vars, input_vars):
    # Translates the raw output of gn into just what we'll need to generate a
    # mozbuild configuration.
    gn_out = {
        'targets': {},
        'sandbox_vars': sandbox_vars,
        'gn_gen_args': input_vars,
    }

    gn_mozbuild_vars = (
        'MOZ_DEBUG',
        'OS_TARGET',
        'HOST_CPU_ARCH',
        'CPU_ARCH',
    )

    mozbuild_args = {k: config.substs.get(k) for k in gn_mozbuild_vars}
    gn_out['mozbuild_args'] = mozbuild_args
    all_deps = find_deps(gn_result['targets'], "//:default")

    for target_fullname in all_deps:
        raw_spec = gn_result['targets'][target_fullname]

        # TODO: 'executable' will need to be handled here at some point as well.
        if raw_spec['type'] not in ('static_library', 'shared_library',
                                    'source_set'):
            continue

        spec = {}
        for spec_attr in ('type', 'sources', 'defines', 'include_dirs',
                          'cflags', 'deps', 'libs'):
            spec[spec_attr] = raw_spec.get(spec_attr, [])
            gn_out['targets'][target_fullname] = spec

    return gn_out


def process_gn_config(gn_config, srcdir, config, output, non_unified_sources,
                      sandbox_vars, mozilla_flags):
    # Translates a json gn config into attributes that can be used to write out
    # moz.build files for this configuration.

    # Much of this code is based on similar functionality in `gyp_reader.py`.

    mozbuild_attrs = {'mozbuild_args': gn_config.get('mozbuild_args', None),
                      'dirs': {}}

    targets = gn_config["targets"]

    project_relsrcdir = mozpath.relpath(srcdir, config.topsrcdir)

    def target_info(fullname):
        path, name = target_fullname.split(':')
        # Stripping '//' gives us a path relative to the project root,
        # adding a suffix avoids name collisions with libraries already
        # in the tree (like "webrtc").
        return path.lstrip('//'), name + '_gn'

    # Process all targets from the given gn project and its dependencies.
    for target_fullname, spec in targets.iteritems():

        target_path, target_name = target_info(target_fullname)
        context_attrs = {}

        # Remove leading 'lib' from the target_name if any, and use as
        # library name.
        name = target_name
        if spec['type'] in ('static_library', 'shared_library', 'source_set'):
            if name.startswith('lib'):
                name = name[3:]
            context_attrs['LIBRARY_NAME'] = name.decode('utf-8')
        else:
            raise Exception('The following GN target type is not currently '
                            'consumed by moz.build: "%s". It may need to be '
                            'added, or you may need to re-run the '
                            '`GnConfigGen` step.' % spec['type'])

        if spec['type'] == 'shared_library':
            context_attrs['FORCE_SHARED_LIB'] = True

        sources = []
        unified_sources = []
        extensions = set()
        use_defines_in_asflags = False

        for f in spec.get('sources', []):
            f = f.lstrip("//")
            ext = mozpath.splitext(f)[-1]
            extensions.add(ext)
            src = '%s/%s' % (project_relsrcdir, f)
            if ext == '.h':
                continue
            elif ext == '.def':
                context_attrs['SYMBOLS_FILE'] = src
            elif ext != '.S' and src not in non_unified_sources:
                unified_sources.append('/%s' % src)
            else:
                sources.append('/%s' % src)
            # The Mozilla build system doesn't use DEFINES for building
            # ASFILES.
            if ext == '.s':
                use_defines_in_asflags = True

        context_attrs['SOURCES'] = sources
        context_attrs['UNIFIED_SOURCES'] = unified_sources

        context_attrs['DEFINES'] = {}
        for define in spec.get('defines', []):
            if '=' in define:
                name, value = define.split('=', 1)
                context_attrs['DEFINES'][name] = value
            else:
                context_attrs['DEFINES'][define] = True

        context_attrs['LOCAL_INCLUDES'] = []
        for include in spec.get('include_dirs', []):
            # GN will have resolved all these paths relative to the root of
            # the project indicated by "//".
            if include.startswith('//'):
                include = include[2:]
            # moz.build expects all LOCAL_INCLUDES to exist, so ensure they do.
            if include.startswith('/'):
                resolved = mozpath.abspath(mozpath.join(config.topsrcdir, include[1:]))
            else:
                resolved = mozpath.abspath(mozpath.join(srcdir, include))
            if not os.path.exists(resolved):
                # GN files may refer to include dirs that are outside of the
                # tree or we simply didn't vendor. Print a warning in this case.
                if not resolved.endswith('gn-output/gen'):
                    print("Included path: '%s' does not exist, dropping include from GN "
                          "configuration." % resolved, file=sys.stderr)
                continue
            if not include.startswith('/'):
                include = '/%s/%s' % (project_relsrcdir, include)
            context_attrs['LOCAL_INCLUDES'] += [include]

        context_attrs['ASFLAGS'] = spec.get('asflags_mozilla', [])
        if use_defines_in_asflags and defines:
            context_attrs['ASFLAGS'] += ['-D' + d for d in defines]
        flags = [f for f in spec.get('cflags', []) if f in mozilla_flags]
        if flags:
            suffix_map = {
                '.c': 'CFLAGS',
                '.cpp': 'CXXFLAGS',
                '.cc': 'CXXFLAGS',
                '.m': 'CMFLAGS',
                '.mm': 'CMMFLAGS',
            }
            variables = (suffix_map[e] for e in extensions if e in suffix_map)
            for var in variables:
                for f in flags:
                    # We may be getting make variable references out of the
                    # gn data, and we don't want those in emitted data, so
                    # substitute them with their actual value.
                    f = expand_variables(f, config.substs).split()
                    if not f:
                        continue
                    # the result may be a string or a list.
                    if isinstance(f, types.StringTypes):
                        context_attrs.setdefault(var, []).append(f)
                    else:
                        context_attrs.setdefault(var, []).extend(f)

        context_attrs['OS_LIBS'] = []
        for lib in spec.get('libs', []):
            lib_name = os.path.splitext(lib)[0]
            if lib.endswith('.framework'):
                context_attrs['OS_LIBS'] += ['-framework ' + lib_name]
            else:
                context_attrs['OS_LIBS'] += [lib_name]

        # Add some features to all contexts. Put here in case LOCAL_INCLUDES
        # order matters.
        context_attrs['LOCAL_INCLUDES'] += [
            '!/ipc/ipdl/_ipdlheaders',
            '/ipc/chromium/src',
            '/ipc/glue',
        ]
        # These get set via VC project file settings for normal GYP builds.
        # TODO: Determine if these defines are needed for GN builds.
        if gn_config['mozbuild_args']['OS_TARGET'] == 'WINNT':
            context_attrs['DEFINES']['UNICODE'] = True
            context_attrs['DEFINES']['_UNICODE'] = True

        context_attrs['COMPILE_FLAGS'] = {
            'STL': [],
            'OS_INCLUDES': [],
        }

        for key, value in sandbox_vars.items():
            if context_attrs.get(key) and isinstance(context_attrs[key], list):
                # If we have a key from sandbox_vars that's also been
                # populated here we use the value from sandbox_vars as our
                # basis rather than overriding outright.
                context_attrs[key] = value + context_attrs[key]
            elif context_attrs.get(key) and isinstance(context_attrs[key], dict):
                context_attrs[key].update(value)
            else:
                context_attrs[key] = value

        target_relsrcdir = mozpath.join(project_relsrcdir, target_path, target_name)
        mozbuild_attrs['dirs'][target_relsrcdir] = context_attrs

    return mozbuild_attrs


def find_common_attrs(config_attributes):
    # Returns the intersection of the given configs and prunes the inputs
    # to no longer contain these common attributes.

    common_attrs = deepcopy(config_attributes[0])

    def make_intersection(reference, input_attrs):
        # Modifies `reference` so that after calling this function it only
        # contains parts it had in common with in `input_attrs`.

        for k, input_value in input_attrs.items():
            # Anything in `input_attrs` must match what's already in
            # `reference`.
            common_value = reference.get(k)
            if common_value:
                if isinstance(input_value, list):
                    input_value = set(input_value)
                    reference[k] = [i for i in common_value if i in input_value]
                elif isinstance(input_value, dict):
                    reference[k] = {key: value for key, value in common_value.items()
                                    if key in input_value and value == input_value[key]}
                elif input_value != common_value:
                    del reference[k]
            elif k in reference:
                del reference[k]

        # Additionally, any keys in `reference` that aren't in `input_attrs`
        # must be deleted.
        for k in set(reference.keys()) - set(input_attrs.keys()):
            del reference[k]

    def make_difference(reference, input_attrs):
        # Modifies `input_attrs` so that after calling this function it contains
        # no parts it has in common with in `reference`.
        for k, input_value in input_attrs.items():
            common_value = reference.get(k)
            if common_value:
                if isinstance(input_value, list):
                    common_value = set(common_value)
                    input_attrs[k] = [i for i in input_value if i not in common_value]
                elif isinstance(input_value, dict):
                    input_attrs[k] = {key: value for key, value in input_value.items()
                                      if key not in common_value}
                else:
                    del input_attrs[k]

    for config_attr_set in config_attributes[1:]:
        make_intersection(common_attrs, config_attr_set)

    for config_attr_set in config_attributes:
        make_difference(common_attrs, config_attr_set)

    return common_attrs


def write_mozbuild(config, srcdir, output, non_unified_sources, gn_config_files,
                   mozilla_flags):

    all_mozbuild_results = []

    for path in gn_config_files:
        with open(path, 'r') as fh:
            gn_config = json.load(fh)
            mozbuild_attrs = process_gn_config(gn_config, srcdir, config,
                                               output, non_unified_sources,
                                               gn_config['sandbox_vars'],
                                               mozilla_flags)
            all_mozbuild_results.append(mozbuild_attrs)

    # Translate {config -> {dirs -> build info}} into
    #           {dirs -> [(config, build_info)]}
    configs_by_dir = defaultdict(list)
    for config_attrs in all_mozbuild_results:
        mozbuild_args = config_attrs['mozbuild_args']
        dirs = config_attrs['dirs']
        for d, build_data in dirs.items():
            configs_by_dir[d].append((mozbuild_args, build_data))

    for relsrcdir, configs in configs_by_dir.items():
        target_srcdir = mozpath.join(config.topsrcdir, relsrcdir)
        mkdir(target_srcdir)

        target_mozbuild = mozpath.join(target_srcdir, 'moz.build')
        with open(target_mozbuild, 'w') as fh:
            mb = MozbuildWriter(fh)
            mb.write(license_header)
            mb.write('\n')
            mb.write(generated_header)

            all_attr_sets = [attrs for _, attrs in configs]
            all_args = [args for args, _ in configs]

            # Start with attributes that will be a part of the mozconfig
            # for every configuration, then factor by other potentially useful
            # combinations.
            for attrs in ((),
                          ('MOZ_DEBUG',), ('OS_TARGET',), ('MOZ_DEBUG', 'OS_TARGET',),
                          ('MOZ_DEBUG', 'OS_TARGET', 'CPU_ARCH', 'HOST_CPU_ARCH')):
                conditions = set()
                for args in all_args:
                    cond = tuple(((k, args.get(k)) for k in attrs))
                    conditions.add(cond)
                for cond in conditions:
                    common_attrs = find_common_attrs([attrs for args, attrs in configs if
                                                      all(args.get(k) == v for k, v in cond)])
                    if any(common_attrs.values()):
                        if cond:
                            mb.write_condition(dict(cond))
                        mb.write_attrs(common_attrs)
                        if cond:
                            mb.terminate_condition()

            mb.finalize()

    dirs_mozbuild = mozpath.join(srcdir, 'moz.build')
    with open(dirs_mozbuild, 'w') as fh:
        mb = MozbuildWriter(fh)
        mb.write(license_header)
        mb.write('\n')
        mb.write(generated_header)

        # Not every srcdir is present for every config, which needs to be
        # reflected in the generated root moz.build.
        dirs_by_config = {tuple(v['mozbuild_args'].items()): set(v['dirs'].keys())
                          for v in all_mozbuild_results}

        for attrs in ((), ('OS_TARGET',), ('OS_TARGET', 'CPU_ARCH')):

            conditions = set()
            for args in dirs_by_config.keys():
                cond = tuple(((k, dict(args).get(k)) for k in attrs))
                conditions.add(cond)

            for cond in conditions:
                common_dirs = None
                for args, dir_set in dirs_by_config.items():
                    if all(dict(args).get(k) == v for k, v in cond):
                        if common_dirs is None:
                            common_dirs = deepcopy(dir_set)
                        else:
                            common_dirs &= dir_set

                for args, dir_set in dirs_by_config.items():
                    if all(dict(args).get(k) == v for k, v in cond):
                        dir_set -= common_dirs

                if common_dirs:
                    if cond:
                        mb.write_condition(dict(cond))
                    mb.write_mozbuild_list('DIRS',
                                           ['/%s' % d for d in common_dirs])
                    if cond:
                        mb.terminate_condition()


def generate_gn_config(config, srcdir, output, non_unified_sources, gn_binary,
                       input_variables, sandbox_variables):

    def str_for_arg(v):
        if v in (True, False):
            return str(v).lower()
        return '"%s"' % v

    gn_args = '--args=%s' % ' '.join(['%s=%s' % (k, str_for_arg(v)) for k, v
                                      in input_variables.iteritems()])
    gn_arg_string = '_'.join([str(input_variables[k]) for k in sorted(input_variables.keys())])
    out_dir = mozpath.join(config.topobjdir, 'gn-output')
    gen_args = [
        config.substs['GN'], 'gen', out_dir, gn_args, '--ide=json',
    ]
    print("Running \"%s\"" % ' '.join(gen_args), file=sys.stderr)
    subprocess.check_call(gen_args, cwd=srcdir, stderr=subprocess.STDOUT)


    gn_config_file = mozpath.join(out_dir, 'project.json')

    with open(gn_config_file, 'r') as fh:
        gn_out = json.load(fh)
        gn_out = filter_gn_config(gn_out, config, sandbox_variables,
                                  input_variables)

    os.remove(gn_config_file)

    gn_out_file = mozpath.join(out_dir, gn_arg_string + '.json')
    with open(gn_out_file, 'w') as fh:
        json.dump(gn_out, fh, indent=4, sort_keys=True, separators=(',', ': '))
    print("Wrote gn config to %s" % gn_out_file)


class GnConfigGenBackend(BuildBackend):

    def consume_object(self, obj):
        if isinstance(obj, GnProjectData):
            gn_binary = obj.config.substs.get('GN')
            if not gn_binary:
                raise Exception("The GN program must be present to generate GN configs.")

            generate_gn_config(obj.config, mozpath.join(obj.srcdir, obj.target_dir),
                               mozpath.join(obj.objdir, obj.target_dir),
                               obj.non_unified_sources, gn_binary,
                               obj.gn_input_variables, obj.gn_sandbox_variables)
        return True

    def consume_finished(self):
        pass


class GnMozbuildWriterBackend(BuildBackend):

    def consume_object(self, obj):
        if isinstance(obj, GnProjectData):
            gn_config_files = glob.glob(mozpath.join(obj.srcdir, 'gn-configs', '*.json'))
            if not gn_config_files:
                # Check the objdir for a gn-config in to aide debugging in cases
                # someone is running both steps on the same machine and want to
                # sanity check moz.build generation for a particular config.
                gn_config_files = glob.glob(mozpath.join(obj.topobjdir,
                                                         'gn-output', '*.json'))
            if gn_config_files:
                print("Writing moz.build files based on the following gn configs: %s" %
                      gn_config_files)
                write_mozbuild(obj.config, mozpath.join(obj.srcdir, obj.target_dir),
                               mozpath.join(obj.objdir, obj.target_dir),
                               obj.non_unified_sources, gn_config_files,
                               obj.mozilla_flags)
            else:
                print("Ignoring gn project '%s', no config files found in '%s'" %
                      (obj.srcdir, mozpath.join(obj.srcdir, 'gn-configs')))
        return True

    def consume_finished(self):
        pass
