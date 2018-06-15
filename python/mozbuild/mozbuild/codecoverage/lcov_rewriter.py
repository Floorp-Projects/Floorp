# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from argparse import ArgumentParser
import json
import os

try:
    import urlparse
except ImportError:
    import urllib.parse as urlparse

from mozpack.chrome.manifest import parse_manifest
import mozpack.path as mozpath
from manifest_handler import ChromeManifestHandler

class LcovRecord(object):
    __slots__ = ("test_name",
                 "source_file",
                 "functions",
                 "function_exec_counts",
                 "function_count",
                 "covered_function_count",
                 "branches",
                 "branch_count",
                 "covered_branch_count",
                 "lines",
                 "line_count",
                 "covered_line_count")
    def __init__(self):
        self.functions = {}
        self.function_exec_counts = {}
        self.branches = {}
        self.lines = {}

    def __iadd__(self, other):

        # These shouldn't differ.
        self.source_file = other.source_file
        if hasattr(other, 'test_name'):
            self.test_name = other.test_name
        self.functions.update(other.functions)

        for name, count in other.function_exec_counts.iteritems():
            self.function_exec_counts[name] = count + self.function_exec_counts.get(name, 0)

        for key, taken in other.branches.iteritems():
            self.branches[key] = taken + self.branches.get(key, 0)

        for line, (exec_count, checksum) in other.lines.iteritems():
            new_exec_count = exec_count
            if line in self.lines:
                old_exec_count, _ = self.lines[line]
                new_exec_count += old_exec_count
            self.lines[line] = new_exec_count, checksum

        self.resummarize()
        return self

    def resummarize(self):
        # Re-calculate summaries after generating or splitting a record.
        self.function_count = len(self.functions.keys())
        # Function records may have moved between files, so filter here.
        self.function_exec_counts = {fn_name: count for fn_name, count in self.function_exec_counts.iteritems()
                                     if fn_name in self.functions.values()}
        self.covered_function_count = len([c for c in self.function_exec_counts.values() if c])
        self.line_count = len(self.lines)
        self.covered_line_count = len([c for c, _ in self.lines.values() if c])
        self.branch_count = len(self.branches)
        self.covered_branch_count = len([c for c in self.branches.values() if c])

class RecordRewriter(object):
    # Helper class for rewriting/spliting individual lcov records according
    # to what the preprocessor did.
    def __init__(self):
        self._ranges = None

    def _get_range(self, line):
        for start, end in self._ranges:
            if line < start:
                return None
            if line < end:
                return start, end
        return None

    def _get_mapped_line(self, line, r):
        inc_source, inc_start = self._current_pp_info[r]
        start, end = r
        offs = line - start
        return inc_start + offs

    def _get_record(self, inc_source):
        if inc_source in self._additions:
            gen_rec = self._additions[inc_source]
        else:
            gen_rec = LcovRecord()
            gen_rec.source_file = inc_source
            self._additions[inc_source] = gen_rec
        return gen_rec

    def _rewrite_lines(self, record):
        rewritten_lines = {}
        for ln, line_info in record.lines.iteritems():
            r = self._get_range(ln)
            if r is None:
                rewritten_lines[ln] = line_info
                continue
            new_ln = self._get_mapped_line(ln, r)
            inc_source, _ = self._current_pp_info[r]

            if inc_source != record.source_file:
                gen_rec = self._get_record(inc_source)
                gen_rec.lines[new_ln] = line_info
                continue

            # Move exec_count to the new lineno.
            rewritten_lines[new_ln] = line_info

        record.lines = rewritten_lines

    def _rewrite_functions(self, record):
        rewritten_fns = {}

        # Sometimes we get multiple entries for a named function ("top-level", for
        # instance). It's not clear the records that result are well-formed, but
        # we act as though if a function has multiple FN's, the corresponding
        # FNDA's are all the same.
        for ln, fn_name in record.functions.iteritems():
            r = self._get_range(ln)
            if r is None:
                rewritten_fns[ln] = fn_name
                continue
            new_ln = self._get_mapped_line(ln, r)
            inc_source, _ = self._current_pp_info[r]
            if inc_source != record.source_file:
                gen_rec = self._get_record(inc_source)
                gen_rec.functions[new_ln] = fn_name
                if fn_name in record.function_exec_counts:
                    gen_rec.function_exec_counts[fn_name] = record.function_exec_counts[fn_name]
                continue
            rewritten_fns[new_ln] = fn_name
        record.functions = rewritten_fns

    def _rewrite_branches(self, record):
        rewritten_branches = {}
        for (ln, block_number, branch_number), taken in record.branches.iteritems():
            r = self._get_range(ln)
            if r is None:
                rewritten_branches[ln, block_number, branch_number] = taken
                continue
            new_ln = self._get_mapped_line(ln, r)
            inc_source, _ = self._current_pp_info[r]
            if inc_source != record.source_file:
                gen_rec = self._get_record(inc_source)
                gen_rec.branches[(new_ln, block_number, branch_number)] = taken
                continue
            rewritten_branches[(new_ln, block_number, branch_number)] = taken

        record.branches = rewritten_branches

    def rewrite_record(self, record, pp_info):
        # Rewrite the lines in the given record according to preprocessor info
        # and split to additional records when pp_info has included file info.
        self._current_pp_info = dict([(tuple([int(l) for l in k.split(',')]), v) for k, v in pp_info.items()])
        self._ranges = sorted(self._current_pp_info.keys())
        self._additions = {}
        self._rewrite_lines(record)
        self._rewrite_functions(record)
        self._rewrite_branches(record)

        record.resummarize()

        generated_records = self._additions.values()
        for r in generated_records:
            r.resummarize()
        return generated_records

