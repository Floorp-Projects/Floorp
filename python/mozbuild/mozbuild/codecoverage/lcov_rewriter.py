# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from argparse import ArgumentParser
import json
import os
import re
import sys
import urlparse

from mozpack.copier import FileRegistry
from mozpack.files import PreprocessedFile
from mozpack.manifests import InstallManifest
from mozpack.chrome.manifest import parse_manifest
from chrome_map import ChromeManifestHandler

import buildconfig
import mozpack.path as mozpath

class UrlFinderError(Exception):
    pass

class UrlFinder(object):
    # Given a "chrome://" or "resource://" url, uses data from the UrlMapBackend
    # and install manifests to find a path to the source file and the corresponding
    # (potentially pre-processed) file in the objdir.
    def __init__(self, appdir, gredir, extra_chrome_manifests):
        # Normalized for "startswith" checks below.
        self.topobjdir = mozpath.normpath(buildconfig.topobjdir)

        # Cached entries
        self._final_mapping = {}

        info_file = os.path.join(self.topobjdir, 'chrome-map.json')

        try:
            with open(info_file) as fh:
                url_prefixes, overrides, install_info = json.load(fh)
        except IOError:
            print("Error reading %s. Run |./mach build-backend -b ChromeMap| to "
                  "populate the ChromeMap backend." % info_file)
            raise

        # These are added dynamically in nsIResProtocolHandler, we might
        # need to get them at run time.
        if "resource:///" not in url_prefixes:
            url_prefixes["resource:///"] = [appdir]
        if "resource://gre/" not in url_prefixes:
            url_prefixes["resource://gre/"] = [gredir]

        self._url_prefixes = url_prefixes
        self._url_overrides = overrides

        self._respath = None

        mac_bundle_name = buildconfig.substs.get('MOZ_MACBUNDLE_NAME')
        if mac_bundle_name:
            self._respath = mozpath.join('dist',
                                         mac_bundle_name,
                                         'Contents',
                                         'Resources')

        if extra_chrome_manifests:
            self._populate_chrome(extra_chrome_manifests)

        self._install_mapping = install_info
        self._populate_install_manifest()

    def _populate_chrome(self, manifests):
        handler = ChromeManifestHandler()
        for m in manifests:
            path = os.path.abspath(m)
            for e in parse_manifest(None, path):
                handler.handle_manifest_entry(e)
        self._url_overrides.update(handler.overrides)
        self._url_prefixes.update(handler.chrome_mapping)

    def _load_manifest(self, path, root):
        install_manifest = InstallManifest(path)
        reg = FileRegistry()
        install_manifest.populate_registry(reg)

        for dest, src in reg:
            if hasattr(src, 'path'):
                if not os.path.isabs(dest):
                    dest = root + dest
                self._install_mapping[dest] = (src.path,
                                               isinstance(src, PreprocessedFile))

    def _populate_install_manifest(self):
        mp = os.path.join(self.topobjdir, '_build_manifests', 'install',
                          '_tests')
        self._load_manifest(mp, root='_tests/')

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
        # Returns a tuple of (source file, objdir file, preprocessed)
        # for the given "resource:", "chrome:", or "file:" uri.
        term = url
        if term in self._url_overrides:
            term = self._url_overrides[term]

        if os.path.isabs(term) and term.startswith(self.topobjdir):
            source_path, preprocessed = self._abs_objdir_install_info(term)
            return source_path, term, preprocessed

        objdir_path = None
        for prefix, dests in self._url_prefixes.iteritems():
            if term.startswith(prefix):
                for dest in dests:
                    if not dest.endswith('/'):
                        dest += '/'
                    replaced = url.replace(prefix, dest)

                    if os.path.isfile(mozpath.join(self.topobjdir, replaced)):
                        objdir_path = replaced
                        break

                    if (dest.startswith('resource://') or
                        dest.startswith('chrome://')):
                        result = self.find_files(term.replace(prefix, dest))
                        if result:
                            return result

        if not objdir_path:
            raise UrlFinderError("No objdir path for %s" % term)
        while objdir_path.startswith('//'):
            # The mochitest harness produces some wonky file:// uris
            # that need to be fixed.
            objdir_path = objdir_path[1:]

        if os.path.isabs(objdir_path) and objdir_path.startswith(self.topobjdir):
            source_path, preprocessed = self._abs_objdir_install_info(objdir_path)
            return source_path, term, preprocessed

        src_path, preprocessed = self._install_info(objdir_path)
        return mozpath.normpath(src_path), objdir_path, preprocessed

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

        url_obj = urlparse.urlparse(url)
        if url_obj.scheme == 'file' and os.path.isabs(url_obj.path):
            path = url_obj.path
            if not os.path.isfile(path):
                # This may have been in a profile directory that no
                # longer exists.
                return None
            if not path.startswith(self.topobjdir):
                return path, None, False
            url = url_obj.path
        if url_obj.scheme in ('http', 'https', 'javascript', 'data', 'about'):
            return None

        result = self.find_files(url)
        self._final_mapping[url] = result
        return result

class LcovFileRewriter(object):
    # Class for partial parses of LCOV format and rewriting to resolve urls
    # and preprocessed file lines.
    def __init__(self, appdir, gredir, extra_chrome_manifests):
        self.topobjdir = buildconfig.topobjdir
        self.url_finder = UrlFinder(appdir, gredir, extra_chrome_manifests)
        self._line_comment_re = re.compile('//@line \d+ "(.+)"$')

    def rewrite_file(self, in_path, output_suffix):
        in_path = os.path.abspath(in_path)
        out_path = in_path + output_suffix
        skip_section = False
        with open(in_path) as fh, open(out_path, 'w+') as out_fh:
            for line in fh:
                if skip_section:
                    if line.rstrip() == 'end_of_record':
                        skip_section = False
                    continue
                if line.startswith('SF:'):
                    url = line[3:].rstrip()
                    try:
                        res = self.url_finder.rewrite_url(url)
                        if res is None:
                            skip_section = True
                            continue
                    except UrlFinderError as e:
                        print("Error: %s.\nCouldn't find source info for %s" %
                              (e, url))
                        skip_section = True
                        continue
                    src_file, objdir_file, _ = res
                    assert os.path.isfile(src_file), "Couldn't find mapped source file at %s!" % src_file
                    line = 'SF:%s\n' % src_file
                out_fh.write(line)

def main():
    parser = ArgumentParser(description="Given a set of gcov .info files produced "
                            "by spidermonkey's code coverage, re-maps file urls "
                            "back to source files and lines in preprocessed files "
                            "back to their original locations.")
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
    parser.add_argument("files", nargs='+',
                        help="The set of files to process.")

    args = parser.parse_args()
    if not args.extra_chrome_manifests:
        extra_path = os.path.join(buildconfig.topobjdir, '_tests',
                                  'extra.manifest')
        if os.path.isfile(extra_path):
            args.extra_chrome_manifests = [extra_path]

    rewriter = LcovFileRewriter(args.app_dir, args.gre_dir,
                                args.extra_chrome_manifests)

    for f in args.files:
        rewriter.rewrite_file(f, args.output_suffix)

if __name__ == '__main__':
    main()
