#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import shutil
import re
import tempfile
from optparse import OptionParser
from subprocess import check_call
from subprocess import check_output

nssutil_h = "lib/util/nssutil.h"
softkver_h = "lib/softoken/softkver.h"
nss_h = "lib/nss/nss.h"
nssckbi_h = "lib/ckfw/builtins/nssckbi.h"
abi_base_version_file = "automation/abi-check/previous-nss-release"

abi_report_files = ['automation/abi-check/expected-report-libfreebl3.so.txt',
                    'automation/abi-check/expected-report-libfreeblpriv3.so.txt',
                    'automation/abi-check/expected-report-libnspr4.so.txt',
                    'automation/abi-check/expected-report-libnss3.so.txt',
                    'automation/abi-check/expected-report-libnssckbi.so.txt',
                    'automation/abi-check/expected-report-libnssdbm3.so.txt',
                    'automation/abi-check/expected-report-libnsssysinit.so.txt',
                    'automation/abi-check/expected-report-libnssutil3.so.txt',
                    'automation/abi-check/expected-report-libplc4.so.txt',
                    'automation/abi-check/expected-report-libplds4.so.txt',
                    'automation/abi-check/expected-report-libsmime3.so.txt',
                    'automation/abi-check/expected-report-libsoftokn3.so.txt',
                    'automation/abi-check/expected-report-libssl3.so.txt']


def check_call_noisy(cmd, *args, **kwargs):
    print("Executing command: {}".format(cmd))
    check_call(cmd, *args, **kwargs)


def exit_with_failure(what):
    print("failure: {}".format(what))
    sys.exit(2)


def check_files_exist():
    if (not os.path.exists(nssutil_h) or not os.path.exists(softkver_h)
            or not os.path.exists(nss_h) or not os.path.exists(nssckbi_h)):
        exit_with_failure("cannot find expected header files, must run from inside NSS hg directory")


class Replacement():
    def __init__(self, regex="", repl=""):
        self.regex = regex
        self.repl = repl
        self.matcher = re.compile(self.regex)

    def replace(self, line):
        return self.matcher.sub(self.repl, line)


def inplace_replace(replacements=[], filename=""):
    for r in replacements:
        if not isinstance(r, Replacement):
            raise TypeError("Expecting a list of Replacement objects")

    with tempfile.NamedTemporaryFile(mode="w", delete=False) as tmp_file:
        with open(filename) as in_file:
            for line in in_file:
                for r in replacements:
                    line = r.replace(line)
                tmp_file.write(line)
        tmp_file.flush()

        shutil.copystat(filename, tmp_file.name)
        shutil.move(tmp_file.name, filename)


def toggle_beta_status(is_beta):
    check_files_exist()
    if (is_beta):
        print("adding Beta status to version numbers")
        inplace_replace(filename=nssutil_h, replacements=[
            Replacement(regex=r'^(#define *NSSUTIL_VERSION *\"[0-9.]+)\" *$',
                        repl=r'\g<1> Beta"'),
            Replacement(regex=r'^(#define *NSSUTIL_BETA *)PR_FALSE *$',
                        repl=r'\g<1>PR_TRUE')])
        inplace_replace(filename=softkver_h, replacements=[
            Replacement(regex=r'^(#define *SOFTOKEN_VERSION *\"[0-9.]+\" *SOFTOKEN_ECC_STRING) *$',
                        repl=r'\g<1> " Beta"'),
            Replacement(regex=r'^(#define *SOFTOKEN_BETA *)PR_FALSE *$',
                        repl=r'\g<1>PR_TRUE')])
        inplace_replace(filename=nss_h, replacements=[
            Replacement(regex=r'^(#define *NSS_VERSION *\"[0-9.]+\" *_NSS_CUSTOMIZED) *$',
                        repl=r'\g<1> " Beta"'),
            Replacement(regex=r'^(#define *NSS_BETA *)PR_FALSE *$',
                        repl=r'\g<1>PR_TRUE')])
    else:
        print("removing Beta status from version numbers")
        inplace_replace(filename=nssutil_h, replacements=[
            Replacement(regex=r'^(#define *NSSUTIL_VERSION *\"[0-9.]+) *Beta\" *$',
                        repl=r'\g<1>"'),
            Replacement(regex=r'^(#define *NSSUTIL_BETA *)PR_TRUE *$',
                        repl=r'\g<1>PR_FALSE')])
        inplace_replace(filename=softkver_h, replacements=[
            Replacement(regex=r'^(#define *SOFTOKEN_VERSION *\"[0-9.]+\" *SOFTOKEN_ECC_STRING) *\" *Beta\" *$',
                        repl=r'\g<1>'),
            Replacement(regex=r'^(#define *SOFTOKEN_BETA *)PR_TRUE *$',
                        repl=r'\g<1>PR_FALSE')])
        inplace_replace(filename=nss_h, replacements=[
            Replacement(regex=r'^(#define *NSS_VERSION *\"[0-9.]+\" *_NSS_CUSTOMIZED) *\" *Beta\" *$',
                        repl=r'\g<1>'),
            Replacement(regex=r'^(#define *NSS_BETA *)PR_TRUE *$',
                        repl=r'\g<1>PR_FALSE')])

    print("please run 'hg stat' and 'hg diff' to verify the files have been verified correctly")


