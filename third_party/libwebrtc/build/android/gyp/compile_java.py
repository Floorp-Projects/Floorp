#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import distutils.spawn
import functools
import logging
import multiprocessing
import optparse
import os
import re
import shutil
import sys
import time
import zipfile

from util import build_utils
from util import md5_check
from util import jar_info_utils

sys.path.insert(
    0,
    os.path.join(build_utils.DIR_SOURCE_ROOT, 'third_party', 'colorama', 'src'))
import colorama

_JAVAC_EXTRACTOR = os.path.join(build_utils.DIR_SOURCE_ROOT, 'third_party',
                                'android_prebuilts', 'build_tools', 'common',
                                'framework', 'javac_extractor.jar')

# Full list of checks: https://errorprone.info/bugpatterns
ERRORPRONE_WARNINGS_TO_DISABLE = [
    # These should really be turned on.
    'ParameterNotNullable',
    'CollectionUndefinedEquality',
    'ModifyCollectionInEnhancedForLoop',
    # The following are super useful, but existing issues need to be fixed first
    # before they can start failing the build on new errors.
    'InvalidParam',
    'InvalidLink',
    'InvalidInlineTag',
    'EmptyBlockTag',
    'PublicConstructorForAbstractClass',
    'InvalidBlockTag',
    'StaticAssignmentInConstructor',
    'MutablePublicArray',
    'UnescapedEntity',
    'NonCanonicalType',
    'ProtectedMembersInFinalClass',
    'AlmostJavadoc',
    # TODO(crbug.com/834807): Follow steps in bug
    'DoubleBraceInitialization',
    # TODO(crbug.com/834790): Follow steps in bug.
    'CatchAndPrintStackTrace',
    # TODO(crbug.com/801210): Follow steps in bug.
    'SynchronizeOnNonFinalField',
    # TODO(crbug.com/802073): Follow steps in bug.
    'TypeParameterUnusedInFormals',
    # TODO(crbug.com/803484): Follow steps in bug.
    'CatchFail',
    # TODO(crbug.com/803485): Follow steps in bug.
    'JUnitAmbiguousTestClass',
    # TODO(crbug.com/1027683): Follow steps in bug.
    'UnnecessaryParentheses',
    # TODO(wnwen): Fix issue in JavaUploadDataSinkBase.java
    'PrimitiveAtomicReference',
    # Android platform default is always UTF-8.
    # https://developer.android.com/reference/java/nio/charset/Charset.html#defaultCharset()
    'DefaultCharset',
    # Low priority since the alternatives still work.
    'JdkObsolete',
    # We don't use that many lambdas.
    'FunctionalInterfaceClash',
    # There are lots of times when we just want to post a task.
    'FutureReturnValueIgnored',
    # Nice to be explicit about operators, but not necessary.
    'OperatorPrecedence',
    # Just false positives in our code.
    'ThreadJoinLoop',
    # Low priority corner cases with String.split.
    # Linking Guava and using Splitter was rejected
    # in the https://chromium-review.googlesource.com/c/chromium/src/+/871630.
    'StringSplitter',
    # Preferred to use another method since it propagates exceptions better.
    'ClassNewInstance',
    # Nice to have static inner classes but not necessary.
    'ClassCanBeStatic',
    # Explicit is better than implicit.
    'FloatCast',
    # Results in false positives.
    'ThreadLocalUsage',
    # Also just false positives.
    'Finally',
    # False positives for Chromium.
    'FragmentNotInstantiable',
    # Low priority to fix.
    'HidingField',
    # Low priority.
    'IntLongMath',
    # Low priority.
    'BadComparable',
    # Low priority.
    'EqualsHashCode',
    # Nice to fix but low priority.
    'TypeParameterShadowing',
    # Good to have immutable enums, also low priority.
    'ImmutableEnumChecker',
    # False positives for testing.
    'InputStreamSlowMultibyteRead',
    # Nice to have better primitives.
    'BoxedPrimitiveConstructor',
    # Not necessary for tests.
    'OverrideThrowableToString',
    # Nice to have better type safety.
    'CollectionToArraySafeParameter',
    # Makes logcat debugging more difficult, and does not provide obvious
    # benefits in the Chromium codebase.
    'ObjectToString',
    # Triggers on private methods that are @CalledByNative.
    'UnusedMethod',
    # Triggers on generated R.java files.
    'UnusedVariable',
    # Not that useful.
    'UnsafeReflectiveConstructionCast',
    # Not that useful.
    'MixedMutabilityReturnType',
    # Nice to have.
    'EqualsGetClass',
    # A lot of false-positives from CharSequence.equals().
    'UndefinedEquals',
    # Nice to have.
    'ExtendingJUnitAssert',
    # Nice to have.
    'SystemExitOutsideMain',
    # Nice to have.
    'TypeParameterNaming',
    # Nice to have.
    'UnusedException',
    # Nice to have.
    'UngroupedOverloads',
    # Nice to have.
    'FunctionalInterfaceClash',
    # Nice to have.
    'InconsistentOverloads',
    # Dagger generated code triggers this.
    'SameNameButDifferent',
    # Nice to have.
    'UnnecessaryLambda',
    # Nice to have.
    'UnnecessaryAnonymousClass',
    # Nice to have.
    'LiteProtoToString',
    # Nice to have.
    'MissingSummary',
    # Nice to have.
    'ReturnFromVoid',
    # Nice to have.
    'EmptyCatch',
    # Nice to have.
    'BadImport',
    # Nice to have.
    'UseCorrectAssertInTests',
    # Nice to have.
    'InlineFormatString',
    # Nice to have.
    'DefaultPackage',
    # Must be off since we are now passing in annotation processor generated
    # code as a source jar (deduplicating work with turbine).
    'RefersToDaggerCodegen',
    # We already have presubmit checks for this. Not necessary to warn on
    # every build.
    'RemoveUnusedImports',
]

