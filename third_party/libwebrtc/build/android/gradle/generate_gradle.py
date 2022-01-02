#!/usr/bin/env vpython
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates an Android Studio project from a GN target."""

import argparse
import codecs
import collections
import glob
import json
import logging
import os
import re
import shutil
import subprocess
import sys

_BUILD_ANDROID = os.path.join(os.path.dirname(__file__), os.pardir)
sys.path.append(_BUILD_ANDROID)
import devil_chromium
from devil.utils import run_tests_helper
from pylib import constants
from pylib.constants import host_paths

sys.path.append(os.path.join(_BUILD_ANDROID, 'gyp'))
import jinja_template
from util import build_utils
from util import resource_utils

sys.path.append(os.path.dirname(_BUILD_ANDROID))
import gn_helpers

_DEPOT_TOOLS_PATH = os.path.join(host_paths.DIR_SOURCE_ROOT, 'third_party',
                                 'depot_tools')
_DEFAULT_ANDROID_MANIFEST_PATH = os.path.join(
    host_paths.DIR_SOURCE_ROOT, 'build', 'android', 'gradle',
    'AndroidManifest.xml')
_FILE_DIR = os.path.dirname(__file__)
_GENERATED_JAVA_SUBDIR = 'generated_java'
_JNI_LIBS_SUBDIR = 'symlinked-libs'
_ARMEABI_SUBDIR = 'armeabi'
_GRADLE_BUILD_FILE = 'build.gradle'
_CMAKE_FILE = 'CMakeLists.txt'
# This needs to come first alphabetically among all modules.
_MODULE_ALL = '_all'
_SRC_INTERNAL = os.path.join(
    os.path.dirname(host_paths.DIR_SOURCE_ROOT), 'src-internal')
_INSTRUMENTATION_TARGET_SUFFIX = '_test_apk__test_apk__apk'

_DEFAULT_TARGETS = [
    '//android_webview/test/embedded_test_server:aw_net_test_support_apk',
    '//android_webview/test:webview_instrumentation_apk',
    '//android_webview/test:webview_instrumentation_test_apk',
    '//base:base_junit_tests',
    '//chrome/android:chrome_junit_tests',
    '//chrome/android:chrome_public_apk',
    '//chrome/android:chrome_public_test_apk',
    '//content/public/android:content_junit_tests',
    '//content/shell/android:content_shell_apk',
    # Below must be included even with --all since they are libraries.
    '//base/android/jni_generator:jni_processor',
    '//tools/android/errorprone_plugin:errorprone_plugin_java',
]

_EXCLUDED_PREBUILT_JARS = [
    # Android Studio already provides Desugar runtime.
    # Including it would cause linking error because of a duplicate class.
    'lib.java/third_party/bazel/desugar/Desugar-runtime.jar'
]


def _TemplatePath(name):
  return os.path.join(_FILE_DIR, '{}.jinja'.format(name))


def _RebasePath(path_or_list, new_cwd=None, old_cwd=None):
  """Makes the given path(s) relative to new_cwd, or absolute if not specified.

  If new_cwd is not specified, absolute paths are returned.
  If old_cwd is not specified, constants.GetOutDirectory() is assumed.
  """
  if path_or_list is None:
    return []
  if not isinstance(path_or_list, basestring):
    return [_RebasePath(p, new_cwd, old_cwd) for p in path_or_list]
  if old_cwd is None:
    old_cwd = constants.GetOutDirectory()
  old_cwd = os.path.abspath(old_cwd)
  if new_cwd:
    new_cwd = os.path.abspath(new_cwd)
    return os.path.relpath(os.path.join(old_cwd, path_or_list), new_cwd)
  return os.path.abspath(os.path.join(old_cwd, path_or_list))


def _IsSubpathOf(child, parent):
  """Returns whether |child| is a subpath of |parent|."""
  return not os.path.relpath(child, parent).startswith(os.pardir)


def _WriteFile(path, data):
  """Writes |data| to |path|, constucting parent directories if necessary."""
  logging.info('Writing %s', path)
  dirname = os.path.dirname(path)
  if not os.path.exists(dirname):
    os.makedirs(dirname)
  with codecs.open(path, 'w', 'utf-8') as output_file:
    output_file.write(data)


def _RunGnGen(output_dir, args=None):
  cmd = [os.path.join(_DEPOT_TOOLS_PATH, 'gn'), 'gen', output_dir]
  if args:
    cmd.extend(args)
  logging.info('Running: %r', cmd)
  subprocess.check_call(cmd)


def _RunNinja(output_dir, args):
  # Don't use version within _DEPOT_TOOLS_PATH, since most devs don't use
  # that one when building.
  cmd = ['autoninja', '-C', output_dir]
  cmd.extend(args)
  logging.info('Running: %r', cmd)
  subprocess.check_call(cmd)


def _QueryForAllGnTargets(output_dir):
  cmd = [
      os.path.join(_BUILD_ANDROID, 'list_java_targets.py'), '--gn-labels',
      '--nested', '--build-build-configs', '--output-directory', output_dir
  ]
  logging.info('Running: %r', cmd)
  return subprocess.check_output(cmd).splitlines()


class _ProjectEntry(object):
  """Helper class for project entries."""

  _cached_entries = {}

  def __init__(self, gn_target):
    # Use _ProjectEntry.FromGnTarget instead for caching.
    self._gn_target = gn_target
    self._build_config = None
    self._java_files = None
    self._all_entries = None
    self.android_test_entries = []

  @classmethod
  def FromGnTarget(cls, gn_target):
    assert gn_target.startswith('//'), gn_target
    if ':' not in gn_target:
      gn_target = '%s:%s' % (gn_target, os.path.basename(gn_target))
    if gn_target not in cls._cached_entries:
      cls._cached_entries[gn_target] = cls(gn_target)
    return cls._cached_entries[gn_target]

  @classmethod
  def FromBuildConfigPath(cls, path):
    prefix = 'gen/'
    suffix = '.build_config'
    assert path.startswith(prefix) and path.endswith(suffix), path
    subdir = path[len(prefix):-len(suffix)]
    gn_target = '//%s:%s' % (os.path.split(subdir))
    return cls.FromGnTarget(gn_target)

  def __hash__(self):
    return hash(self._gn_target)

  def __eq__(self, other):
    return self._gn_target == other.GnTarget()

  def GnTarget(self):
    return self._gn_target

  def NinjaTarget(self):
    return self._gn_target[2:]

  def GnBuildConfigTarget(self):
    return '%s__build_config_crbug_908819' % self._gn_target

  def GradleSubdir(self):
    """Returns the output subdirectory."""
    ninja_target = self.NinjaTarget()
    # Support targets at the root level. e.g. //:foo
    if ninja_target[0] == ':':
      ninja_target = ninja_target[1:]
    return ninja_target.replace(':', os.path.sep)

  def GeneratedJavaSubdir(self):
    return _RebasePath(
        os.path.join('gen', self.GradleSubdir(), _GENERATED_JAVA_SUBDIR))

  def ProjectName(self):
    """Returns the Gradle project name."""
    return self.GradleSubdir().replace(os.path.sep, '.')

  def BuildConfig(self):
    """Reads and returns the project's .build_config JSON."""
    if not self._build_config:
      path = os.path.join('gen', self.GradleSubdir() + '.build_config')
      with open(_RebasePath(path)) as jsonfile:
        self._build_config = json.load(jsonfile)
    return self._build_config

  def DepsInfo(self):
    return self.BuildConfig()['deps_info']

  def Gradle(self):
    return self.BuildConfig()['gradle']

  def Javac(self):
    return self.BuildConfig()['javac']

  def GetType(self):
    """Returns the target type from its .build_config."""
    return self.DepsInfo()['type']

  def IsValid(self):
    return self.GetType() in (
        'android_apk',
        'android_app_bundle_module',
        'java_library',
        "java_annotation_processor",
        'java_binary',
        'junit_binary',
    )

  def ResSources(self):
    return self.DepsInfo().get('lint_resource_sources', [])

  def JavaFiles(self):
    if self._java_files is None:
      java_sources_file = self.DepsInfo().get('java_sources_file')
      java_files = []
      if java_sources_file:
        java_sources_file = _RebasePath(java_sources_file)
        java_files = build_utils.ReadSourcesList(java_sources_file)
      self._java_files = java_files
    return self._java_files

  def PrebuiltJars(self):
    all_jars = self.Gradle().get('dependent_prebuilt_jars', [])
    return [i for i in all_jars if i not in _EXCLUDED_PREBUILT_JARS]

  def AllEntries(self):
    """Returns a list of all entries that the current entry depends on.

    This includes the entry itself to make iterating simpler."""
    if self._all_entries is None:
      logging.debug('Generating entries for %s', self.GnTarget())
      deps = [_ProjectEntry.FromBuildConfigPath(p)
          for p in self.Gradle()['dependent_android_projects']]
      deps.extend(_ProjectEntry.FromBuildConfigPath(p)
          for p in self.Gradle()['dependent_java_projects'])
      all_entries = set()
      for dep in deps:
        all_entries.update(dep.AllEntries())
      all_entries.add(self)
      self._all_entries = list(all_entries)
    return self._all_entries


