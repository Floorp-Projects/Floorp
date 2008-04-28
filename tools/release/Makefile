PRODUCT = $(shell grep "^product[ \t]" bootstrap.cfg | sed -e 's/[^=]*= *//')
VERSION = $(shell grep "^version[ \t]" bootstrap.cfg | sed -e 's/[^=]*= *//')
OLD_VERSION = $(shell grep "^oldVersion[ \t]" bootstrap.cfg | sed -e 's/[^=]*= *//')
OLD_BUILD = $(shell grep "^oldBuild[ \t]" bootstrap.cfg | sed -e 's/[^=]*= *//')
PRODUCT_TAG = $(shell grep "^productTag[ \t]" bootstrap.cfg | sed -e 's/[^=]*= *//')
# extract the numeric build value from the config file
PRODUCT_BUILD = $(shell grep "^build[ \t]" bootstrap.cfg | sed -e 's/[^0-9]*\([0-9]*\).*/\1/')

test:
	for f in release t/test.pl `find . -name "*.pm"`; do \
	perl -c $$f || exit $?; done
	if [ -f t/test.log ]; then rm t/test.log; fi
	if [ ! -f bootstrap.cfg ]; then cp bootstrap.cfg.example bootstrap.cfg; fi
	cp t/tinder-config.pl.orig t/tinder-config.pl
	./t/test.pl

stage:
	# basic environment
	mkdir -p /builds/config
	mkdir -p /builds/tags
	mkdir -p /builds/updates/
	mkdir -p /builds/verify/
	mkdir -p /builds/logs/
	mkdir -p /data/symbols/
	# fake key
	mkdir -p /home/ftp/pub/${PRODUCT}/releases/1.5/
	touch /home/ftp/pub/${PRODUCT}/releases/1.5/KEY
	# ftp
	mkdir -p /home/ftp/pub/${PRODUCT}/nightly
	chown -R cltbld:${PRODUCT} /home/ftp/pub/${PRODUCT}
	chmod -R g+rwx /home/ftp/pub/${PRODUCT}
	chmod -R o+rx /home/ftp/pub
	# staging environment
	mkdir -p /data/cltbld/${PRODUCT}-${VERSION}/batch1/stage
	# download old builds, for l10n verify
	cd /data && wget -nv --cut-dirs=3 -np -r ftp://ftp.mozilla.org/pub/mozilla.org/${PRODUCT}/nightly/${OLD_VERSION}-candidates/build${OLD_BUILD}/* && mv ftp.mozilla.org/nightly/* /home/ftp/pub/${PRODUCT}/nightly/ && rm -rfv ftp.mozilla.org
	# download old release, for update verify
	cd /data && wget -nv --cut-dirs=3 -np -r -e robots=off http://stage.mozilla.org/pub/mozilla.org/${PRODUCT}/releases/${OLD_VERSION}/ && mv stage.mozilla.org/releases/* /home/ftp/pub/${PRODUCT}/releases/ && rm -rfv stage.mozilla.org


cvsmirror: cvsmirror_mofo cvsmirror_main

cvsmirror_main:
	rsync -a --delete-after --exclude=CVSROOT/config --exclude=CVSROOT/loginfo cvs-mirror.mozilla.org::mozilla/ /builds/cvsmirror.clean/cvsroot/
	rsync -a --delete-after cvs-mirror.mozilla.org::l10n/ /builds/cvsmirror.clean/l10n/
	chgrp -R cvs /builds/cvsmirror.clean/cvsroot /builds/cvsmirror.clean/l10n
	chmod -R g+rw /builds/cvsmirror.clean/cvsroot /builds/cvsmirror.clean/l10n
	cvs -d /builds/cvsmirror.clean/cvsroot rtag -d ${PRODUCT_TAG}_RELEASE mozilla
	cvs -d /builds/cvsmirror.clean/cvsroot rtag -d ${PRODUCT_TAG}_RELEASE_l10n mozilla/tools/tinderbox-configs/
	cvs -d /builds/cvsmirror.clean/l10n rtag -d ${PRODUCT_TAG}_RELEASE l10n
	for ((build=1; build <= ${PRODUCT_BUILD}; build++)); do \
		cvs -d /builds/cvsmirror.clean/cvsroot rtag -d ${PRODUCT_TAG}_BUILD$$build mozilla;\
		cvs -d /builds/cvsmirror.clean/cvsroot rtag -d ${PRODUCT_TAG}_BUILD$$build_l10n mozilla/tools/tinderbox-configs/;\
		cvs -d /builds/cvsmirror.clean/l10n rtag -d ${PRODUCT_TAG}_BUILD$$build l10n;\
	done

cvsmirror_mofo:
	mkdir -p /builds/cvsmirror.clean/tmp/mofo
	mkdir -p /builds/cvsmirror.clean/mofo
	cvs -d /builds/cvsmirror.clean/mofo init
	cd /builds/cvsmirror.clean/tmp/mofo && cvs -d cltbld@cvs.mozilla.org:/mofo export -r MOZILLA_1_8_BRANCH talkback
	cd /builds/cvsmirror.clean/tmp/mofo && cvs -d cltbld@cvs.mozilla.org:/mofo export -r HEAD release
	cd /builds/cvsmirror.clean/tmp/mofo/release && cvs -d /builds/cvsmirror.clean/mofo import -d -m "import CVS snapshot" release MOZILLA_1_8_BRANCH_release R0_1
	cd /builds/cvsmirror.clean/tmp/mofo/talkback && cvs -d /builds/cvsmirror.clean/mofo import -d -m "import CVS snapshot" talkback MOZILLA_1_8_BRANCH R0_1
	rm -rf /builds/cvsmirror.clean/tmp/mofo
	chgrp -R cvs /builds/cvsmirror.clean/mofo
	chmod g+rwx /builds/cvsmirror.clean/mofo
	chmod -R g+rw /builds/cvsmirror.clean/mofo

clean_stage: clean_logs
	rm -rf /builds/config/*
	rm -rf /builds/tags/*
	rm -rf /builds/source/*
	rm -rf /builds/release/logs/*
	rm -rf /builds/updates/*
	rm -rf /builds/verify/*
	rm -rf /builds/symbols/*
	rm -rf /data/cltbld/${PRODUCT}-*/
	rm -rf /data/symbols/*
	rm -rf /home/ftp/pub/${PRODUCT}/*

clean_cvsmirror: clean_cvsmirror_main clean_cvsmirror_mofo
	rm -rf /builds/cvsmirror.clean/*

clean_cvsmirror_main:
	rm -rf /builds/cvsmirror.clean/cvsroot/*
	rm -rf /builds/cvsmirror.clean/l10n/*

clean_cvsmirror_mofo:
	rm -rf /builds/cvsmirror.clean/mofo/*

clean_logs:
	rm -rf /builds/logs/*