class LcovFile(object):
    # Simple parser/pretty-printer for lcov format.
    # lcov parsing based on http://ltp.sourceforge.net/coverage/lcov/geninfo.1.php

    # TN:<test name>
    # SF:<absolute path to the source file>
    # FN:<line number of function start>,<function name>
    # FNDA:<execution count>,<function name>
    # FNF:<number of functions found>
    # FNH:<number of function hit>
    # BRDA:<line number>,<block number>,<branch number>,<taken>
    # BRF:<number of branches found>
    # BRH:<number of branches hit>
    # DA:<line number>,<execution count>[,<checksum>]
    # LF:<number of instrumented lines>
    # LH:<number of lines with a non-zero execution count>
    # end_of_record
    PREFIX_TYPES = {
      'TN': 0,
      'SF': 0,
      'FN': 1,
      'FNDA': 1,
      'FNF': 0,
      'FNH': 0,
      'BRDA': 3,
      'BRF': 0,
      'BRH': 0,
      'DA': 2,
      'LH': 0,
      'LF': 0,
    }

    def __init__(self, lcov_paths):
        self.lcov_paths = lcov_paths

    def iterate_records(self, rewrite_source=None):
        current_source_file = None
        current_pp_info = None
        current_lines = []
        for lcov_path in self.lcov_paths:
            with open(lcov_path) as lcov_fh:
                for line in lcov_fh:
                    line = line.rstrip()
                    if not line:
                        continue

                    if line == 'end_of_record':
                        # We skip records that we couldn't rewrite, that is records for which
                        # rewrite_url returns None.
                        if current_source_file is not None:
                            yield (current_source_file, current_pp_info, current_lines)
                        current_source_file = None
                        current_pp_info = None
                        current_lines = []
                        continue

                    colon = line.find(':')
                    prefix = line[:colon]

                    if prefix == 'SF':
                        sf = line[(colon + 1):]
                        res = rewrite_source(sf) if rewrite_source is not None else (sf, None)
                        if res is None:
                            current_lines.append(line)
                        else:
                            current_source_file, current_pp_info = res
                            current_lines.append('SF:' + current_source_file)
                    else:
                        current_lines.append(line)

    def parse_record(self, record_content):
        self.current_record = LcovRecord()

        for line in record_content:
            colon = line.find(':')

            prefix = line[:colon]

            # We occasionally end up with multi-line scripts in data:
            # uris that will trip up the parser, just skip them for now.
            if colon < 0 or prefix not in self.PREFIX_TYPES:
                continue

            args = line[(colon + 1):].split(',', self.PREFIX_TYPES[prefix])

            def try_convert(a):
                try:
                    return int(a)
                except ValueError:
                    return a
            args = [try_convert(a) for a in args]

            try:
                LcovFile.__dict__['parse_' + prefix](self, *args)
            except ValueError:
                print("Encountered an error in %s:\n%s" %
                      (self.lcov_fh.name, line))
                raise
            except KeyError:
                print("Invalid lcov line start in %s:\n%s" %
                      (self.lcov_fh.name, line))
                raise
            except TypeError:
                print("Invalid lcov line start in %s:\n%s" %
                      (self.lcov_fh.name, line))
                raise

        ret = self.current_record
        self.current_record = LcovRecord()
        return ret

    def print_file(self, fh, rewrite_source, rewrite_record):
        for source_file, pp_info, record_content in self.iterate_records(rewrite_source):
            if pp_info is not None:
                record = self.parse_record(record_content)
                for r in rewrite_record(record, pp_info):
                    fh.write(self.format_record(r))
                fh.write(self.format_record(record))
            else:
                fh.write('\n'.join(record_content) + '\nend_of_record\n')

    def format_record(self, record):
        out_lines = []
        for name in LcovRecord.__slots__:
            if hasattr(record, name):
                out_lines.append(LcovFile.__dict__['format_' + name](self, record))
        return '\n'.join(out_lines) + '\nend_of_record\n'

    def format_test_name(self, record):
        return "TN:%s" % record.test_name

    def format_source_file(self, record):
        return "SF:%s" % record.source_file

    def format_functions(self, record):
        # Sorting results gives deterministic output (and is a lot faster than
        # using OrderedDict).
        fns = []
        for start_lineno, fn_name in sorted(record.functions.iteritems()):
            fns.append('FN:%s,%s' % (start_lineno, fn_name))
        return '\n'.join(fns)

    def format_function_exec_counts(self, record):
        fndas = []
        for name, exec_count in sorted(record.function_exec_counts.iteritems()):
            fndas.append('FNDA:%s,%s' % (exec_count, name))
        return '\n'.join(fndas)

    def format_function_count(self, record):
        return 'FNF:%s' % record.function_count

    def format_covered_function_count(self, record):
        return 'FNH:%s' % record.covered_function_count

    def format_branches(self, record):
        brdas = []
        for key in sorted(record.branches):
            taken = record.branches[key]
            taken = '-' if taken == 0 else taken
            brdas.append('BRDA:%s' %
                         ','.join(map(str, list(key) + [taken])))
        return '\n'.join(brdas)

    def format_branch_count(self, record):
        return 'BRF:%s' % record.branch_count

    def format_covered_branch_count(self, record):
        return 'BRH:%s' % record.covered_branch_count

    def format_lines(self, record):
        das = []
        for line_no, (exec_count, checksum) in sorted(record.lines.iteritems()):
            s = 'DA:%s,%s' % (line_no, exec_count)
            if checksum:
                s += ',%s' % checksum
            das.append(s)
        return '\n'.join(das)

    def format_line_count(self, record):
        return 'LF:%s' % record.line_count

    def format_covered_line_count(self, record):
        return 'LH:%s' % record.covered_line_count

    def parse_TN(self, test_name):
        self.current_record.test_name = test_name

    def parse_SF(self, source_file):
        self.current_record.source_file = source_file

    def parse_FN(self, start_lineno, fn_name):
        self.current_record.functions[start_lineno] = fn_name

    def parse_FNDA(self, exec_count, fn_name):
        self.current_record.function_exec_counts[fn_name] = exec_count

    def parse_FNF(self, function_count):
        self.current_record.function_count = function_count

    def parse_FNH(self, covered_function_count):
        self.current_record.covered_function_count = covered_function_count

    def parse_BRDA(self, line_number, block_number, branch_number, taken):
        taken = 0 if taken == '-' else taken
        self.current_record.branches[(line_number, block_number,
                                      branch_number)] = taken

    def parse_BRF(self, branch_count):
        self.current_record.branch_count = branch_count

    def parse_BRH(self, covered_branch_count):
        self.current_record.covered_branch_count = covered_branch_count

    def parse_DA(self, line_number, execution_count, checksum=None):
        self.current_record.lines[line_number] = (execution_count, checksum)

    def parse_LH(self, covered_line_count):
        self.current_record.covered_line_count = covered_line_count

    def parse_LF(self, line_count):
        self.current_record.line_count = line_count