class _ProjectContextGenerator(object):
  """Helper class to generate gradle build files"""
  def __init__(self, project_dir, build_vars, use_gradle_process_resources,
               jinja_processor, split_projects, channel):
    self.project_dir = project_dir
    self.build_vars = build_vars
    self.use_gradle_process_resources = use_gradle_process_resources
    self.jinja_processor = jinja_processor
    self.split_projects = split_projects
    self.channel = channel
    self.processed_java_dirs = set()
    self.processed_prebuilts = set()
    self.processed_res_dirs = set()

  def _GenJniLibs(self, root_entry):
    libraries = []
    for entry in self._GetEntries(root_entry):
      libraries += entry.BuildConfig().get('native', {}).get('libraries', [])
    if libraries:
      return _CreateJniLibsDir(constants.GetOutDirectory(),
          self.EntryOutputDir(root_entry), libraries)
    return []

  def _GenJavaDirs(self, root_entry):
    java_files = []
    for entry in self._GetEntries(root_entry):
      java_files += entry.JavaFiles()
    java_dirs, excludes = _ComputeJavaSourceDirsAndExcludes(
        constants.GetOutDirectory(), java_files)
    return java_dirs, excludes

  def _GenCustomManifest(self, entry):
    """Returns the path to the generated AndroidManifest.xml.

    Gradle uses package id from manifest when generating R.class. So, we need
    to generate a custom manifest if we let gradle process resources. We cannot
    simply set android.defaultConfig.applicationId because it is not supported
    for library targets."""
    resource_packages = entry.Javac().get('resource_packages')
    if not resource_packages:
      logging.debug('Target ' + entry.GnTarget() + ' includes resources from '
          'unknown package. Unable to process with gradle.')
      return _DEFAULT_ANDROID_MANIFEST_PATH
    elif len(resource_packages) > 1:
      logging.debug('Target ' + entry.GnTarget() + ' includes resources from '
          'multiple packages. Unable to process with gradle.')
      return _DEFAULT_ANDROID_MANIFEST_PATH

    variables = {'package': resource_packages[0]}
    data = self.jinja_processor.Render(_TemplatePath('manifest'), variables)
    output_file = os.path.join(
        self.EntryOutputDir(entry), 'AndroidManifest.xml')
    _WriteFile(output_file, data)

    return output_file

  def _Relativize(self, entry, paths):
    return _RebasePath(paths, self.EntryOutputDir(entry))

  def _GetEntries(self, entry):
    if self.split_projects:
      return [entry]
    return entry.AllEntries()

  def EntryOutputDir(self, entry):
    return os.path.join(self.project_dir, entry.GradleSubdir())

  def GeneratedInputs(self, root_entry):
    generated_inputs = set()
    for entry in self._GetEntries(root_entry):
      generated_inputs.update(entry.PrebuiltJars())
    return generated_inputs

  def GenerateManifest(self, root_entry):
    android_manifest = root_entry.DepsInfo().get('android_manifest')
    if not android_manifest:
      android_manifest = self._GenCustomManifest(root_entry)
    return self._Relativize(root_entry, android_manifest)

  def Generate(self, root_entry):
    # TODO(agrieve): Add an option to use interface jars and see if that speeds
    # things up at all.
    variables = {}
    java_dirs, excludes = self._GenJavaDirs(root_entry)
    java_dirs.extend(
        e.GeneratedJavaSubdir() for e in self._GetEntries(root_entry))
    self.processed_java_dirs.update(java_dirs)
    java_dirs.sort()
    variables['java_dirs'] = self._Relativize(root_entry, java_dirs)
    variables['java_excludes'] = excludes
    variables['jni_libs'] = self._Relativize(
        root_entry, set(self._GenJniLibs(root_entry)))
    prebuilts = set(
        p for e in self._GetEntries(root_entry) for p in e.PrebuiltJars())
    self.processed_prebuilts.update(prebuilts)
    variables['prebuilts'] = self._Relativize(root_entry, prebuilts)
    res_sources_files = _RebasePath(
        set(p for e in self._GetEntries(root_entry) for p in e.ResSources()))
    res_sources = []
    for res_sources_file in res_sources_files:
      res_sources.extend(build_utils.ReadSourcesList(res_sources_file))
    res_dirs = resource_utils.DeduceResourceDirsFromFileList(res_sources)
    # Do not add generated resources for the all module since it creates many
    # duplicates, and currently resources are only used for editing.
    self.processed_res_dirs.update(res_dirs)
    variables['res_dirs'] = self._Relativize(root_entry, res_dirs)
    if self.split_projects:
      deps = [_ProjectEntry.FromBuildConfigPath(p)
              for p in root_entry.Gradle()['dependent_android_projects']]
      variables['android_project_deps'] = [d.ProjectName() for d in deps]
      deps = [_ProjectEntry.FromBuildConfigPath(p)
              for p in root_entry.Gradle()['dependent_java_projects']]
      variables['java_project_deps'] = [d.ProjectName() for d in deps]
    return variables


