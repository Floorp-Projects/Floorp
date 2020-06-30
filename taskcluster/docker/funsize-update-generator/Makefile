IMAGE_NAME = funsize-update-generator

build:
	docker build -t $(IMAGE_NAME) --no-cache --rm .

update_pubkeys:
	curl https://hg.mozilla.org/mozilla-central/raw-file/default/toolkit/mozapps/update/updater/nightly_aurora_level3_primary.der | openssl x509 -inform DER -pubkey -noout > nightly.pubkey
	curl https://hg.mozilla.org/mozilla-central/raw-file/default/toolkit/mozapps/update/updater/dep1.der | openssl x509 -inform DER -pubkey -noout > dep.pubkey
	curl https://hg.mozilla.org/mozilla-central/raw-file/default/toolkit/mozapps/update/updater/release_primary.der | openssl x509 -inform DER -pubkey -noout > release.pubkey