class UrlFinderError(Exception):
    pass

class UrlFinder(object):
    # Given a "chrome://" or "resource://" url, uses data from the UrlMapBackend
    # and install manifests to find a path to the source file and the corresponding
    # (potentially pre-processed) file in the objdir.
    def __init__(self, chrome_map_path, appdir, gredir, extra_chrome_manifests):
        # Cached entries
        self._final_mapping = {}

        try:
            with open(chrome_map_path) as fh:
                url_prefixes, overrides, install_info, buildconfig = json.load(fh)
        except IOError:
            print("Error reading %s. Run |./mach build-backend -b ChromeMap| to "
                  "populate the ChromeMap backend." % chrome_map_path)
            raise

        self.topobjdir = buildconfig['topobjdir']
        self.MOZ_APP_NAME = buildconfig['MOZ_APP_NAME']
        self.OMNIJAR_NAME = buildconfig['OMNIJAR_NAME']

        # These are added dynamically in nsIResProtocolHandler, we might
        # need to get them at run time.
        if "resource:///" not in url_prefixes:
            url_prefixes["resource:///"] = [appdir]
        if "resource://gre/" not in url_prefixes:
            url_prefixes["resource://gre/"] = [gredir]

        self._url_prefixes = url_prefixes
        self._url_overrides = overrides

        self._respath = None

        mac_bundle_name = buildconfig['MOZ_MACBUNDLE_NAME']
        if mac_bundle_name:
            self._respath = mozpath.join('dist',
                                         mac_bundle_name,
                                         'Contents',
                                         'Resources')

        if not extra_chrome_manifests:
            extra_path = os.path.join(self.topobjdir, '_tests', 'extra.manifest')
            if os.path.isfile(extra_path):
                extra_chrome_manifests = [extra_path]

        if extra_chrome_manifests:
            self._populate_chrome(extra_chrome_manifests)

        self._install_mapping = install_info

    def _populate_chrome(self, manifests):
        handler = ChromeManifestHandler()
        for m in manifests:
            path = os.path.abspath(m)
            for e in parse_manifest(None, path):
                handler.handle_manifest_entry(e)
        self._url_overrides.update(handler.overrides)
        self._url_prefixes.update(handler.chrome_mapping)

    def _find_install_prefix(self, objdir_path):

        def _prefix(s):
            for p in mozpath.split(s):
                if '*' not in p:
                    yield p + '/'

        offset = 0
        for leaf in reversed(mozpath.split(objdir_path)):
            offset += len(leaf)
            if objdir_path[:-offset] in self._install_mapping:
                pattern_prefix, is_pp = self._install_mapping[objdir_path[:-offset]]
                full_leaf = objdir_path[len(objdir_path) - offset:]
                src_prefix = ''.join(_prefix(pattern_prefix))
                self._install_mapping[objdir_path] = (mozpath.join(src_prefix, full_leaf),
                                                      is_pp)
                break
            offset += 1

    def _install_info(self, objdir_path):
        if objdir_path not in self._install_mapping:
            # If our path is missing, some prefix of it may be in the install
            # mapping mapped to a wildcard.
            self._find_install_prefix(objdir_path)
        if objdir_path not in self._install_mapping:
            raise UrlFinderError("Couldn't find entry in manifest for %s" %
                                 objdir_path)
        return self._install_mapping[objdir_path]

    def _abs_objdir_install_info(self, term):
        obj_relpath = term[len(self.topobjdir) + 1:]
        res = self._install_info(obj_relpath)

        # Some urls on osx will refer to paths in the mac bundle, so we
        # re-interpret them as being their original location in dist/bin.
        if (not res and self._respath and
            obj_relpath.startswith(self._respath)):
            obj_relpath = obj_relpath.replace(self._respath, 'dist/bin')
            res = self._install_info(obj_relpath)

        if not res:
            raise UrlFinderError("Couldn't find entry in manifest for %s" %
                                 obj_relpath)
        return res

    def find_files(self, url):
        # Returns a tuple of (source file, pp_info)
        # for the given "resource:", "chrome:", or "file:" uri.
        term = url
        if term in self._url_overrides:
            term = self._url_overrides[term]

        if os.path.isabs(term) and term.startswith(self.topobjdir):
            source_path, pp_info = self._abs_objdir_install_info(term)
            return source_path, pp_info

        for prefix, dests in self._url_prefixes.iteritems():
            if term.startswith(prefix):
                for dest in dests:
                    if not dest.endswith('/'):
                        dest += '/'
                    objdir_path = term.replace(prefix, dest)

                    while objdir_path.startswith('//'):
                        # The mochitest harness produces some wonky file:// uris
                        # that need to be fixed.
                        objdir_path = objdir_path[1:]

                    try:
                        if os.path.isabs(objdir_path) and objdir_path.startswith(self.topobjdir):
                            return self._abs_objdir_install_info(objdir_path)
                        else:
                            src_path, pp_info = self._install_info(objdir_path)
                            return mozpath.normpath(src_path), pp_info
                    except UrlFinderError:
                        pass

                    if (dest.startswith('resource://') or
                        dest.startswith('chrome://')):
                        result = self.find_files(term.replace(prefix, dest))
                        if result:
                            return result

        raise UrlFinderError("No objdir path for %s" % term)

    def rewrite_url(self, url):
        # This applies one-off rules and returns None for urls that we aren't
        # going to be able to resolve to a source file ("about:" urls, for
        # instance).
        if url in self._final_mapping:
            return self._final_mapping[url]
        if url.endswith('> eval'):
            return None
        if url.endswith('> Function'):
            return None
        if ' -> ' in url:
            url = url.split(' -> ')[1].rstrip()
        if '?' in url:
            url = url.split('?')[0]

        url_obj = urlparse.urlparse(url)
        if url_obj.scheme == 'jar':
            app_name = self.MOZ_APP_NAME
            omnijar_name = self.OMNIJAR_NAME

            if app_name in url:
                if omnijar_name in url:
                    # e.g. file:///home/worker/workspace/build/application/firefox/omni.ja!/components/MainProcessSingleton.js
                    parts = url_obj.path.split(omnijar_name + '!', 1)
                elif '.xpi!' in url:
                    # e.g. file:///home/worker/workspace/build/application/firefox/browser/features/e10srollout@mozilla.org.xpi!/bootstrap.js
                    parts = url_obj.path.split('.xpi!', 1)
                else:
                    # We don't know how to handle this jar: path, so return it to the
                    # caller to make it print a warning.
                    return url_obj.path, None

                dir_parts = parts[0].rsplit(app_name + '/', 1)
                url = mozpath.normpath(mozpath.join(self.topobjdir, 'dist', 'bin', dir_parts[1].lstrip('/'), parts[1].lstrip('/')))
            elif '.xpi!' in url:
                # This matching mechanism is quite brittle and based on examples seen in the wild.
                # There's no rule to match the XPI name to the path in dist/xpi-stage.
                parts = url_obj.path.split('.xpi!', 1)
                addon_name = os.path.basename(parts[0])
                if '-test@mozilla.org' in addon_name:
                    addon_name = addon_name[:-len('-test@mozilla.org')]
                elif addon_name.endswith('@mozilla.org'):
                    addon_name = addon_name[:-len('@mozilla.org')]
                url = mozpath.normpath(mozpath.join(self.topobjdir, 'dist', 'xpi-stage', addon_name, parts[1].lstrip('/')))
        elif url_obj.scheme == 'file' and os.path.isabs(url_obj.path):
            path = url_obj.path
            if not os.path.isfile(path):
                # This may have been in a profile directory that no
                # longer exists.
                return None
            if not path.startswith(self.topobjdir):
                return path, None
            url = url_obj.path
        elif url_obj.scheme in ('http', 'https', 'javascript', 'data', 'about'):
            return None

        result = self.find_files(url)
        self._final_mapping[url] = result
        return result