def _ComputeJavaSourceDirs(java_files):
  """Returns a dictionary of source dirs with each given files in one."""
  found_roots = {}
  for path in java_files:
    path_root = path
    # Recognize these tokens as top-level.
    while True:
      path_root = os.path.dirname(path_root)
      basename = os.path.basename(path_root)
      assert basename, 'Failed to find source dir for ' + path
      if basename in ('java', 'src'):
        break
      if basename in ('javax', 'org', 'com'):
        path_root = os.path.dirname(path_root)
        break
    if path_root not in found_roots:
      found_roots[path_root] = []
    found_roots[path_root].append(path)
  return found_roots


def _ComputeExcludeFilters(wanted_files, unwanted_files, parent_dir):
  """Returns exclude patters to exclude unwanted files but keep wanted files.

  - Shortens exclude list by globbing if possible.
  - Exclude patterns are relative paths from the parent directory.
  """
  excludes = []
  files_to_include = set(wanted_files)
  files_to_exclude = set(unwanted_files)
  while files_to_exclude:
    unwanted_file = files_to_exclude.pop()
    target_exclude = os.path.join(
        os.path.dirname(unwanted_file), '*.java')
    found_files = set(glob.glob(target_exclude))
    valid_files = found_files & files_to_include
    if valid_files:
      excludes.append(os.path.relpath(unwanted_file, parent_dir))
    else:
      excludes.append(os.path.relpath(target_exclude, parent_dir))
      files_to_exclude -= found_files
  return excludes