# Full list of checks: https://errorprone.info/bugpatterns
# Only those marked as "experimental" need to be listed here in order to be
# enabled.
ERRORPRONE_WARNINGS_TO_ENABLE = [
    'BinderIdentityRestoredDangerously',
    'EmptyIf',
    'EqualsBrokenForNull',
    'InvalidThrows',
    'LongLiteralLowerCaseSuffix',
    'MultiVariableDeclaration',
    'RedundantOverride',
    'StaticQualifiedUsingExpression',
    'StringEquality',
    'TimeUnitMismatch',
    'UnnecessaryStaticImport',
    'UseBinds',
    'WildcardImport',
]


def ProcessJavacOutput(output):
  fileline_prefix = r'(?P<fileline>(?P<file>[-.\w/\\]+.java):(?P<line>[0-9]+):)'
  warning_re = re.compile(fileline_prefix +
                          r'(?P<full_message> warning: (?P<message>.*))$')
  error_re = re.compile(fileline_prefix +
                        r'(?P<full_message> (?P<message>.*))$')
  marker_re = re.compile(r'\s*(?P<marker>\^)\s*$')

  # These warnings cannot be suppressed even for third party code. Deprecation
  # warnings especially do not help since we must support older android version.
  deprecated_re = re.compile(
      r'(Note: .* uses? or overrides? a deprecated API.)$')
  unchecked_re = re.compile(
      r'(Note: .* uses? unchecked or unsafe operations.)$')
  recompile_re = re.compile(r'(Note: Recompile with -Xlint:.* for details.)$')

  warning_color = ['full_message', colorama.Fore.YELLOW + colorama.Style.DIM]
  error_color = ['full_message', colorama.Fore.MAGENTA + colorama.Style.BRIGHT]
  marker_color = ['marker', colorama.Fore.BLUE + colorama.Style.BRIGHT]

  def Colorize(line, regex, color):
    match = regex.match(line)
    start = match.start(color[0])
    end = match.end(color[0])
    return (line[:start] + color[1] + line[start:end] + colorama.Fore.RESET +
            colorama.Style.RESET_ALL + line[end:])

  def ApplyFilters(line):
    return not (deprecated_re.match(line) or unchecked_re.match(line)
                or recompile_re.match(line))

  def ApplyColors(line):
    if warning_re.match(line):
      line = Colorize(line, warning_re, warning_color)
    elif error_re.match(line):
      line = Colorize(line, error_re, error_color)
    elif marker_re.match(line):
      line = Colorize(line, marker_re, marker_color)
    return line

  return '\n'.join(map(ApplyColors, filter(ApplyFilters, output.split('\n'))))


