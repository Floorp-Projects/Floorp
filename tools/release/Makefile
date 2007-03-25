test:
	for f in release t/test.pl `find . -name "*.pm"`; do perl -c $$f; done
	if [ -f t/test.log ]; then rm t/test.log; fi
	if [ ! -f bootstrap.cfg ]; then cp bootstrap.cfg.example bootstrap.cfg; fi
	./t/test.pl

stage:
	# basic environment
	mkdir -p /builds/tags
	mkdir -p /builds/updates/
	mkdir -p /builds/verify/
	# ftp
	mkdir -p /home/ftp/pub/firefox/nightly
	# fake key
	mkdir -p /home/ftp/pub/firefox/releases/1.5/
	touch /home/ftp/pub/firefox/releases/1.5/KEY
	# staging environment
	mkdir -p /data/cltbld/firefox-1.5.0.11/batch1/stage

cvsmirror: cvsmirror_mofo cvsmirror_main

cvsmirror_main:
	rsync -a --delete-after --exclude=CVSROOT/config --exclude=CVSROOT/loginfo cvs-mirror.mozilla.org::mozilla/ /builds/cvsmirror/cvsroot/
	rsync -a --delete-after cvs-mirror.mozilla.org::l10n/ /builds/cvsmirror/l10n/
	chgrp -R cvs /builds/cvsmirror/cvsroot /builds/cvsmirror/l10n
	chmod -R g+rw /builds/cvsmirror/cvsroot /builds/cvsmirror/l10n
	cvs -d /builds/cvsmirror/cvsroot rtag -d FIREFOX_1_5_0_11_RELEASE mozilla
	cvs -d /builds/cvsmirror/cvsroot rtag -d FIREFOX_1_5_0_11_RC1 mozilla
	cvs -d /builds/cvsmirror/cvsroot rtag -d -B FIREFOX_1_5_0_11_MINIBRANCH mozilla
	cvs -d /builds/cvsmirror/l10n rtag -d FIREFOX_1_5_0_11_RELEASE l10n
	cvs -d /builds/cvsmirror/l10n rtag -d FIREFOX_1_5_0_11_RC1 l10n

cvsmirror_mofo:
	mkdir -p /builds/cvsmirror/tmp/mofo
	mkdir -p /builds/cvsmirror/mofo
	cvs -d /builds/cvsmirror/mofo init
	cd /builds/cvsmirror/tmp/mofo && cvs -d cltbld@cvs.mozilla.org:/mofo export -r MOZILLA_1_8_0_BRANCH talkback
	cd /builds/cvsmirror/tmp/mofo && cvs -d cltbld@cvs.mozilla.org:/mofo export -r HEAD release
	rm -rf /builds/cvsmirror/tmp/mofo/release/tinderbox-configs/
	cd /builds/cvsmirror/tmp/mofo && cvs -d cltbld@cvs.mozilla.org:/mofo export -r MOZILLA_1_8_0_BRANCH_release release/tinderbox-configs/
	cd /builds/cvsmirror/tmp/mofo/release && cvs -d /builds/cvsmirror/mofo import -d -m "import CVS snapshot" release MOZILLA_1_8_0_BRANCH_release R0_1
	cd /builds/cvsmirror/tmp/mofo/talkback && cvs -d /builds/cvsmirror/mofo import -d -m "import CVS snapshot" talkback MOZILLA_1_8_0_BRANCH R0_1
	rm -rf /builds/cvsmirror/tmp/mofo
	chgrp -R cvs /builds/cvsmirror/mofo
	chmod g+rwx /builds/cvsmirror/mofo
	chmod -R g+rw /builds/cvsmirror/mofo

clean_stage:
	rm -rf /builds/tags/*
	rm -rf /builds/release/logs/*
	rm -rf /builds/updates/*
	rm -rf /builds/verify/*
	rm -rf /data/cltbld/firefox-*/
	rm -rf /home/ftp/pub/firefox/*

clean_cvsmirror: clean_cvsmirror_main clean_cvsmirror_mofo
	rm -rf /builds/cvsmirror/*

clean_cvsmirror_main:
	rm -rf /builds/cvsmirror/cvsroot/*
	rm -rf /builds/cvsmirror/l10n/*

clean_cvsmirror_mofo:
	rm -rf /builds/cvsmirror/mofo/*