def print_beta_versions():
    check_call_noisy(["egrep", "#define *NSSUTIL_VERSION|#define *NSSUTIL_BETA", nssutil_h])
    check_call_noisy(["egrep", "#define *SOFTOKEN_VERSION|#define *SOFTOKEN_BETA", softkver_h])
    check_call_noisy(["egrep", "#define *NSS_VERSION|#define *NSS_BETA", nss_h])


def remove_beta_status():
    print("--- removing beta flags. Existing versions were:")
    print_beta_versions()
    toggle_beta_status(False)
    print("--- finished modifications, new versions are:")
    print_beta_versions()


def set_beta_status():
    print("--- adding beta flags. Existing versions were:")
    print_beta_versions()
    toggle_beta_status(True)
    print("--- finished modifications, new versions are:")
    print_beta_versions()


def print_library_versions():
    check_files_exist()
    check_call_noisy(["egrep", "#define *NSSUTIL_VERSION|#define NSSUTIL_VMAJOR|#define *NSSUTIL_VMINOR|#define *NSSUTIL_VPATCH|#define *NSSUTIL_VBUILD|#define *NSSUTIL_BETA", nssutil_h])
    check_call_noisy(["egrep", "#define *SOFTOKEN_VERSION|#define SOFTOKEN_VMAJOR|#define *SOFTOKEN_VMINOR|#define *SOFTOKEN_VPATCH|#define *SOFTOKEN_VBUILD|#define *SOFTOKEN_BETA", softkver_h])
    check_call_noisy(["egrep", "#define *NSS_VERSION|#define NSS_VMAJOR|#define *NSS_VMINOR|#define *NSS_VPATCH|#define *NSS_VBUILD|#define *NSS_BETA", nss_h])


def print_root_ca_version():
    check_files_exist()
    check_call_noisy(["grep", "define *NSS_BUILTINS_LIBRARY_VERSION", nssckbi_h])


def ensure_arguments_after_action(how_many, usage):
    if (len(sys.argv) != (2 + how_many)):
        exit_with_failure("incorrect number of arguments, expected parameters are:\n" + usage)


def set_major_versions(major):
    for name, file in [["NSSUTIL_VMAJOR", nssutil_h],
                       ["SOFTOKEN_VMAJOR", softkver_h],
                       ["NSS_VMAJOR", nss_h]]:
        inplace_replace(filename=file, replacements=[
            Replacement(regex=r'^(#define *{} ?).*$'.format(name),
                        repl=r'\g<1>{}'.format(major))])


def set_minor_versions(minor):
    for name, file in [["NSSUTIL_VMINOR", nssutil_h],
                       ["SOFTOKEN_VMINOR", softkver_h],
                       ["NSS_VMINOR", nss_h]]:
        inplace_replace(filename=file, replacements=[
            Replacement(regex=r'^(#define *{} ?).*$'.format(name),
                        repl=r'\g<1>{}'.format(minor))])