def CheckErrorproneStderrWarning(jar_path, expected_warning_regex,
                                 javac_output):
  if not re.search(expected_warning_regex, javac_output):
    raise Exception('Expected `{}` warning when compiling `{}`'.format(
        expected_warning_regex, os.path.basename(jar_path)))

  # Do not print warning
  return ''


def _ParsePackageAndClassNames(java_file):
  package_name = ''
  class_names = []
  with open(java_file) as f:
    for l in f:
      # Strip unindented comments.
      # Considers a leading * as a continuation of a multi-line comment (our
      # linter doesn't enforce a space before it like there should be).
      l = re.sub(r'^(?://.*|/?\*.*?(?:\*/\s*|$))', '', l)

      m = re.match(r'package\s+(.*?);', l)
      if m and not package_name:
        package_name = m.group(1)

      # Not exactly a proper parser, but works for sources that Chrome uses.
      # In order to not match nested classes, it just checks for lack of indent.
      m = re.match(r'(?:\S.*?)?(?:class|@?interface|enum)\s+(.+?)\b', l)
      if m:
        class_names.append(m.group(1))
  return package_name, class_names


def _ProcessJavaFileForInfo(java_file):
  package_name, class_names = _ParsePackageAndClassNames(java_file)
  return java_file, package_name, class_names


class _InfoFileContext(object):
  """Manages the creation of the class->source file .info file."""

  def __init__(self, chromium_code, excluded_globs):
    self._chromium_code = chromium_code
    self._excluded_globs = excluded_globs
    # Map of .java path -> .srcjar/nested/path.java.
    self._srcjar_files = {}
    # List of generators from pool.imap_unordered().
    self._results = []
    # Lazily created multiprocessing.Pool.
    self._pool = None

  def AddSrcJarSources(self, srcjar_path, extracted_paths, parent_dir):
    for path in extracted_paths:
      # We want the path inside the srcjar so the viewer can have a tree
      # structure.
      self._srcjar_files[path] = '{}/{}'.format(
          srcjar_path, os.path.relpath(path, parent_dir))

  def SubmitFiles(self, java_files):
    if self._pool is None:
      # Restrict to just one process to not slow down compiling. Compiling
      # is always slower.
      self._pool = multiprocessing.Pool(1)
    logging.info('Submitting %d files for info', len(java_files))
    self._results.append(
        self._pool.imap_unordered(
            _ProcessJavaFileForInfo, java_files, chunksize=1000))

  def _CheckPathMatchesClassName(self, java_file, package_name, class_name):
    parts = package_name.split('.') + [class_name + '.java']
    expected_path_suffix = os.path.sep.join(parts)
    if not java_file.endswith(expected_path_suffix):
      raise Exception(('Java package+class name do not match its path.\n'
                       'Actual path: %s\nExpected path: %s') %
                      (java_file, expected_path_suffix))

  def _ProcessInfo(self, java_file, package_name, class_names, source):
    for class_name in class_names:
      yield '{}.{}'.format(package_name, class_name)
      # Skip aidl srcjars since they don't indent code correctly.
      if '_aidl.srcjar' in source:
        continue
      assert not self._chromium_code or len(class_names) == 1, (
          'Chromium java files must only have one class: {}'.format(source))
      if self._chromium_code:
        # This check is not necessary but nice to check this somewhere.
        self._CheckPathMatchesClassName(java_file, package_name, class_names[0])

  def _ShouldIncludeInJarInfo(self, fully_qualified_name):
    name_as_class_glob = fully_qualified_name.replace('.', '/') + '.class'
    return not build_utils.MatchesGlob(name_as_class_glob, self._excluded_globs)

  def _Collect(self):
    if self._pool is None:
      return {}
    ret = {}
    for result in self._results:
      for java_file, package_name, class_names in result:
        source = self._srcjar_files.get(java_file, java_file)
        for fully_qualified_name in self._ProcessInfo(java_file, package_name,
                                                      class_names, source):
          if self._ShouldIncludeInJarInfo(fully_qualified_name):
            ret[fully_qualified_name] = java_file
    self._pool.terminate()
    return ret

  def __del__(self):
    # Work around for Python 2.x bug with multiprocessing and daemon threads:
    # https://bugs.python.org/issue4106
    if self._pool is not None:
      logging.info('Joining multiprocessing.Pool')
      self._pool.terminate()
      self._pool.join()
      logging.info('Done.')

  def Commit(self, output_path):
    """Writes a .jar.info file.

    Maps fully qualified names for classes to either the java file that they
    are defined in or the path of the srcjar that they came from.
    """
    logging.info('Collecting info file entries')
    entries = self._Collect()

    logging.info('Writing info file: %s', output_path)
    with build_utils.AtomicOutput(output_path) as f:
      jar_info_utils.WriteJarInfoFile(f, entries, self._srcjar_files)
    logging.info('Completed info file: %s', output_path)