def _ComputeJavaSourceDirsAndExcludes(output_dir, java_files):
  """Computes the list of java source directories and exclude patterns.

  1. Computes the root java source directories from the list of files.
  2. Compute exclude patterns that exclude all extra files only.
  3. Returns the list of java source directories and exclude patterns.
  """
  java_dirs = []
  excludes = []
  if java_files:
    java_files = _RebasePath(java_files)
    computed_dirs = _ComputeJavaSourceDirs(java_files)
    java_dirs = computed_dirs.keys()
    all_found_java_files = set()

    for directory, files in computed_dirs.iteritems():
      found_java_files = build_utils.FindInDirectory(directory, '*.java')
      all_found_java_files.update(found_java_files)
      unwanted_java_files = set(found_java_files) - set(files)
      if unwanted_java_files:
        logging.debug('Directory requires excludes: %s', directory)
        excludes.extend(
            _ComputeExcludeFilters(files, unwanted_java_files, directory))

    missing_java_files = set(java_files) - all_found_java_files
    # Warn only about non-generated files that are missing.
    missing_java_files = [p for p in missing_java_files
                          if not p.startswith(output_dir)]
    if missing_java_files:
      logging.warning(
          'Some java files were not found: %s', missing_java_files)

  return java_dirs, excludes


def _CreateRelativeSymlink(target_path, link_path):
  link_dir = os.path.dirname(link_path)
  relpath = os.path.relpath(target_path, link_dir)
  logging.debug('Creating symlink %s -> %s', link_path, relpath)
  os.symlink(relpath, link_path)


def _CreateJniLibsDir(output_dir, entry_output_dir, so_files):
  """Creates directory with symlinked .so files if necessary.

  Returns list of JNI libs directories."""

  if so_files:
    symlink_dir = os.path.join(entry_output_dir, _JNI_LIBS_SUBDIR)
    shutil.rmtree(symlink_dir, True)
    abi_dir = os.path.join(symlink_dir, _ARMEABI_SUBDIR)
    if not os.path.exists(abi_dir):
      os.makedirs(abi_dir)
    for so_file in so_files:
      target_path = os.path.join(output_dir, so_file)
      symlinked_path = os.path.join(abi_dir, so_file)
      _CreateRelativeSymlink(target_path, symlinked_path)

    return [symlink_dir]

  return []