def set_patch_versions(patch):
    for name, file in [["NSSUTIL_VPATCH", nssutil_h],
                       ["SOFTOKEN_VPATCH", softkver_h],
                       ["NSS_VPATCH", nss_h]]:
        inplace_replace(filename=file, replacements=[
            Replacement(regex=r'^(#define *{} ?).*$'.format(name),
                        repl=r'\g<1>{}'.format(patch))])


def set_build_versions(build):
    for name, file in [["NSSUTIL_VBUILD", nssutil_h],
                       ["SOFTOKEN_VBUILD", softkver_h],
                       ["NSS_VBUILD", nss_h]]:
        inplace_replace(filename=file, replacements=[
            Replacement(regex=r'^(#define *{} ?).*$'.format(name),
                        repl=r'\g<1>{}'.format(build))])


def set_full_lib_versions(version):
    for name, file in [["NSSUTIL_VERSION", nssutil_h],
                       ["SOFTOKEN_VERSION", softkver_h],
                       ["NSS_VERSION", nss_h]]:
        inplace_replace(filename=file, replacements=[
            Replacement(regex=r'^(#define *{} *\")([0-9.]+)(.*)$'.format(name),
                        repl=r'\g<1>{}\g<3>'.format(version))])


def set_root_ca_version():
    ensure_arguments_after_action(2, "major_version  minor_version")
    major = args[1].strip()
    minor = args[2].strip()
    version = major + '.' + minor

    inplace_replace(filename=nssckbi_h, replacements=[
        Replacement(regex=r'^(#define *NSS_BUILTINS_LIBRARY_VERSION *\").*$',
                    repl=r'\g<1>{}"'.format(version)),
        Replacement(regex=r'^(#define *NSS_BUILTINS_LIBRARY_VERSION_MAJOR ?).*$',
                    repl=r'\g<1>{}'.format(major)),
        Replacement(regex=r'^(#define *NSS_BUILTINS_LIBRARY_VERSION_MINOR ?).*$',
                    repl=r'\g<1>{}'.format(minor))])


def set_all_lib_versions(version, major, minor, patch, build):
    grep_major = check_output(['grep', 'define.*NSS_VMAJOR', nss_h])
    grep_minor = check_output(['grep', 'define.*NSS_VMINOR', nss_h])

    old_major = int(grep_major.split()[2])
    old_minor = int(grep_minor.split()[2])

    new_major = int(major)
    new_minor = int(minor)

    if (old_major < new_major or (old_major == new_major and old_minor < new_minor)):
        print("You're increasing the minor (or major) version:")
        print("- erasing ABI comparison expectations")
        new_branch = "NSS_" + str(old_major) + "_" + str(old_minor) + "_BRANCH"
        print("- setting reference branch to the branch of the previous version: " + new_branch)
        with open(abi_base_version_file, "w") as abi_base:
            abi_base.write("%s\n" % new_branch)
        for report_file in abi_report_files:
            with open(report_file, "w") as report_file_handle:
                report_file_handle.truncate()

    set_full_lib_versions(version)
    set_major_versions(major)
    set_minor_versions(minor)
    set_patch_versions(patch)
    set_build_versions(build)


def set_version_to_minor_release():
    ensure_arguments_after_action(2, "major_version  minor_version")
    major = args[1].strip()
    minor = args[2].strip()
    version = major + '.' + minor
    patch = "0"
    build = "0"
    set_all_lib_versions(version, major, minor, patch, build)


def set_version_to_patch_release():
    ensure_arguments_after_action(3, "major_version  minor_version  patch_release")
    major = args[1].strip()
    minor = args[2].strip()
    patch = args[3].strip()
    version = major + '.' + minor + '.' + patch
    build = "0"
    set_all_lib_versions(version, major, minor, patch, build)


def set_release_candidate_number():
    ensure_arguments_after_action(1, "release_candidate_number")
    build = args[1].strip()
    set_build_versions(build)