def _CreateJarFile(jar_path, service_provider_configuration_dir,
                   additional_jar_files, classes_dir):
  logging.info('Start creating jar file: %s', jar_path)
  with build_utils.AtomicOutput(jar_path) as f:
    with zipfile.ZipFile(f.name, 'w') as z:
      build_utils.ZipDir(z, classes_dir)
      if service_provider_configuration_dir:
        config_files = build_utils.FindInDirectory(
            service_provider_configuration_dir)
        for config_file in config_files:
          zip_path = os.path.relpath(config_file,
                                     service_provider_configuration_dir)
          build_utils.AddToZipHermetic(z, zip_path, src_path=config_file)

      if additional_jar_files:
        for src_path, zip_path in additional_jar_files:
          build_utils.AddToZipHermetic(z, zip_path, src_path=src_path)
  logging.info('Completed jar file: %s', jar_path)


def _OnStaleMd5(options, javac_cmd, javac_args, java_files):
  logging.info('Starting _OnStaleMd5')
  if options.enable_kythe_annotations:
    # Kythe requires those env variables to be set and compile_java.py does the
    # same
    if not os.environ.get('KYTHE_ROOT_DIRECTORY') or \
        not os.environ.get('KYTHE_OUTPUT_DIRECTORY'):
      raise Exception('--enable-kythe-annotations requires '
                      'KYTHE_ROOT_DIRECTORY and KYTHE_OUTPUT_DIRECTORY '
                      'environment variables to be set.')
    javac_extractor_cmd = build_utils.JavaCmd() + [
        '-jar',
        _JAVAC_EXTRACTOR,
    ]
    try:
      _RunCompiler(options, javac_extractor_cmd + javac_args, java_files,
                   options.classpath, options.jar_path + '.javac_extractor',
                   save_outputs=False),
    except build_utils.CalledProcessError as e:
      # Having no index for particular target is better than failing entire
      # codesearch. Log and error and move on.
      logging.error('Could not generate kzip: %s', e)

  # Compiles with Error Prone take twice as long to run as pure javac. Thus GN
  # rules run both in parallel, with Error Prone only used for checks.
  _RunCompiler(options, javac_cmd + javac_args, java_files,
               options.classpath, options.jar_path,
               save_outputs=not options.enable_errorprone)
  logging.info('Completed all steps in _OnStaleMd5')


