# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Script to produce an Android ARchive (.aar) containing the compiled
Gecko library binaries.  The AAR file is intended for use by local
developers using Gradle.
'''

from __future__ import absolute_import, print_function

import argparse
import hashlib
import os
import shutil
import sys
import zipfile

from mozbuild import util
from mozpack.copier import Jarrer
from mozpack.files import (
    File,
    FileFinder,
    JarFinder,
)
from mozpack.mozjar import JarReader

MAVEN_POM_TEMPLATE = r'''
<?xml version="1.0" encoding="UTF-8"?>
<project xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 http://maven.apache.org/xsd/maven-4.0.0.xsd" xmlns="http://maven.apache.org/POM/4.0.0"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <modelVersion>4.0.0</modelVersion>
  <groupId>{groupId}</groupId>
  <artifactId>{artifactId}</artifactId>
  <version>{version}</version>
  <packaging>{packaging}</packaging>
  <dependencies>
    {dependencies}
  </dependencies>
</project>
'''.lstrip()

MAVEN_POM_DEPENDENCY_TEMPLATE = r'''
  <dependency>
     <groupId>{groupId}</groupId>
     <artifactId>{artifactId}</artifactId>
     <version>{version}</version>
     <type>{packaging}</type>
  </dependency>
'''.lstrip()

IVY_XML_TEMPLATE = r'''
<?xml version="1.0" encoding="UTF-8"?>
<ivy-module version="2.0">
  <info organisation="{organisation}" module="{module}" revision="{revision}" status="integration" publication="{publication}"/>
  <configurations/>
  <publications>
    <artifact name="{name}" type="{type}" ext="{ext}"/>
  </publications>
  <dependencies>
    {dependencies}
  </dependencies>