class LcovFileRewriter(object):
    # Class for partial parses of LCOV format and rewriting to resolve urls
    # and preprocessed file lines.
    def __init__(self, chrome_map_path, appdir='dist/bin/browser/', gredir='dist/bin/', extra_chrome_manifests=[]):
        self.url_finder = UrlFinder(chrome_map_path, appdir, gredir, extra_chrome_manifests)
        self.pp_rewriter = RecordRewriter()

    def rewrite_files(self, in_paths, output_file, output_suffix):
        unknowns = set()
        found_valid = [False]

        def rewrite_source(url):
            try:
                res = self.url_finder.rewrite_url(url)
                if res is None:
                    return None
            except Exception as e:
                if url not in unknowns:
                    print("Error: %s.\nCouldn't find source info for %s, removing record" %
                          (e, url))
                unknowns.add(url)
                return None

            source_file, pp_info = res
            # We can't assert that the file exists here, because we don't have the source checkout available
            # on test machines. We can bring back this assertion when bug 1432287 is fixed.
            # assert os.path.isfile(source_file), "Couldn't find mapped source file %s at %s!" % (url, source_file)

            found_valid[0] = True

            return res

        in_paths = [os.path.abspath(in_path) for in_path in in_paths]

        if output_file:
            lcov_file = LcovFile(in_paths)
            with open(output_file, 'w+') as out_fh:
                lcov_file.print_file(out_fh, rewrite_source, self.pp_rewriter.rewrite_record)
        else:
            for in_path in in_paths:
                lcov_file = LcovFile([in_path])
                with open(in_path + output_suffix, 'w+') as out_fh:
                    lcov_file.print_file(out_fh, rewrite_source, self.pp_rewriter.rewrite_record)

        if not found_valid[0]:
            print("WARNING: No valid records found in %s" % in_paths)
            return