def _RunCompiler(options, javac_cmd, java_files, classpath, jar_path,
                 save_outputs=True):
  logging.info('Starting _RunCompiler')

  # Compiles with Error Prone take twice as long to run as pure javac. Thus GN
  # rules run both in parallel, with Error Prone only used for checks.
  save_outputs = not options.enable_errorprone

  # Use jar_path's directory to ensure paths are relative (needed for goma).
  temp_dir = jar_path + '.staging'
  shutil.rmtree(temp_dir, True)
  os.makedirs(temp_dir)
  try:
    classes_dir = os.path.join(temp_dir, 'classes')
    service_provider_configuration = os.path.join(
        temp_dir, 'service_provider_configuration')

    if save_outputs:
      input_srcjars_dir = os.path.join(options.generated_dir, 'input_srcjars')
      annotation_processor_outputs_dir = os.path.join(
          options.generated_dir, 'annotation_processor_outputs')
      # Delete any stale files in the generated directory. The purpose of
      # options.generated_dir is for codesearch.
      shutil.rmtree(options.generated_dir, True)
      info_file_context = _InfoFileContext(options.chromium_code,
                                           options.jar_info_exclude_globs)
    else:
      input_srcjars_dir = os.path.join(temp_dir, 'input_srcjars')
      annotation_processor_outputs_dir = os.path.join(
          temp_dir, 'annotation_processor_outputs')

    if options.java_srcjars:
      logging.info('Extracting srcjars to %s', input_srcjars_dir)
      build_utils.MakeDirectory(input_srcjars_dir)
      for srcjar in options.java_srcjars:
        extracted_files = build_utils.ExtractAll(
            srcjar, no_clobber=True, path=input_srcjars_dir, pattern='*.java')
        java_files.extend(extracted_files)
        if save_outputs:
          info_file_context.AddSrcJarSources(srcjar, extracted_files,
                                             input_srcjars_dir)
      logging.info('Done extracting srcjars')

    if options.header_jar:
      logging.info('Extracting service provider configs')
      # Extract META-INF/services/* so that it can be copied into the output
      # .jar
      build_utils.ExtractAll(options.header_jar,
                             no_clobber=True,
                             path=service_provider_configuration,
                             pattern='META-INF/services/*')
      logging.info('Done extracting service provider configs')

    if save_outputs and java_files:
      info_file_context.SubmitFiles(java_files)

    if java_files:
      # Don't include the output directory in the initial set of args since it
      # being in a temp dir makes it unstable (breaks md5 stamping).
      cmd = list(javac_cmd)
      os.makedirs(classes_dir)
      cmd += ['-d', classes_dir]

      if options.processors:
        os.makedirs(annotation_processor_outputs_dir)
        cmd += ['-s', annotation_processor_outputs_dir]

      if classpath:
        cmd += ['-classpath', ':'.join(classpath)]

      # Pass source paths as response files to avoid extremely long command
      # lines that are tedius to debug.
      java_files_rsp_path = os.path.join(temp_dir, 'files_list.txt')
      with open(java_files_rsp_path, 'w') as f:
        f.write(' '.join(java_files))
      cmd += ['@' + java_files_rsp_path]


      # |errorprone_expected_warning_regex| is used in tests for errorprone
      # warnings. Fail compile if expected warning is not present.
      stderr_filter = ProcessJavacOutput
      if (options.enable_errorprone
          and options.errorprone_expected_warning_regex):
        stderr_filter = functools.partial(
            CheckErrorproneStderrWarning, options.jar_path,
            options.errorprone_expected_warning_regex)

      logging.debug('Build command %s', cmd)
      start = time.time()
      build_utils.CheckOutput(cmd,
                              print_stdout=options.chromium_code,
                              stdout_filter=ProcessJavacOutput,
                              stderr_filter=stderr_filter,
                              fail_on_output=options.warnings_as_errors)
      end = time.time() - start
      logging.info('Java compilation took %ss', end)

    if save_outputs:
      if options.processors:
        annotation_processor_java_files = build_utils.FindInDirectory(
            annotation_processor_outputs_dir)
        if annotation_processor_java_files:
          info_file_context.SubmitFiles(annotation_processor_java_files)

      _CreateJarFile(jar_path, service_provider_configuration,
                     options.additional_jar_files, classes_dir)

      info_file_context.Commit(jar_path + '.info')
    else:
      build_utils.Touch(jar_path)

    logging.info('Completed all steps in _RunCompiler')
  finally:
    shutil.rmtree(temp_dir)


