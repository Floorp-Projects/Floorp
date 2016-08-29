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

prinit_h = "pr/include/prinit.h"
f_conf = "configure"
f_conf_in = "configure.in"

def check_call_noisy(cmd, *args, **kwargs):
    print "Executing command:", cmd
    check_call(cmd, *args, **kwargs)

o = OptionParser(usage="client.py [options] remove_beta | set_beta | print_library_versions | set_version_to_minor_release | set_version_to_patch_release | create_nspr_release_archive")

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
    if (not os.path.exists(prinit_h)):
        exit_with_failure("cannot find expected header files, must run from inside NSPR hg directory")

def sed_inplace(sed_expression, filename):
    backup_file = filename + '.tmp'
    check_call_noisy(["sed", "-i.tmp", sed_expression, filename])
    os.remove(backup_file)

def toggle_beta_status(is_beta):
    check_files_exist()
    if (is_beta):
        print "adding Beta status to version numbers"
        sed_inplace('s/^\(#define *PR_VERSION *\"[0-9.]\+\)\" *$/\\1 Beta\"/', prinit_h)
        sed_inplace('s/^\(#define *PR_BETA *\)PR_FALSE *$/\\1PR_TRUE/', prinit_h)

    else:
        print "removing Beta status from version numbers"
        sed_inplace('s/^\(#define *PR_VERSION *\"[0-9.]\+\) *Beta\" *$/\\1\"/', prinit_h)
        sed_inplace('s/^\(#define *PR_BETA *\)PR_TRUE *$/\\1PR_FALSE/', prinit_h)
    print "please run 'hg stat' and 'hg diff' to verify the files have been verified correctly"

def print_beta_versions():
    check_call_noisy(["egrep", "#define *PR_VERSION|#define *PR_BETA", prinit_h])

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
    check_call_noisy(["egrep", "#define *PR_VERSION|#define PR_VMAJOR|#define *PR_VMINOR|#define *PR_VPATCH|#define *PR_BETA", prinit_h])

def ensure_arguments_after_action(how_many, usage):
    if (len(sys.argv) != (2+how_many)):
        exit_with_failure("incorrect number of arguments, expected parameters are:\n" + usage)

def set_major_versions(major):
    sed_inplace('s/^\(#define *PR_VMAJOR *\).*$/\\1' + major + '/', prinit_h)
    sed_inplace('s/^MOD_MAJOR_VERSION=.*$/MOD_MAJOR_VERSION=' + major + '/', f_conf)
    sed_inplace('s/^MOD_MAJOR_VERSION=.*$/MOD_MAJOR_VERSION=' + major + '/', f_conf_in)

def set_minor_versions(minor):
    sed_inplace('s/^\(#define *PR_VMINOR *\).*$/\\1' + minor + '/', prinit_h)
    sed_inplace('s/^MOD_MINOR_VERSION=.*$/MOD_MINOR_VERSION=' + minor + '/', f_conf)
    sed_inplace('s/^MOD_MINOR_VERSION=.*$/MOD_MINOR_VERSION=' + minor + '/', f_conf_in)

def set_patch_versions(patch):
    sed_inplace('s/^\(#define *PR_VPATCH *\).*$/\\1' + patch + '/', prinit_h)
    sed_inplace('s/^MOD_PATCH_VERSION=.*$/MOD_PATCH_VERSION=' + patch + '/', f_conf)
    sed_inplace('s/^MOD_PATCH_VERSION=.*$/MOD_PATCH_VERSION=' + patch + '/', f_conf_in)

def set_full_lib_versions(version):
    sed_inplace('s/^\(#define *PR_VERSION *\"\)\([0-9.]\+\)\(.*\)$/\\1' + version + '\\3/', prinit_h)

def set_all_lib_versions(version, major, minor, patch):
    set_full_lib_versions(version)
    set_major_versions(major)
    set_minor_versions(minor)
    set_patch_versions(patch)
    print
    print "==========================="
    print "======== ATTENTION ========"
    print
    print "You *MUST* manually edit file pr/tests/vercheck.c"
    print
    print "Edit two arrays, named compatible_version and incompatible_version"
    print "according to the new version you're adding."
    print
    print "======== ATTENTION ========"
    print "==========================="

def set_version_to_minor_release():
    ensure_arguments_after_action(2, "major_version  minor_version")
    major = args[1].strip()
    minor = args[2].strip()
    version = major + '.' + minor
    patch = "0"
    set_all_lib_versions(version, major, minor, patch)

def set_version_to_patch_release():
    ensure_arguments_after_action(3, "major_version  minor_version  patch_release")
    major = args[1].strip()
    minor = args[2].strip()
    patch = args[3].strip()
    version = major + '.' + minor + '.' + patch
    set_all_lib_versions(version, major, minor, patch)

def create_nspr_release_archive():
    ensure_arguments_after_action(2, "nspr_release_version  nspr_hg_release_tag")
    nsprrel = args[1].strip() #e.g. 4.10.9
    nsprreltag = args[2].strip() #e.g. NSPR_4_10_9_RTM

    nspr_tar = "nspr-" + nsprrel + ".tar.gz"
    nspr_stagedir="../stage/v" + nsprrel + "/src"
    if (os.path.exists(nspr_stagedir)):
        exit_with_failure("nspr stage directory already exists: " + nspr_stagedir)

    check_call_noisy(["mkdir", "-p", nspr_stagedir])
    check_call_noisy(["hg", "archive", "-r", nsprreltag, "--prefix=nspr-" + nsprrel + "/nspr",
                      "../stage/v" + nsprrel + "/src/" + nspr_tar, "-X", ".hgtags"])
    print "changing to directory " + nspr_stagedir
    os.chdir(nspr_stagedir)

    check_call("sha1sum " + nspr_tar + " > SHA1SUMS", shell=True)
    check_call("sha256sum " + nspr_tar + " > SHA256SUMS", shell=True)
    print "created directory " + nspr_stagedir + " with files:"
    check_call_noisy(["ls", "-l"])

if action in ('remove_beta'):
    remove_beta_status()

elif action in ('set_beta'):
    set_beta_status()

elif action in ('print_library_versions'):
    print_library_versions()

# x.y version number - 2 parameters
elif action in ('set_version_to_minor_release'):
    set_version_to_minor_release()

# x.y.z version number - 3 parameters
elif action in ('set_version_to_patch_release'):
    set_version_to_patch_release()

elif action in ('create_nspr_release_archive'):
    create_nspr_release_archive()

else:
    o.print_help()
    sys.exit(2)

sys.exit(0)
