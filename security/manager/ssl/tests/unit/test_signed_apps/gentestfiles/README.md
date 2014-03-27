This file contains the scripts and binary files needed to regenerate the signed
files used on the signed apps test.

Prerequisites:

* NSS 3.4 or higher.
* Python 2.7 (should work with 2.6 also)
* Bash

Usage:

Run

./create_test_files.sh

The new test files will be created at the ./testApps directory. Just copy the
contents of this directory to the test directory (dom/apps/tests/signed and
security/manager/ssl/tests/unit) to use the new files.