def _ParseOptions(argv):
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)

  parser.add_option(
      '--java-srcjars',
      action='append',
      default=[],
      help='List of srcjars to include in compilation.')
  parser.add_option(
      '--generated-dir',
      help='Subdirectory within target_gen_dir to place extracted srcjars and '
      'annotation processor output for codesearch to find.')
  parser.add_option(
      '--bootclasspath',
      action='append',
      default=[],
      help='Boot classpath for javac. If this is specified multiple times, '
      'they will all be appended to construct the classpath.')
  parser.add_option(
      '--java-version',
      help='Java language version to use in -source and -target args to javac.')
  parser.add_option('--classpath', action='append', help='Classpath to use.')
  parser.add_option(
      '--processors',
      action='append',
      help='GN list of annotation processor main classes.')
  parser.add_option(
      '--processorpath',
      action='append',
      help='GN list of jars that comprise the classpath used for Annotation '
      'Processors.')
  parser.add_option(
      '--processor-arg',
      dest='processor_args',
      action='append',
      help='key=value arguments for the annotation processors.')
  parser.add_option(
      '--additional-jar-file',
      dest='additional_jar_files',
      action='append',
      help='Additional files to package into jar. By default, only Java .class '
      'files are packaged into the jar. Files should be specified in '
      'format <filename>:<path to be placed in jar>.')
  parser.add_option(
      '--jar-info-exclude-globs',
      help='GN list of exclude globs to filter from generated .info files.')
  parser.add_option(
      '--chromium-code',
      type='int',
      help='Whether code being compiled should be built with stricter '
      'warnings for chromium code.')
  parser.add_option(
      '--gomacc-path', help='When set, prefix javac command with gomacc')
  parser.add_option(
      '--errorprone-path', help='Use the Errorprone compiler at this path.')
  parser.add_option(
      '--enable-errorprone',
      action='store_true',
      help='Enable errorprone checks')
  parser.add_option(
      '--errorprone-expected-warning-regex',
      help='When set, throws an exception if the errorprone compile does not '
      'log a warning which matches the regex.')
  parser.add_option(
      '--warnings-as-errors',
      action='store_true',
      help='Treat all warnings as errors.')
  parser.add_option('--jar-path', help='Jar output path.')
  parser.add_option(
      '--javac-arg',
      action='append',
      default=[],
      help='Additional arguments to pass to javac.')
  parser.add_option(
      '--enable-kythe-annotations',
      action='store_true',
      help='Enable generation of Kythe kzip, used for codesearch. Ensure '
      'proper environment variables are set before using this flag.')
  parser.add_option(
      '--header-jar',
      help='This is the header jar for the current target that contains '
      'META-INF/services/* files to be included in the output jar.')

  options, args = parser.parse_args(argv)
  build_utils.CheckOptions(options, parser, required=('jar_path', ))

  options.bootclasspath = build_utils.ParseGnList(options.bootclasspath)
  options.classpath = build_utils.ParseGnList(options.classpath)
  options.processorpath = build_utils.ParseGnList(options.processorpath)
  options.processors = build_utils.ParseGnList(options.processors)
  options.java_srcjars = build_utils.ParseGnList(options.java_srcjars)
  options.jar_info_exclude_globs = build_utils.ParseGnList(
      options.jar_info_exclude_globs)

  additional_jar_files = []
  for arg in options.additional_jar_files or []:
    filepath, jar_filepath = arg.split(':')
    additional_jar_files.append((filepath, jar_filepath))
  options.additional_jar_files = additional_jar_files

  java_files = []
  for arg in args:
    # Interpret a path prefixed with @ as a file containing a list of sources.
    if arg.startswith('@'):
      java_files.extend(build_utils.ReadSourcesList(arg[1:]))
    else:
      java_files.append(arg)

  return options, java_files


