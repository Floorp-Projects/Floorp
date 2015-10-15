# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozpack.packager.formats import (
    FlatFormatter,
    JarFormatter,
    OmniJarFormatter,
)
from mozpack.packager import (
    preprocess_manifest,
    preprocess,
    Component,
    SimpleManifestSink,
)
from mozpack.files import (
    GeneratedFile,
    FileFinder,
    File,
)
from mozpack.copier import (
    FileCopier,
    Jarrer,
)
from mozpack.errors import errors
from mozpack.unify import UnifiedBuildFinder
import mozpack.path as mozpath
import buildconfig
from argparse import ArgumentParser
import os
from StringIO import StringIO
import subprocess
import platform
import mozinfo

# List of libraries to shlibsign.
SIGN_LIBS = [
    'softokn3',
    'nssdbm3',
    'freebl3',
    'freebl_32fpu_3',
    'freebl_32int_3',
    'freebl_32int64_3',
    'freebl_64fpu_3',
    'freebl_64int_3',
]


class ToolLauncher(object):
    '''
    Helper to execute tools like xpcshell with the appropriate environment.
        launcher = ToolLauncher()
        launcher.tooldir = '/path/to/tools'
        launcher.launch(['xpcshell', '-e', 'foo.js'])
    '''
    def __init__(self):
        self.tooldir = None

    def launch(self, cmd, extra_linker_path=None, extra_env={}):
        '''
        Launch the given command, passed as a list. The first item in the
        command list is the program name, without a path and without a suffix.
        These are determined from the tooldir member and the BIN_SUFFIX value.
        An extra_linker_path may be passed to give an additional directory
        to add to the search paths for the dynamic linker.
        An extra_env dict may be passed to give additional environment
        variables to export when running the command.
        '''
        assert self.tooldir
        cmd[0] = os.path.join(self.tooldir, 'bin',
                              cmd[0] + buildconfig.substs['BIN_SUFFIX'])
        if not extra_linker_path:
            extra_linker_path = os.path.join(self.tooldir, 'bin')
        env = dict(os.environ)
        for p in ['LD_LIBRARY_PATH', 'DYLD_LIBRARY_PATH']:
            if p in env:
                env[p] = extra_linker_path + ':' + env[p]
            else:
                env[p] = extra_linker_path
        for e in extra_env:
            env[e] = extra_env[e]

        # For VC12, make sure we can find the right bitness of pgort120.dll
        if 'VS120COMNTOOLS' in env and not buildconfig.substs['HAVE_64BIT_BUILD']:
            vc12dir = os.path.abspath(os.path.join(env['VS120COMNTOOLS'],
                                                   '../../VC/bin'))
            if os.path.exists(vc12dir):
                env['PATH'] = vc12dir + ';' + env['PATH']

        # Work around a bug in Python 2.7.2 and lower where unicode types in
        # environment variables aren't handled by subprocess.
        for k, v in env.items():
            if isinstance(v, unicode):
                env[k] = v.encode('utf-8')

        print >>errors.out, 'Executing', ' '.join(cmd)
        errors.out.flush()
        return subprocess.call(cmd, env=env)

    def can_launch(self):
        return self.tooldir is not None

launcher = ToolLauncher()


class LibSignFile(File):
    '''
    File class for shlibsign signatures.
    '''
    def copy(self, dest, skip_if_older=True):
        assert isinstance(dest, basestring)
        # os.path.getmtime returns a result in seconds with precision up to the
        # microsecond. But microsecond is too precise because shutil.copystat
        # only copies milliseconds, and seconds is not enough precision.
        if os.path.exists(dest) and skip_if_older and \
                int(os.path.getmtime(self.path) * 1000) <= \
                int(os.path.getmtime(dest) * 1000):
            return False
        if launcher.launch(['shlibsign', '-v', '-o', dest, '-i', self.path]):
            errors.fatal('Error while signing %s' % self.path)


