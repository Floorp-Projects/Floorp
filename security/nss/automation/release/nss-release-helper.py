#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import datetime
import shutil
import glob
from optparse import OptionParser
from subprocess import check_call

nssutil_h = "lib/util/nssutil.h"
softkver_h = "lib/softoken/softkver.h"
nss_h = "lib/nss/nss.h"
nssckbi_h = "lib/ckfw/builtins/nssckbi.h"

def check_call_noisy(cmd, *args, **kwargs):
    print "Executing command:", cmd
    check_call(cmd, *args, **kwargs)

o = OptionParser(usage="client.py [options] remove_beta | set_beta | print_library_versions | print_root_ca_version | set_root_ca_version | set_version_to_minor_release | set_version_to_patch_release | set_release_candidate_number | set_4_digit_release_number | create_nss_release_archive")

try:
    options, args = o.parse_args()
    action = args[0]
except IndexError:
    o.print_help()
    sys.exit(2)

def exit_with_failure(what):
    print "failure: ", what
    sys.exit(2)

def check_files_exist():
    if (not os.path.exists(nssutil_h) or not os.path.exists(softkver_h)
        or not os.path.exists(nss_h) or not os.path.exists(nssckbi_h)):
        exit_with_failure("cannot find expected header files, must run from inside NSS hg directory")

def sed_inplace(sed_expression, filename):
    backup_file = filename + '.tmp'
    check_call_noisy(["sed", "-i.tmp", sed_expression, filename])
    os.remove(backup_file)

def toggle_beta_status(is_beta):
    check_files_exist()
    if (is_beta):
        print "adding Beta status to version numbers"
        sed_inplace('s/^\(#define *NSSUTIL_VERSION *\"[0-9.]\+\)\" *$/\\1 Beta\"/', nssutil_h)
        sed_inplace('s/^\(#define *NSSUTIL_BETA *\)PR_FALSE *$/\\1PR_TRUE/', nssutil_h)
        sed_inplace('s/^\(#define *SOFTOKEN_VERSION *\"[0-9.]\+\" *SOFTOKEN_ECC_STRING\) *$/\\1 \" Beta"/', softkver_h)
        sed_inplace('s/^\(#define *SOFTOKEN_BETA *\)PR_FALSE *$/\\1PR_TRUE/', softkver_h)
        sed_inplace('s/^\(#define *NSS_VERSION *\"[0-9.]\+\" *_NSS_CUSTOMIZED\) *$/\\1 \" Beta"/', nss_h)
        sed_inplace('s/^\(#define *NSS_BETA *\)PR_FALSE *$/\\1PR_TRUE/', nss_h)
    else:
        print "removing Beta status from version numbers"
        sed_inplace('s/^\(#define *NSSUTIL_VERSION *\"[0-9.]\+\) *Beta\" *$/\\1\"/', nssutil_h)
        sed_inplace('s/^\(#define *NSSUTIL_BETA *\)PR_TRUE *$/\\1PR_FALSE/', nssutil_h)
        sed_inplace('s/^\(#define *SOFTOKEN_VERSION *\"[0-9.]\+\" *SOFTOKEN_ECC_STRING\) *\" *Beta\" *$/\\1/', softkver_h)
        sed_inplace('s/^\(#define *SOFTOKEN_BETA *\)PR_TRUE *$/\\1PR_FALSE/', softkver_h)
        sed_inplace('s/^\(#define *NSS_VERSION *\"[0-9.]\+\" *_NSS_CUSTOMIZED\) *\" *Beta\" *$/\\1/', nss_h)
        sed_inplace('s/^\(#define *NSS_BETA *\)PR_TRUE *$/\\1PR_FALSE/', nss_h)
    print "please run 'hg stat' and 'hg diff' to verify the files have been verified correctly"

def print_beta_versions():
    check_call_noisy(["egrep", "#define *NSSUTIL_VERSION|#define *NSSUTIL_BETA", nssutil_h])
    check_call_noisy(["egrep", "#define *SOFTOKEN_VERSION|#define *SOFTOKEN_BETA", softkver_h])
    check_call_noisy(["egrep", "#define *NSS_VERSION|#define *NSS_BETA", nss_h])

def remove_beta_status():
    print "--- removing beta flags. Existing versions were:"
    print_beta_versions()
    toggle_beta_status(False)
    print "--- finished modifications, new versions are:"
    print_beta_versions()

def set_beta_status():
    print "--- adding beta flags. Existing versions were:"
    print_beta_versions()
    toggle_beta_status(True)
    print "--- finished modifications, new versions are:"
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
    if (len(sys.argv) != (2+how_many)):
        exit_with_failure("incorrect number of arguments, expected parameters are:\n" + usage)

def set_major_versions(major):
    sed_inplace('s/^\(#define *NSSUTIL_VMAJOR *\).*$/\\1' + major + '/', nssutil_h)
    sed_inplace('s/^\(#define *SOFTOKEN_VMAJOR *\).*$/\\1' + major + '/', softkver_h)
    sed_inplace('s/^\(#define *NSS_VMAJOR *\).*$/\\1' + major + '/', nss_h)

