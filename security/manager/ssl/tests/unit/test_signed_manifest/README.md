This folder contains the scripts needed to generate signed manifest files
to verify the Trusted Hosted Apps concept.

Prerequisites:

* NSS 3.4 or higher.
* Python 2.7 (should work with 2.6 also)
* Bash
* OpenSSL

Usage:

Run
  I) For usage info execute ./create_test_files.sh --help

 II) Upload the signed manifest.webapp and manifest.sig to the
     application hosting server.
