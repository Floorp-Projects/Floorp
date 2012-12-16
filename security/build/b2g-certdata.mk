# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# On B2G, we need to remove the trust bits for code signing from all the
# built-in CAs, because we are redefining the code signing bit to mean
# "is trusted to issue certs that are trusted for signing apps," which none
# of the normal built-in CAs are. This is a temporary hack until we can use
# libpkix to verify the certificates. (libpkix gives the flexibility we need
# to verify certificates using different sets of trust anchors per validation.)
#
# Whenever we change the B2G app signing trust anchor, we need to manually
# update certdata-b2g.txt. To do so:
#
# 1. replace ./b2g-app-root-cert.der with the new DER-encoded root cert
#
# 2. In this directory run:
#
#     PATH=$NSS/bin:$NSS/lib addbuiltin -n "b2g-app-root-cert" -t ",,Cu" \
#       < b2g-app-root-cert.der > b2g-certdata.txt
#
# Then, commit the changes. We don't do this step as part of the build because
# we do not build addbuiltin as part of a Gecko build.

# Distrust all existing builtin CAs for code-signing
hacked-certdata.txt : $(srcdir)/../nss/lib/ckfw/builtins/certdata.txt
	sed -e "s/^CKA_TRUST_CODE_SIGNING.*CKT_NSS_TRUSTED_DELEGATOR.*/CKA_TRUST_CODE_SIGNING CK_TRUST CKT_NSS_MUST_VERIFY_TRUST/" \
			$< > $@

combined-certdata.txt : hacked-certdata.txt $(srcdir)/b2g-certdata.txt
	cat $^ > $@

libs:: combined-certdata.txt

DEFAULT_GMAKE_FLAGS += NSS_CERTDATA_TXT='$(CURDIR)/combined-certdata.txt'