def precompile_cache(formatter, source_path, gre_path, app_path):
    '''
    Create startup cache for the given application directory, using the
    given GRE path.
    - formatter is a Formatter instance where to add the startup cache.
    - source_path is the base path of the package.
    - gre_path is the GRE path, relative to source_path.
    - app_path is the application path, relative to source_path.
    Startup cache for all resources under resource://app/ are generated,
    except when gre_path == app_path, in which case it's under
    resource://gre/.
    '''
    from tempfile import mkstemp
    source_path = os.path.abspath(source_path)
    if app_path != gre_path:
        resource = 'app'
    else:
        resource = 'gre'
    app_path = os.path.join(source_path, app_path)
    gre_path = os.path.join(source_path, gre_path)

    fd, cache = mkstemp('.zip')
    os.close(fd)
    os.remove(cache)

    try:
        extra_env = {'MOZ_STARTUP_CACHE': cache}
        if buildconfig.substs.get('MOZ_TSAN'):
            extra_env['TSAN_OPTIONS'] = 'report_bugs=0'
        if buildconfig.substs.get('MOZ_ASAN'):
            extra_env['ASAN_OPTIONS'] = 'detect_leaks=0'
        if launcher.launch(['xpcshell', '-g', gre_path, '-a', app_path,
                            '-f', os.path.join(os.path.dirname(__file__),
                            'precompile_cache.js'),
                            '-e', 'precompile_startupcache("resource://%s/");'
                                  % resource],
                           extra_linker_path=gre_path,
                           extra_env=extra_env):
            errors.fatal('Error while running startup cache precompilation')
            return
        from mozpack.mozjar import JarReader
        jar = JarReader(cache)
        resource = '/resource/%s/' % resource
        for f in jar:
            if resource in f.filename:
                path = f.filename[f.filename.index(resource) + len(resource):]
                if formatter.contains(path):
                    formatter.add(f.filename, GeneratedFile(f.read()))
        jar.close()
    finally:
        if os.path.exists(cache):
            os.remove(cache)


class RemovedFiles(GeneratedFile):
    '''
    File class for removed-files. Is used as a preprocessor parser.
    '''
    def __init__(self, copier):
        self.copier = copier
        GeneratedFile.__init__(self, '')

    def handle_line(self, str):
        f = str.strip()
        if not f:
            return
        if self.copier.contains(f):
            errors.error('Removal of packaged file(s): %s' % f)
        self.content += f + '\n'


def split_define(define):
    '''
    Give a VAR[=VAL] string, returns a (VAR, VAL) tuple, where VAL defaults to
    1. Numeric VALs are returned as ints.
    '''
    if '=' in define:
        name, value = define.split('=', 1)
        try:
            value = int(value)
        except ValueError:
            pass
        return (name, value)
    return (define, 1)


class NoPkgFilesRemover(object):
    '''
    Formatter wrapper to handle NO_PKG_FILES.
    '''
    def __init__(self, formatter, has_manifest):
        assert 'NO_PKG_FILES' in os.environ
        self._formatter = formatter
        self._files = os.environ['NO_PKG_FILES'].split()
        if has_manifest:
            self._error = errors.error
            self._msg = 'NO_PKG_FILES contains file listed in manifest: %s'
        else:
            self._error = errors.warn
            self._msg = 'Skipping %s'

    def add_base(self, base, *args):
        self._formatter.add_base(base, *args)

    def add(self, path, content):
        if not any(mozpath.match(path, spec) for spec in self._files):
            self._formatter.add(path, content)
        else:
            self._error(self._msg % path)

    def add_manifest(self, entry):
        self._formatter.add_manifest(entry)

    def add_interfaces(self, path, content):
        self._formatter.add_interfaces(path, content)

    def contains(self, path):
        return self._formatter.contains(path)