</ivy-module>
'''.lstrip()

IVY_XML_DEPENDENCY_TEMPLATE = r'''
<dependency org="{organisation}" name="{name}" rev="{revision}" />
'''.lstrip()

def _zipdir(path, output_file):
    zip = zipfile.ZipFile(output_file, "w")
    for root, dirs, files in os.walk(path, topdown=True):
        archive_root = root.replace(path, '')
        for file in files:
            zip.write(os.path.join(root, file), os.path.join(archive_root, file))

def _generate_geckoview_classes_jar(distdir, base_path):
    base_folder = FileFinder(base_path, ignore=['gecko-R.jar'])

    # Unzip all jar files into $(DISTDIR)/geckoview_aar_classes.
    geckoview_aar_classes_path = os.path.join(distdir, 'geckoview_aar_classes')
    shutil.rmtree(geckoview_aar_classes_path, ignore_errors=True)
    util.ensureParentDir(geckoview_aar_classes_path)

    for p, f in base_folder.find('*.jar'):
        with zipfile.ZipFile(f.path) as zf:
            zf.extractall(geckoview_aar_classes_path)

    # Rezip them into a single classes.jar file.
    classes_jar_path =  os.path.join(distdir, 'classes.jar')
    _zipdir(geckoview_aar_classes_path, classes_jar_path)
    return File(classes_jar_path)

def package_geckolibs_aar(topsrcdir, distdir, appname, output_file):
    jarrer = Jarrer(optimize=False)

    srcdir = os.path.join(topsrcdir, 'mobile', 'android', 'geckoview_library', 'geckolibs')
    jarrer.add('AndroidManifest.xml', File(os.path.join(srcdir, 'AndroidManifest.xml')))
    jarrer.add('classes.jar', File(os.path.join(srcdir, 'classes.jar')))

    jni = FileFinder(os.path.join(distdir, appname, 'lib'))
    for p, f in jni.find('**/*.so'):
        jarrer.add(os.path.join('jni', p), f)

    # Include the buildinfo JSON as an asset, to give future consumers at least
    # a hope of determining where this AAR came from.
    json = FileFinder(distdir, ignore=['*.mozinfo.json'])
    for p, f in json.find('*.json'):
        jarrer.add(os.path.join('assets', p), f)

    # This neatly ignores omni.ja.
    assets = FileFinder(os.path.join(distdir, appname, 'assets'))
    for p, f in assets.find('**/*.so'):
        jarrer.add(os.path.join('assets', p), f)

    jarrer.copy(output_file)
    return 0

def package_geckoview_aar(topsrcdir, distdir, appname, output_file):
    jarrer = Jarrer(optimize=False)
    app_path = os.path.join(distdir, appname)
    assets = FileFinder(os.path.join(app_path, 'assets'), ignore=['*.so'])
    for p, f in assets.find('omni.ja'):
        jarrer.add(os.path.join('assets', p), f)

    # The folder that contains Fennec's JAR files and resources.
    base_path = os.path.join(distdir, '..', 'mobile', 'android', 'base')

    # The resource set is packaged during Fennec's build.
    resjar = JarReader(os.path.join(base_path, 'geckoview_resources.zip'))
    for p, f in JarFinder(base_path, resjar).find('*'):
        jarrer.add(os.path.join('res', p), f)

    # Package the contents of all Fennec JAR files into classes.jar.
    classes_jar_file = _generate_geckoview_classes_jar(distdir, base_path)
    jarrer.add('classes.jar', classes_jar_file)

    # Add R.txt.
    jarrer.add('R.txt', File(os.path.join(base_path, 'R.txt')))

    # Finally add AndroidManifest.xml.
    srcdir = os.path.join(topsrcdir, 'mobile', 'android', 'geckoview_library', 'geckoview')
    jarrer.add('AndroidManifest.xml', File(os.path.join(srcdir, 'AndroidManifest.xml')))

    jarrer.copy(output_file)
    return 0

def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument(dest='dir',
                        metavar='DIR',
                        help='Path to write Android ARchives and metadata to.')
    parser.add_argument('--verbose', '-v', default=False, action='store_true',
                        help='be verbose')
    parser.add_argument('--revision',
                        help='Revision identifier to write.')
    parser.add_argument('--topsrcdir',
                        help='Top source directory.')
    parser.add_argument('--distdir',
                        help='Distribution directory (usually $OBJDIR/dist).')
    parser.add_argument('--appname',
                        help='Application name (usually $MOZ_APP_NAME, like "fennec").')
    parser.add_argument('--purge-old', default=False, action='store_true',
                        help='Delete any existing output files in the output directory.')
    args = parser.parse_args(args)

    # An Ivy 'publication' date must be given in the form yyyyMMddHHmmss, and Mozilla buildids are in this format.
    if len(args.revision) != 14:
        raise ValueError('Revision must be in yyyyMMddHHmmss format: %s' % args.revision)

    paths_to_hash = []

    groupId='org.mozilla'
    packaging_type='aar'
    gecklibs_aar = os.path.join(args.dir, 'geckolibs-{revision}.aar').format(revision=args.revision)
    paths_to_hash.append(gecklibs_aar)
    geckoview_aar = os.path.join(args.dir, 'geckoview-{revision}.aar').format(revision=args.revision)
    paths_to_hash.append(geckoview_aar)

    if args.purge_old:
        old_output_finder = FileFinder(args.dir, find_executables=False)
        for p, f in old_output_finder.find('geckoview-*.*'):
            os.remove(f.path)
        for p, f in old_output_finder.find('geckolibs-*.*'):
            os.remove(f.path)
        for p, f in old_output_finder.find('ivy-*.*'):
            os.remove(f.path)

    package_geckolibs_aar(args.topsrcdir, args.distdir, args.appname, gecklibs_aar)
    package_geckoview_aar(args.topsrcdir, args.distdir, args.appname, geckoview_aar)

    geckolibs_pom_path = os.path.join(args.dir, 'geckolibs-{revision}.pom').format(revision=args.revision)
    paths_to_hash.append(geckolibs_pom_path)
    geckolibs_pom = MAVEN_POM_TEMPLATE.format(
            groupId=groupId,
            artifactId='geckolibs',
            version=args.revision,
            packaging=packaging_type,
            dependencies=''
        )

    with open(geckolibs_pom_path, 'wt') as f:
        f.write(geckolibs_pom)

    geckoview_pom_path = os.path.join(args.dir, 'geckoview-{revision}.pom').format(revision=args.revision)
    paths_to_hash.append(geckoview_pom_path)
    geckoview_pom = MAVEN_POM_TEMPLATE.format(
        groupId=groupId,
        artifactId='geckoview',
        version=args.revision,
        packaging=packaging_type,
        dependencies=MAVEN_POM_DEPENDENCY_TEMPLATE.format(
            groupId=groupId,
            artifactId='geckolibs',
            version=args.revision,
            packaging=packaging_type
        )
    )

    with open(geckoview_pom_path, 'wt') as f:
        f.write(geckoview_pom)

    geckolibs_ivy_path = os.path.join(args.dir, 'ivy-geckolibs-{revision}.xml').format(revision=args.revision)
    paths_to_hash.append(geckolibs_ivy_path)
    with open(geckolibs_ivy_path, 'wt') as f:
        f.write(IVY_XML_TEMPLATE.format(
            organisation=groupId,
            module='geckolibs',
            revision=args.revision,
            publication=args.revision, # A white lie.
            name='geckolibs',
            type=packaging_type,
            ext=packaging_type,
            dependencies=''
        ))

    geckoview_ivy_path = os.path.join(args.dir, 'ivy-geckoview-{revision}.xml').format(revision=args.revision)
    paths_to_hash.append(geckoview_ivy_path)
    with open(geckoview_ivy_path, 'wt') as f:
        f.write(IVY_XML_TEMPLATE.format(
            organisation=groupId,
            module='geckoview',
            revision=args.revision,
            publication=args.revision, # A white lie.
            name='geckoview',
            type=packaging_type,
            ext=packaging_type,
            dependencies=IVY_XML_DEPENDENCY_TEMPLATE.format(
                organisation=groupId,
                name='geckolibs',
                revision=args.revision)
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