def _GenerateLocalProperties(sdk_dir):
  """Returns the data for local.properties as a string."""
  return '\n'.join([
      '# Generated by //build/android/gradle/generate_gradle.py',
      'sdk.dir=%s' % sdk_dir,
      '',
  ])


def _GenerateGradleWrapperPropertiesCanary():
  """Returns the data for gradle-wrapper.properties as a string."""
  # Before May 2020, this wasn't necessary. Might not be necessary at some point
  # in the future?
  return '\n'.join([
      '# Generated by //build/android/gradle/generate_gradle.py',
      ('distributionUrl=https\\://services.gradle.org/distributions/'
       'gradle-6.5-rc-1-all.zip\n'),
      '',
  ])


def _GenerateGradleProperties():
  """Returns the data for gradle.properties as a string."""
  return '\n'.join([
      '# Generated by //build/android/gradle/generate_gradle.py',
      '',
      '# Tells Gradle to show warnings during project sync.',
      'org.gradle.warning.mode=all',
      '',
  ])


def _GenerateBaseVars(generator, build_vars):
  variables = {}
  variables['compile_sdk_version'] = (
      'android-%s' % build_vars['compile_sdk_version'])
  target_sdk_version = build_vars['android_sdk_version']
  if target_sdk_version.isalpha():
    target_sdk_version = '"{}"'.format(target_sdk_version)
  variables['target_sdk_version'] = target_sdk_version
  variables['use_gradle_process_resources'] = (
      generator.use_gradle_process_resources)
  variables['channel'] = generator.channel
  return variables


def _GenerateGradleFile(entry, generator, build_vars, jinja_processor):
  """Returns the data for a project's build.gradle."""
  deps_info = entry.DepsInfo()
  variables = _GenerateBaseVars(generator, build_vars)
  sourceSetName = 'main'

  if deps_info['type'] == 'android_apk':
    target_type = 'android_apk'
  elif deps_info['type'] in ('java_library', 'java_annotation_processor'):
    is_prebuilt = deps_info.get('is_prebuilt', False)
    gradle_treat_as_prebuilt = deps_info.get('gradle_treat_as_prebuilt', False)
    if is_prebuilt or gradle_treat_as_prebuilt:
      return None
    elif deps_info['requires_android']:
      target_type = 'android_library'
    else:
      target_type = 'java_library'
  elif deps_info['type'] == 'java_binary':
    target_type = 'java_binary'
    variables['main_class'] = deps_info.get('main_class')
  elif deps_info['type'] == 'junit_binary':
    target_type = 'android_junit'
    sourceSetName = 'test'
  else:
    return None

  variables['target_name'] = os.path.splitext(deps_info['name'])[0]
  variables['template_type'] = target_type
  variables['main'] = {}
  variables[sourceSetName] = generator.Generate(entry)
  variables['main']['android_manifest'] = generator.GenerateManifest(entry)

  if entry.android_test_entries:
    variables['android_test'] = []
    for e in entry.android_test_entries:
      test_entry = generator.Generate(e)
      test_entry['android_manifest'] = generator.GenerateManifest(e)
      variables['android_test'].append(test_entry)
      for key, value in test_entry.iteritems():
        if isinstance(value, list):
          test_entry[key] = sorted(set(value) - set(variables['main'][key]))

  return jinja_processor.Render(
      _TemplatePath(target_type.split('_')[0]), variables)


def _IsTestDir(path):
  return ('javatests/' in path or
          'junit/' in path or
          'test/' in path or
          'testing/' in path)


# Example: //chrome/android:monochrome
def _GetNative(relative_func, target_names):
  """Returns an object containing native c++ sources list and its included path

  Iterate through all target_names and their deps to get the list of included
  paths and sources."""
  out_dir = constants.GetOutDirectory()
  with open(os.path.join(out_dir, 'project.json'), 'r') as project_file:
    projects = json.load(project_file)
  project_targets = projects['targets']
  root_dir = projects['build_settings']['root_path']
  includes = set()
  processed_target = set()
  targets_stack = list(target_names)
  sources = []

  while targets_stack:
    target_name = targets_stack.pop()
    if target_name in processed_target:
      continue
    processed_target.add(target_name)
    target = project_targets[target_name]
    includes.update(target.get('include_dirs', []))
    targets_stack.extend(target.get('deps', []))
    # Ignore generated files
    sources.extend(f for f in target.get('sources', [])
                   if f.endswith('.cc') and not f.startswith('//out'))

  def process_paths(paths):
    # Ignores leading //
    return relative_func(
        sorted(os.path.join(root_dir, path[2:]) for path in paths))

  return {
      'sources': process_paths(sources),
      'includes': process_paths(includes),
  }