def main(argv):
  build_utils.InitLogging('JAVAC_DEBUG')
  colorama.init()

  argv = build_utils.ExpandFileArgs(argv)
  options, java_files = _ParseOptions(argv)

  javac_cmd = []
  if options.gomacc_path:
    javac_cmd.append(options.gomacc_path)
  javac_cmd.append(build_utils.JAVAC_PATH)

  javac_args = [
      '-g',
      # Chromium only allows UTF8 source files.  Being explicit avoids
      # javac pulling a default encoding from the user's environment.
      '-encoding',
      'UTF-8',
      # Prevent compiler from compiling .java files not listed as inputs.
      # See: http://blog.ltgt.net/most-build-tools-misuse-javac/
      '-sourcepath',
      ':',
  ]

  if options.enable_errorprone:
    # All errorprone args are passed space-separated in a single arg.
    errorprone_flags = ['-Xplugin:ErrorProne']
    # Make everything a warning so that when treat_warnings_as_errors is false,
    # they do not fail the build.
    errorprone_flags += ['-XepAllErrorsAsWarnings']
    for warning in ERRORPRONE_WARNINGS_TO_DISABLE:
      errorprone_flags.append('-Xep:{}:OFF'.format(warning))
    for warning in ERRORPRONE_WARNINGS_TO_ENABLE:
      errorprone_flags.append('-Xep:{}:WARN'.format(warning))
    javac_args += ['-XDcompilePolicy=simple', ' '.join(errorprone_flags)]
    # This flag quits errorprone after checks and before code generation, since
    # we do not need errorprone outputs, this speeds up errorprone by 4 seconds
    # for chrome_java.
    javac_args += ['-XDshould-stop.ifNoError=FLOW']

  if options.java_version:
    javac_args.extend([
        '-source',
        options.java_version,
        '-target',
        options.java_version,
    ])
  if options.java_version == '1.8':
    # Android's boot jar doesn't contain all java 8 classes.
    options.bootclasspath.append(build_utils.RT_JAR_PATH)

  if options.processors:
    javac_args.extend(['-processor', ','.join(options.processors)])
  else:
    # This effectively disables all annotation processors, even including
    # annotation processors in service provider configuration files named
    # META-INF/. See the following link for reference:
    #     https://docs.oracle.com/en/java/javase/11/tools/javac.html
    javac_args.extend(['-proc:none'])

  if options.bootclasspath:
    javac_args.extend(['-bootclasspath', ':'.join(options.bootclasspath)])

  if options.processorpath:
    javac_args.extend(['-processorpath', ':'.join(options.processorpath)])
  if options.processor_args:
    for arg in options.processor_args:
      javac_args.extend(['-A%s' % arg])

  javac_args.extend(options.javac_arg)

  classpath_inputs = (
      options.bootclasspath + options.classpath + options.processorpath)

  depfile_deps = classpath_inputs
  # Files that are already inputs in GN should go in input_paths.
  input_paths = depfile_deps + options.java_srcjars + java_files
  if options.header_jar:
    input_paths.append(options.header_jar)
  input_paths += [x[0] for x in options.additional_jar_files]

  output_paths = [
      options.jar_path,
      options.jar_path + '.info',
  ]

  input_strings = javac_cmd + javac_args + options.classpath + java_files + [
      options.warnings_as_errors, options.jar_info_exclude_globs
  ]

  md5_check.CallAndWriteDepfileIfStale(
      lambda: _OnStaleMd5(options, javac_cmd, javac_args, java_files),
      options,
      depfile_deps=depfile_deps,
      input_paths=input_paths,
      input_strings=input_strings,
      output_paths=output_paths)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
