# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Script to produce an Android ARchive (.aar) containing the compiled
Gecko library binaries.  The AAR file is intended for use by local
developers using Gradle.
'''

from __future__ import print_function

import argparse
import hashlib
import os
import sys

from mozbuild import util
from mozpack.copier import Jarrer
from mozpack.files import (
    File,
    FileFinder,
)

MAVEN_POM_TEMPLATE = r'''
<?xml version="1.0" encoding="UTF-8"?>
<project xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 http://maven.apache.org/xsd/maven-4.0.0.xsd" xmlns="http://maven.apache.org/POM/4.0.0"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <modelVersion>4.0.0</modelVersion>
  <groupId>{groupId}</groupId>
  <artifactId>{artifactId}</artifactId>
  <version>{version}</version>
  <packaging>{packaging}</packaging>
</project>
'''.lstrip()

IVY_XML_TEMPLATE = r'''
<?xml version="1.0" encoding="UTF-8"?>
<ivy-module version="2.0">
  <info organisation="{organisation}" module="{module}" revision="{revision}" status="integration" publication="{publication}"/>
  <configurations/>
  <publications>
    <artifact name="{name}" type="{type}" ext="{ext}"/>
  </publications>
  <dependencies/>
</ivy-module>
'''.lstrip()

def package_geckolibs_aar(topsrcdir, distdir, output_file):
    jarrer = Jarrer(optimize=False)

    srcdir = os.path.join(topsrcdir, 'mobile', 'android', 'geckoview_library', 'geckolibs')
    jarrer.add('AndroidManifest.xml', File(os.path.join(srcdir, 'AndroidManifest.xml')))
    jarrer.add('classes.jar', File(os.path.join(srcdir, 'classes.jar')))

    jni = FileFinder(os.path.join(distdir, 'fennec', 'lib'))
    for p, f in jni.find('**/*.so'):
        jarrer.add(os.path.join('jni', p), f)

    # Include the buildinfo JSON as an asset, to give future consumers at least
    # a hope of determining where this AAR came from.
    json = FileFinder(distdir, ignore=['*.mozinfo.json'])
    for p, f in json.find('*.json'):
        jarrer.add(os.path.join('assets', p), f)

    # This neatly ignores omni.ja.
    assets = FileFinder(os.path.join(distdir, 'fennec', 'assets'))
    for p, f in assets.find('**/*.so'):
        jarrer.add(os.path.join('assets', p), f)

    jarrer.copy(output_file)
    return 0


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument(dest='dir',
                        metavar='DIR',
                        help='Path to write geckolibs Android ARchive and metadata to.')
    parser.add_argument('--verbose', '-v', default=False, action='store_true',
                        help='be verbose')
    parser.add_argument('--revision',
                        help='Revision identifier to write.')
    parser.add_argument('--topsrcdir',
                        help='Top source directory.')
    parser.add_argument('--distdir',
                        help='Distribution directory (usually $OBJDIR/dist).')
    args = parser.parse_args(args)

    paths_to_hash = []

    aar = os.path.join(args.dir, 'geckolibs-{revision}.aar').format(revision=args.revision)
    paths_to_hash.append(aar)
    package_geckolibs_aar(args.topsrcdir, args.distdir, aar)

    pom = os.path.join(args.dir, 'geckolibs-{revision}.pom').format(revision=args.revision)
    paths_to_hash.append(pom)
    with open(pom, 'wt') as f:
        f.write(MAVEN_POM_TEMPLATE.format(
            groupId='org.mozilla',
            artifactId='geckolibs',
            version=args.revision,
            packaging='aar',
        ))

    ivy = os.path.join(args.dir, 'ivy-geckolibs-{revision}.xml').format(revision=args.revision)
    paths_to_hash.append(ivy)
    with open(ivy, 'wt') as f:
        f.write(IVY_XML_TEMPLATE.format(
            organisation='org.mozilla',
            module='geckolibs',
            revision=args.revision,
            publication=args.revision, # A white lie.
            name='geckolibs',
            type='aar',
            ext='aar',
        ))

    for p in paths_to_hash:
        sha = "%s.sha1" % p
        with open(sha, 'wt') as f:
            f.write(util.hash_file(p, hasher=hashlib.sha1()))
        if args.verbose:
            print(p)
            print(sha)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