def set_minor_versions(minor):
    sed_inplace('s/^\(#define *NSSUTIL_VMINOR *\).*$/\\1' + minor + '/', nssutil_h)
    sed_inplace('s/^\(#define *SOFTOKEN_VMINOR *\).*$/\\1' + minor + '/', softkver_h)
    sed_inplace('s/^\(#define *NSS_VMINOR *\).*$/\\1' + minor + '/', nss_h)

def set_patch_versions(patch):
    sed_inplace('s/^\(#define *NSSUTIL_VPATCH *\).*$/\\1' + patch + '/', nssutil_h)
    sed_inplace('s/^\(#define *SOFTOKEN_VPATCH *\).*$/\\1' + patch + '/', softkver_h)
    sed_inplace('s/^\(#define *NSS_VPATCH *\).*$/\\1' + patch + '/', nss_h)

def set_build_versions(build):
    sed_inplace('s/^\(#define *NSSUTIL_VBUILD *\).*$/\\1' + build + '/', nssutil_h)
    sed_inplace('s/^\(#define *SOFTOKEN_VBUILD *\).*$/\\1' + build + '/', softkver_h)
    sed_inplace('s/^\(#define *NSS_VBUILD *\).*$/\\1' + build + '/', nss_h)

def set_full_lib_versions(version):
    sed_inplace('s/^\(#define *NSSUTIL_VERSION *\"\)\([0-9.]\+\)\(.*\)$/\\1' + version + '\\3/', nssutil_h)
    sed_inplace('s/^\(#define *SOFTOKEN_VERSION *\"\)\([0-9.]\+\)\(.*\)$/\\1' + version + '\\3/', softkver_h)
    sed_inplace('s/^\(#define *NSS_VERSION *\"\)\([0-9.]\+\)\(.*\)$/\\1' + version + '\\3/', nss_h)

def set_root_ca_version():
    ensure_arguments_after_action(2, "major_version  minor_version")
    major = args[1].strip()
    minor = args[2].strip()
    version = major + '.' + minor
    sed_inplace('s/^\(#define *NSS_BUILTINS_LIBRARY_VERSION *\"\).*$/\\1' + version + '/', nssckbi_h)
    sed_inplace('s/^\(#define *NSS_BUILTINS_LIBRARY_VERSION_MAJOR *\).*$/\\1' + major + '/', nssckbi_h)
    sed_inplace('s/^\(#define *NSS_BUILTINS_LIBRARY_VERSION_MINOR *\).*$/\\1' + minor + '/', nssckbi_h)

def set_all_lib_versions(version, major, minor, patch, build):
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
    ensure_arguments_after_action(4, "nss_release_version  nss_hg_release_tag  nspr_release_version  path_to_stage_directory")
    nssrel = args[1].strip() #e.g. 3.19.3
    nssreltag = args[2].strip() #e.g. NSS_3_19_3_RTM
    nsprrel = args[3].strip() #e.g. 4.10.8
    stagedir = args[4].strip() #e.g. ../stage

    nspr_tar = "nspr-" + nsprrel + ".tar.gz"
    nsprtar_with_path= stagedir + "/v" + nsprrel + "/src/" + nspr_tar
    if (not os.path.exists(nsprtar_with_path)):
        exit_with_failure("cannot find nspr archive at expected location " + nsprtar_with_path)

    nss_stagedir= stagedir + "/" + nssreltag + "/src"
    if (os.path.exists(nss_stagedir)):
        exit_with_failure("nss stage directory already exists: " + nss_stagedir)

    nss_tar = "nss-" + nssrel + ".tar.gz"

    check_call_noisy(["mkdir", "-p", nss_stagedir])
    check_call_noisy(["hg", "archive", "-r", nssreltag, "--prefix=nss-" + nssrel + "/nss",
                      stagedir + "/" + nssreltag + "/src/" + nss_tar, "-X", ".hgtags"])
    check_call_noisy(["tar", "-xz", "-C", nss_stagedir, "-f", nsprtar_with_path])
    print "changing to directory " + nss_stagedir
    os.chdir(nss_stagedir)
    check_call_noisy(["tar", "-xz", "-f", nss_tar])
    check_call_noisy(["mv", "-i", "nspr-" + nsprrel + "/nspr", "nss-" + nssrel + "/"])
    check_call_noisy(["rmdir", "nspr-" + nsprrel])

    nss_nspr_tar = "nss-" + nssrel + "-with-nspr-" + nsprrel + ".tar.gz"

    check_call_noisy(["tar", "-cz", "--remove-files", "-f", nss_nspr_tar, "nss-" + nssrel])
    check_call("sha1sum " + nss_tar + " " + nss_nspr_tar + " > SHA1SUMS", shell=True)
    check_call("sha256sum " + nss_tar + " " + nss_nspr_tar + " > SHA256SUMS", shell=True)
    print "created directory " + nss_stagedir + " with files:"
    check_call_noisy(["ls", "-l"])

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
