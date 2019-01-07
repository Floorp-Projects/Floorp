# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import fnmatch
import os
import subprocess


def collect_apks(path, pattern):
    matches = []
    for root, dirnames, filenames in os.walk(path):
        for filename in fnmatch.filter(filenames, pattern):
            matches.append(os.path.join(root, filename))
    return matches


def zipalign(path):
    unsigned_apks = collect_apks(path, '*-unsigned.apk')
    print("Found {apk_count} APK(s) to zipalign in {path}".format(apk_count=len(unsigned_apks), path=path))
    for apk in unsigned_apks:
        print("Zipaligning", apk)
        split = os.path.splitext(apk)
        print(subprocess.check_output(["zipalign", "-f", "-v", "-p", "4", apk, split[0] + "-aligned" + split[1]]))


def sign(path, store, store_token, key_alias, key_token):
    unsigned_apks = collect_apks(path, '*-aligned.apk')
    print("Found {apk_count} APK(s) to sign in {path}".format(apk_count=len(unsigned_apks), path=path))

    for apk in unsigned_apks:
        print("Signing", apk)
        print(subprocess.check_output([
            "apksigner", "sign",
            "--ks", store,
            "--ks-key-alias", key_alias,
            "--ks-pass", "file:%s" % store_token,
            "--key-pass", "file:%s" % key_token,
            "-v",
            "--out", apk.replace('unsigned', 'signed'), apk]))


def archive_result(path, archive):
    if not os.path.exists(archive):
        os.makedirs(archive)

    signed_apks = collect_apks(path, '*-signed-*.apk')
    print("Found {apk_count} APK(s) to archive in {path}".format(apk_count=len(signed_apks), path=path))

    for apk in signed_apks:
        print("Verifying", apk)
        print(subprocess.check_output(['apksigner', 'verify', apk]))

        destination = archive + "/" + os.path.basename(apk)
        print("Archiving", apk)
        print(" `->", destination)
        os.rename(apk, destination)


def main():
    parser = argparse.ArgumentParser(
        description='Zipaligns, signs and archives APKs')
    parser.add_argument('--path', dest="path", action="store", help='Root path to search for APK files')
    parser.add_argument('--zipalign', dest="zipalign", action="store_true", default=False,
                        help='Zipaligns APKs before signing')
    parser.add_argument('--archive', metavar="PATH", dest="archive", action="store", default=False,
                        help='Path to save sign APKs to')

    parser.add_argument('--store', metavar="PATH", dest="store", action="store", help='Path to keystore')
    parser.add_argument('--store-token', metavar="PATH", dest="store_token", action="store",
                        help='Path to keystore password file')
    parser.add_argument('--key-alias', metavar="ALIAS", dest="key_alias", action="store", help='Key alias')
    parser.add_argument('--key-token', metavar="PATH", dest="key_token", action="store",
                        help='Path to key password file')

    result = parser.parse_args()

    if result.zipalign:
        zipalign(result.path)

    sign(result.path, result.store, result.store_token, result.key_alias, result.key_token)

    if result.archive:
        archive_result(result.path, result.archive)


if __name__ == "__main__":
    main()