def main():
    parser = ArgumentParser()
    parser.add_argument('-D', dest='defines', action='append',
                        metavar="VAR[=VAL]", help='Define a variable')
    parser.add_argument('--format', default='omni',
                        help='Choose the chrome format for packaging ' +
                        '(omni, jar or flat ; default: %(default)s)')
    parser.add_argument('--removals', default=None,
                        help='removed-files source file')
    parser.add_argument('--ignore-errors', action='store_true', default=False,
                        help='Transform errors into warnings.')
    parser.add_argument('--minify', action='store_true', default=False,
                        help='Make some files more compact while packaging')
    parser.add_argument('--minify-js', action='store_true',
                        help='Minify JavaScript files while packaging.')
    parser.add_argument('--js-binary',
                        help='Path to js binary. This is used to verify '
                        'minified JavaScript. If this is not defined, '
                        'minification verification will not be performed.')
    parser.add_argument('--jarlog', default='', help='File containing jar ' +
                        'access logs')
    parser.add_argument('--optimizejars', action='store_true', default=False,
                        help='Enable jar optimizations')
    parser.add_argument('--unify', default='',
                        help='Base directory of another build to unify with')
    parser.add_argument('manifest', default=None, nargs='?',
                        help='Manifest file name')
    parser.add_argument('source', help='Source directory')
    parser.add_argument('destination', help='Destination directory')
    parser.add_argument('--non-resource', nargs='+', metavar='PATTERN',
                        default=[],
                        help='Extra files not to be considered as resources')
    args = parser.parse_args()

    defines = dict(buildconfig.defines)
    if args.ignore_errors:
        errors.ignore_errors()

    if args.defines:
        for name, value in [split_define(d) for d in args.defines]:
            defines[name] = value

    copier = FileCopier()
    if args.format == 'flat':
        formatter = FlatFormatter(copier)
    elif args.format == 'jar':
        formatter = JarFormatter(copier, optimize=args.optimizejars)
    elif args.format == 'omni':
        formatter = OmniJarFormatter(copier,
                                     buildconfig.substs['OMNIJAR_NAME'],
                                     optimize=args.optimizejars,
                                     non_resources=args.non_resource)
    else:
        errors.fatal('Unknown format: %s' % args.format)

    # Adjust defines according to the requested format.
    if isinstance(formatter, OmniJarFormatter):
        defines['MOZ_OMNIJAR'] = 1
    elif 'MOZ_OMNIJAR' in defines:
        del defines['MOZ_OMNIJAR']

    respath = ''
    if 'RESPATH' in defines:
        respath = SimpleManifestSink.normalize_path(defines['RESPATH'])
    while respath.startswith('/'):
        respath = respath[1:]

    if args.unify:
        def is_native(path):
            path = os.path.abspath(path)
            return platform.machine() in mozpath.split(path)

        # Invert args.unify and args.source if args.unify points to the
        # native architecture.
        args.source, args.unify = sorted([args.source, args.unify],
                                         key=is_native, reverse=True)
        if is_native(args.source):
            launcher.tooldir = args.source
    elif not buildconfig.substs['CROSS_COMPILE']:
        launcher.tooldir = buildconfig.substs['LIBXUL_DIST']

    with errors.accumulate():
        finder_args = dict(
            minify=args.minify,
            minify_js=args.minify_js,
        )
        if args.js_binary:
            finder_args['minify_js_verify_command'] = [
                args.js_binary,
                os.path.join(os.path.abspath(os.path.dirname(__file__)),
                    'js-compare-ast.js')
            ]
        if args.unify:
            finder = UnifiedBuildFinder(FileFinder(args.source),
                                        FileFinder(args.unify),
                                        **finder_args)
        else:
            finder = FileFinder(args.source, **finder_args)
        if 'NO_PKG_FILES' in os.environ:
            sinkformatter = NoPkgFilesRemover(formatter,
                                              args.manifest is not None)
        else:
            sinkformatter = formatter
        sink = SimpleManifestSink(finder, sinkformatter)
        if args.manifest:
            preprocess_manifest(sink, args.manifest, defines)
        else:
            sink.add(Component(''), 'bin/*')
        sink.close(args.manifest is not None)

        if args.removals:
            removals_in = StringIO(open(args.removals).read())
            removals_in.name = args.removals
            removals = RemovedFiles(copier)
            preprocess(removals_in, removals, defines)
            copier.add(mozpath.join(respath, 'removed-files'), removals)

    # shlibsign libraries
    if launcher.can_launch():
        if not mozinfo.isMac:
            for lib in SIGN_LIBS:
                libbase = mozpath.join(respath, '%s%s') \
                    % (buildconfig.substs['DLL_PREFIX'], lib)
                libname = '%s%s' % (libbase, buildconfig.substs['DLL_SUFFIX'])
                if copier.contains(libname):
                    copier.add(libbase + '.chk',
                               LibSignFile(os.path.join(args.destination,
                                                        libname)))

    # Setup preloading
    if args.jarlog and os.path.exists(args.jarlog):
        from mozpack.mozjar import JarLog
        log = JarLog(args.jarlog)
        for p, f in copier:
            if not isinstance(f, Jarrer):
                continue
            key = JarLog.canonicalize(os.path.join(args.destination, p))
            if key in log:
                f.preload(log[key])

    # Fill startup cache
    if isinstance(formatter, OmniJarFormatter) and launcher.can_launch() \
      and buildconfig.substs['MOZ_DISABLE_STARTUPCACHE'] != '1':
        gre_path = None
        def get_bases():
            for b in sink.packager.get_bases(addons=False):
                for p in (mozpath.join('bin', b), b):
                    if os.path.exists(os.path.join(args.source, p)):
                        yield p
                        break
        for base in sorted(get_bases()):
            if not gre_path:
                gre_path = base
            base_path = sink.normalize_path(base)
            if base_path in formatter.omnijars:
                precompile_cache(formatter.omnijars[base_path],
                                 args.source, gre_path, base)

    copier.copy(args.destination)


if __name__ == '__main__':
    main()