def set_4_digit_release_number():
    ensure_arguments_after_action(4, "major_version  minor_version  patch_release  4th_digit_release_number")
    major = args[1].strip()
    minor = args[2].strip()
    patch = args[3].strip()
    build = args[4].strip()
    version = major + '.' + minor + '.' + patch + '.' + build
    set_all_lib_versions(version, major, minor, patch, build)


def create_nss_release_archive():
    ensure_arguments_after_action(3, "nss_release_version  nss_hg_release_tag  path_to_stage_directory")
    nssrel = args[1].strip()  # e.g. 3.19.3
    nssreltag = args[2].strip()  # e.g. NSS_3_19_3_RTM
    stagedir = args[3].strip()  # e.g. ../stage

    with open('automation/release/nspr-version.txt') as nspr_version_file:
        nsprrel = next(nspr_version_file).strip()

    nspr_tar = "nspr-" + nsprrel + ".tar.gz"
    nsprtar_with_path = stagedir + "/v" + nsprrel + "/src/" + nspr_tar
    if (not os.path.exists(nsprtar_with_path)):
        exit_with_failure("cannot find nspr archive at expected location " + nsprtar_with_path)

    nss_stagedir = stagedir + "/" + nssreltag + "/src"
    if (os.path.exists(nss_stagedir)):
        exit_with_failure("nss stage directory already exists: " + nss_stagedir)

    nss_tar = "nss-" + nssrel + ".tar.gz"

    check_call_noisy(["mkdir", "-p", nss_stagedir])
    check_call_noisy(["hg", "archive", "-r", nssreltag, "--prefix=nss-" + nssrel + "/nss",
                      stagedir + "/" + nssreltag + "/src/" + nss_tar, "-X", ".hgtags"])
    check_call_noisy(["tar", "-xz", "-C", nss_stagedir, "-f", nsprtar_with_path])
    print("changing to directory " + nss_stagedir)
    os.chdir(nss_stagedir)
    check_call_noisy(["tar", "-xz", "-f", nss_tar])
    check_call_noisy(["mv", "-i", "nspr-" + nsprrel + "/nspr", "nss-" + nssrel + "/"])
    check_call_noisy(["rmdir", "nspr-" + nsprrel])

    nss_nspr_tar = "nss-" + nssrel + "-with-nspr-" + nsprrel + ".tar.gz"

    check_call_noisy(["tar", "-cz", "--remove-files", "-f", nss_nspr_tar, "nss-" + nssrel])
    check_call("sha1sum " + nss_tar + " " + nss_nspr_tar + " > SHA1SUMS", shell=True)
    check_call("sha256sum " + nss_tar + " " + nss_nspr_tar + " > SHA256SUMS", shell=True)
    print("created directory " + nss_stagedir + " with files:")
    check_call_noisy(["ls", "-l"])


o = OptionParser(usage="client.py [options] " + " | ".join([
    "remove_beta", "set_beta", "print_library_versions", "print_root_ca_version",
    "set_root_ca_version", "set_version_to_minor_release",
    "set_version_to_patch_release", "set_release_candidate_number",
    "set_4_digit_release_number", "create_nss_release_archive"]))

try:
    options, args = o.parse_args()
    action = args[0]
except IndexError:
    o.print_help()
    sys.exit(2)

if action in ('remove_beta'):
    remove_beta_status()

elif action in ('set_beta'):
    set_beta_status()

elif action in ('print_library_versions'):
    print_library_versions()

elif action in ('print_root_ca_version'):
    print_root_ca_version()

elif action in ('set_root_ca_version'):
    set_root_ca_version()

# x.y version number - 2 parameters
elif action in ('set_version_to_minor_release'):
    set_version_to_minor_release()

# x.y.z version number - 3 parameters
elif action in ('set_version_to_patch_release'):
    set_version_to_patch_release()

# change the release candidate number, usually increased by one,
# usually if previous release candiate had a bug
# 1 parameter
elif action in ('set_release_candidate_number'):
    set_release_candidate_number()

# use the build/release candiate number in the identifying version number
# 4 parameters
elif action in ('set_4_digit_release_number'):
    set_4_digit_release_number()

elif action in ('create_nss_release_archive'):
    create_nss_release_archive()

else:
    o.print_help()
    sys.exit(2)

sys.exit(0)
