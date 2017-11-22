## This folder holds everything to generate certificates and keys for tests

Scripts in this folder require a copy of [mozilla-central](https://hg.mozilla.org/mozilla-central/) and use scripts from `security/manager/ssl/tests/unit`. The default path for `MOZILLA_CENTRAL` is next to the root of this project.

The helper script `certs.sh` sets all necessary paths and generates certificates.
The following command generates the end-entity certificate with P256, ECDSA, SHA256.

    MOZILLA_CENTRAL=<path to mc> ./certs.sh < ee-p256.certspec