def _GenerateModuleAll(gradle_output_dir, generator, build_vars,
                       jinja_processor, native_targets):
  """Returns the data for a pseudo build.gradle of all dirs.

  See //docs/android_studio.md for more details."""
  variables = _GenerateBaseVars(generator, build_vars)
  target_type = 'android_apk'
  variables['target_name'] = _MODULE_ALL
  variables['template_type'] = target_type
  java_dirs = sorted(generator.processed_java_dirs)
  prebuilts = sorted(generator.processed_prebuilts)
  res_dirs = sorted(generator.processed_res_dirs)
  def Relativize(paths):
    return _RebasePath(paths, os.path.join(gradle_output_dir, _MODULE_ALL))
  main_java_dirs = [d for d in java_dirs if not _IsTestDir(d)]
  test_java_dirs = [d for d in java_dirs if _IsTestDir(d)]
  variables['main'] = {
      'android_manifest': Relativize(_DEFAULT_ANDROID_MANIFEST_PATH),
      'java_dirs': Relativize(main_java_dirs),
      'prebuilts': Relativize(prebuilts),
      'java_excludes': ['**/*.java'],
      'res_dirs': Relativize(res_dirs),
  }
  variables['android_test'] = [{
      'java_dirs': Relativize(test_java_dirs),
      'java_excludes': ['**/*.java'],
  }]
  if native_targets:
    variables['native'] = _GetNative(
        relative_func=Relativize, target_names=native_targets)
  data = jinja_processor.Render(
      _TemplatePath(target_type.split('_')[0]), variables)
  _WriteFile(
      os.path.join(gradle_output_dir, _MODULE_ALL, _GRADLE_BUILD_FILE), data)
  if native_targets:
    cmake_data = jinja_processor.Render(_TemplatePath('cmake'), variables)
    _WriteFile(
        os.path.join(gradle_output_dir, _MODULE_ALL, _CMAKE_FILE), cmake_data)


def _GenerateRootGradle(jinja_processor, channel):
  """Returns the data for the root project's build.gradle."""
  return jinja_processor.Render(_TemplatePath('root'), {'channel': channel})


def _GenerateSettingsGradle(project_entries):
  """Returns the data for settings.gradle."""
  project_name = os.path.basename(os.path.dirname(host_paths.DIR_SOURCE_ROOT))
  lines = []
  lines.append('// Generated by //build/android/gradle/generate_gradle.py')
  lines.append('rootProject.name = "%s"' % project_name)
  lines.append('rootProject.projectDir = settingsDir')
  lines.append('')
  for name, subdir in project_entries:
    # Example target:
    # android_webview:android_webview_java__build_config_crbug_908819
    lines.append('include ":%s"' % name)
    lines.append('project(":%s").projectDir = new File(settingsDir, "%s")' %
                 (name, subdir))
  return '\n'.join(lines)


def _FindAllProjectEntries(main_entries):
  """Returns the list of all _ProjectEntry instances given the root project."""
  found = set()
  to_scan = list(main_entries)
  while to_scan:
    cur_entry = to_scan.pop()
    if cur_entry in found:
      continue
    found.add(cur_entry)
    sub_config_paths = cur_entry.DepsInfo()['deps_configs']
    to_scan.extend(
        _ProjectEntry.FromBuildConfigPath(p) for p in sub_config_paths)
  return list(found)


