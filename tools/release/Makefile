test:
	for f in release t/test.pl `find . -name "*.pm"`; do perl -c $$f; done
	if [ -f t/test.log ]; then rm t/test.log; fi
	if [ ! -f bootstrap.cfg ]; then cp bootstrap.cfg.example bootstrap.cfg; fi
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
	mkdir -p /home/ftp/pub/firefox/releases/1.5/
	touch /home/ftp/pub/firefox/releases/1.5/KEY
	# ftp
	mkdir -p /home/ftp/pub/firefox/nightly
	chown -R cltbld:firefox /home/ftp/pub/firefox
	chmod -R g+rwxs /home/ftp/pub/firefox
	chmod -R o+rx /home/ftp/pub
	# staging environment
	mkdir -p /data/cltbld/firefox-2.0.0.4/batch1/stage
	# download 2.0.0.6, for l10n verify
	cd /data && wget -nv --cut-dirs=3 -np -r http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/2.0.0.6-candidates/rc2/ && mv ftp.mozilla.org/nightly/* /home/ftp/pub/firefox/nightly/ && rm -rfv ftp.mozilla.org

cvsmirror: cvsmirror_mofo cvsmirror_main

cvsmirror_main:
	rsync -a --delete-after --exclude=CVSROOT/config --exclude=CVSROOT/loginfo cvs-mirror.mozilla.org::mozilla/ /builds/cvsmirror.clean/cvsroot/
	rsync -a --delete-after cvs-mirror.mozilla.org::l10n/ /builds/cvsmirror.clean/l10n/
	chgrp -R cvs /builds/cvsmirror.clean/cvsroot /builds/cvsmirror.clean/l10n
	chmod -R g+rw /builds/cvsmirror.clean/cvsroot /builds/cvsmirror.clean/l10n
	cvs -d /builds/cvsmirror.clean/cvsroot rtag -d FIREFOX_2_0_0_7_RELEASE mozilla
	cvs -d /builds/cvsmirror.clean/cvsroot rtag -d FIREFOX_2_0_0_7_RELEASE_l10n mozilla
	cvs -d /builds/cvsmirror.clean/cvsroot rtag -d FIREFOX_2_0_0_7_RC1 mozilla
	cvs -d /builds/cvsmirror.clean/cvsroot rtag -d FIREFOX_2_0_0_7_RC1_l10n mozilla
	cvs -d /builds/cvsmirror.clean/cvsroot rtag -d FIREFOX_2_0_0_7_RC2 mozilla
	cvs -d /builds/cvsmirror.clean/cvsroot rtag -d FIREFOX_2_0_0_7_RC2_l10n mozilla
	cvs -d /builds/cvsmirror.clean/cvsroot rtag -d -B FIREFOX_2_0_0_7_MINIBRANCH mozilla
	cvs -d /builds/cvsmirror.clean/l10n rtag -d FIREFOX_2_0_0_7_RELEASE l10n
	cvs -d /builds/cvsmirror.clean/l10n rtag -d FIREFOX_2_0_0_7_RC1 l10n

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
	rm -rf /builds/release/logs/*
	rm -rf /builds/updates/*
	rm -rf /builds/verify/*
	rm -rf /data/cltbld/firefox-*/
	rm -rf /home/ftp/pub/firefox/*

clean_cvsmirror: clean_cvsmirror_main clean_cvsmirror_mofo
	rm -rf /builds/cvsmirror.clean/*

clean_cvsmirror_main:
	rm -rf /builds/cvsmirror.clean/cvsroot/*
	rm -rf /builds/cvsmirror.clean/l10n/*

clean_cvsmirror_mofo:
	rm -rf /builds/cvsmirror.clean/mofo/*

clean_logs:
	rm -rf /builds/logs/*

