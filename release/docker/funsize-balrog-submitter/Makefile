DOCKERIO_USERNAME =$(error DOCKERIO_USERNAME should be set)
IMAGE_NAME = funsize-balrog-submitter
FULL_IMAGE_NAME = $(DOCKERIO_USERNAME)/$(IMAGE_NAME)

build:
	docker build -t $(FULL_IMAGE_NAME) --no-cache --rm .

push:
	docker push $(FULL_IMAGE_NAME):latest

pull:
	docker pull $(FULL_IMAGE_NAME):latest

update_pubkeys:
	curl https://hg.mozilla.org/mozilla-central/raw-file/default/toolkit/mozapps/update/updater/nightly_aurora_level3_primary.der | openssl x509 -inform DER -pubkey -noout > nightly.pubkey
	curl https://hg.mozilla.org/mozilla-central/raw-file/default/toolkit/mozapps/update/updater/dep1.der | openssl x509 -inform DER -pubkey -noout > dep.pubkey
	curl https://hg.mozilla.org/mozilla-central/raw-file/default/toolkit/mozapps/update/updater/release_primary.der | openssl x509 -inform DER -pubkey -noout > release.pubkey