def _CombineTestEntries(entries):
  """Combines test apks into the androidTest source set of their target.

  - Speeds up android studio
  - Adds proper dependency between test and apk_under_test
  - Doesn't work for junit yet due to resulting circular dependencies
    - e.g. base_junit_tests > base_junit_test_support > base_java
  """
  combined_entries = []
  android_test_entries = collections.defaultdict(list)
  for entry in entries:
    target_name = entry.GnTarget()
    if (target_name.endswith(_INSTRUMENTATION_TARGET_SUFFIX)
        and 'apk_under_test' in entry.Gradle()):
      apk_name = entry.Gradle()['apk_under_test']
      android_test_entries[apk_name].append(entry)
    else:
      combined_entries.append(entry)
  for entry in combined_entries:
    target_name = entry.DepsInfo()['name']
    if target_name in android_test_entries:
      entry.android_test_entries = android_test_entries[target_name]
      del android_test_entries[target_name]
  # Add unmatched test entries as individual targets.
  combined_entries.extend(e for l in android_test_entries.values() for e in l)
  return combined_entries


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output-directory',
                      help='Path to the root build directory.')
  parser.add_argument('-v',
                      '--verbose',
                      dest='verbose_count',
                      default=0,
                      action='count',
                      help='Verbose level')
  parser.add_argument('--target',
                      dest='targets',
                      action='append',
                      help='GN target to generate project for. Replaces set of '
                           'default targets. May be repeated.')
  parser.add_argument('--extra-target',
                      dest='extra_targets',
                      action='append',
                      help='GN target to generate project for, in addition to '
                           'the default ones. May be repeated.')
  parser.add_argument('--project-dir',
                      help='Root of the output project.',
                      default=os.path.join('$CHROMIUM_OUTPUT_DIR', 'gradle'))
  parser.add_argument('--all',
                      action='store_true',
                      help='Include all .java files reachable from any '
                           'apk/test/binary target. On by default unless '
                           '--split-projects is used (--split-projects can '
                           'slow down Studio given too many targets).')
  parser.add_argument('--use-gradle-process-resources',
                      action='store_true',
                      help='Have gradle generate R.java rather than ninja')
  parser.add_argument('--split-projects',
                      action='store_true',
                      help='Split projects by their gn deps rather than '
                           'combining all the dependencies of each target')
  parser.add_argument('--native-target',
                      dest='native_targets',
                      action='append',
                      help='GN native targets to generate for. May be '
                           'repeated.')
  parser.add_argument('--compile-sdk-version',
                      type=int,
                      default=0,
                      help='Override compileSdkVersion for android sdk docs. '
                           'Useful when sources for android_sdk_version is '
                           'not available in Android Studio.')
  parser.add_argument(
      '--sdk-path',
      default=os.path.expanduser('~/Android/Sdk'),
      help='The path to use as the SDK root, overrides the '
      'default at ~/Android/Sdk.')
  version_group = parser.add_mutually_exclusive_group()
  version_group.add_argument('--beta',
                      action='store_true',
                      help='Generate a project that is compatible with '
                           'Android Studio Beta.')
  version_group.add_argument('--canary',
                      action='store_true',
                      help='Generate a project that is compatible with '
                           'Android Studio Canary.')
  args = parser.parse_args()
  if args.output_directory:
    constants.SetOutputDirectory(args.output_directory)
  constants.CheckOutputDirectory()
  output_dir = constants.GetOutDirectory()
  devil_chromium.Initialize(output_directory=output_dir)
  run_tests_helper.SetLogLevel(args.verbose_count)

  if args.use_gradle_process_resources:
    assert args.split_projects, (
        'Gradle resources does not work without --split-projects.')

  _gradle_output_dir = os.path.abspath(
      args.project_dir.replace('$CHROMIUM_OUTPUT_DIR', output_dir))
  logging.warning('Creating project at: %s', _gradle_output_dir)

  # Generate for "all targets" by default when not using --split-projects (too
  # slow), and when no --target has been explicitly set. "all targets" means all
  # java targets that are depended on by an apk or java_binary (leaf
  # java_library targets will not be included).
  args.all = args.all or (not args.split_projects and not args.targets)

  targets_from_args = set(args.targets or _DEFAULT_TARGETS)
  if args.extra_targets:
    targets_from_args.update(args.extra_targets)

  if args.all:
    if args.native_targets:
      _RunGnGen(output_dir, ['--ide=json'])
    elif not os.path.exists(os.path.join(output_dir, 'build.ninja')):
      _RunGnGen(output_dir)
    else:
      # Faster than running "gn gen" in the no-op case.
      _RunNinja(output_dir, ['build.ninja'])
    # Query ninja for all __build_config_crbug_908819 targets.
    targets = _QueryForAllGnTargets(output_dir)
  else:
    assert not args.native_targets, 'Native editing requires --all.'
    targets = [
        re.sub(r'_test_apk$', _INSTRUMENTATION_TARGET_SUFFIX, t)
        for t in targets_from_args
    ]
    # Necessary after "gn clean"
    if not os.path.exists(
        os.path.join(output_dir, gn_helpers.BUILD_VARS_FILENAME)):
      _RunGnGen(output_dir)

  build_vars = gn_helpers.ReadBuildVars(output_dir)
  jinja_processor = jinja_template.JinjaProcessor(_FILE_DIR)
  if args.beta:
    channel = 'beta'
  elif args.canary:
    channel = 'canary'
  else:
    channel = 'stable'
  if args.compile_sdk_version:
    build_vars['compile_sdk_version'] = args.compile_sdk_version
  else:
    build_vars['compile_sdk_version'] = build_vars['android_sdk_version']
  generator = _ProjectContextGenerator(_gradle_output_dir, build_vars,
      args.use_gradle_process_resources, jinja_processor, args.split_projects,
      channel)

  main_entries = [_ProjectEntry.FromGnTarget(t) for t in targets]

  if args.all:
    # There are many unused libraries, so restrict to those that are actually
    # used by apks/bundles/binaries/tests or that are explicitly mentioned in
    # --targets.
    BASE_TYPES = ('android_apk', 'android_app_bundle_module', 'java_binary',
                  'junit_binary')
    main_entries = [
        e for e in main_entries
        if (e.GetType() in BASE_TYPES or e.GnTarget() in targets_from_args
            or e.GnTarget().endswith(_INSTRUMENTATION_TARGET_SUFFIX))
    ]

  if args.split_projects:
    main_entries = _FindAllProjectEntries(main_entries)

  logging.info('Generating for %d targets.', len(main_entries))

  entries = [e for e in _CombineTestEntries(main_entries) if e.IsValid()]
  logging.info('Creating %d projects for targets.', len(entries))

  logging.warning('Writing .gradle files...')
  project_entries = []
  # When only one entry will be generated we want it to have a valid
  # build.gradle file with its own AndroidManifest.
  for entry in entries:
    data = _GenerateGradleFile(entry, generator, build_vars, jinja_processor)
    if data and not args.all:
      project_entries.append((entry.ProjectName(), entry.GradleSubdir()))
      _WriteFile(
          os.path.join(generator.EntryOutputDir(entry), _GRADLE_BUILD_FILE),
          data)
  if args.all:
    project_entries.append((_MODULE_ALL, _MODULE_ALL))
    _GenerateModuleAll(_gradle_output_dir, generator, build_vars,
                       jinja_processor, args.native_targets)

  _WriteFile(os.path.join(generator.project_dir, _GRADLE_BUILD_FILE),
             _GenerateRootGradle(jinja_processor, channel))

  _WriteFile(os.path.join(generator.project_dir, 'settings.gradle'),
             _GenerateSettingsGradle(project_entries))

  # Ensure the Android Studio sdk is correctly initialized.
  if not os.path.exists(args.sdk_path):
    # Help first-time users avoid Android Studio forcibly changing back to
    # the previous default due to not finding a valid sdk under this dir.
    shutil.copytree(_RebasePath(build_vars['android_sdk_root']), args.sdk_path)
  _WriteFile(
      os.path.join(generator.project_dir, 'local.properties'),
      _GenerateLocalProperties(args.sdk_path))
  _WriteFile(os.path.join(generator.project_dir, 'gradle.properties'),
             _GenerateGradleProperties())

  wrapper_properties = os.path.join(generator.project_dir, 'gradle', 'wrapper',
                                    'gradle-wrapper.properties')
  if os.path.exists(wrapper_properties):
    os.unlink(wrapper_properties)
  if args.canary:
    _WriteFile(wrapper_properties, _GenerateGradleWrapperPropertiesCanary())

  generated_inputs = set()
  for entry in entries:
    entries_to_gen = [entry]
    entries_to_gen.extend(entry.android_test_entries)
    for entry_to_gen in entries_to_gen:
      # Build all paths references by .gradle that exist within output_dir.
      generated_inputs.update(generator.GeneratedInputs(entry_to_gen))
  if generated_inputs:
    targets = _RebasePath(generated_inputs, output_dir)
    _RunNinja(output_dir, targets)

  logging.warning('Generated files will only appear once you\'ve built them.')
  logging.warning('Generated projects for Android Studio %s', channel)
  logging.warning('For more tips: https://chromium.googlesource.com/chromium'
                  '/src.git/+/master/docs/android_studio.md')


if __name__ == '__main__':
  main()