def main():
    parser = ArgumentParser(description="Given a set of gcov .info files produced "
                            "by spidermonkey's code coverage, re-maps file urls "
                            "back to source files and lines in preprocessed files "
                            "back to their original locations.")
    parser.add_argument("--chrome-map-path", default="chrome-map.json",
                        help="Path to the chrome-map.json file.")
    parser.add_argument("--app-dir", default="dist/bin/browser/",
                        help="Prefix of the appdir in use. This is used to map "
                             "urls starting with resource:///. It may differ by "
                             "app, but defaults to the valid value for firefox.")
    parser.add_argument("--gre-dir", default="dist/bin/",
                        help="Prefix of the gre dir in use. This is used to map "
                             "urls starting with resource://gre. It may differ by "
                             "app, but defaults to the valid value for firefox.")
    parser.add_argument("--output-suffix", default=".out",
                        help="The suffix to append to output files.")
    parser.add_argument("--extra-chrome-manifests", nargs='+',
                        help="Paths to files containing extra chrome registration.")
    parser.add_argument("--output-file", default="",
                        help="The output file where the results are merged. Leave empty to make the rewriter not merge files.")
    parser.add_argument("files", nargs='+',
                        help="The set of files to process.")

    args = parser.parse_args()

    rewriter = LcovFileRewriter(args.chrome_map_path, args.app_dir, args.gre_dir,
                                args.extra_chrome_manifests)

    files = []
    for f in args.files:
        if os.path.isdir(f):
            files += [os.path.join(f, e) for e in os.listdir(f)]
        else:
            files.append(f)

    rewriter.rewrite_files(files, args.output_file, args.output_suffix)

if __name__ == '__main__':
    main()
