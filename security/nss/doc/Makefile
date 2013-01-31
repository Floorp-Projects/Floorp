#! gmake
#
# Creates man pages for the NSS security tools
#
# pk12util, certutil, modutil, ssltap, 
# signtool, signver, cmsutil, crlutil,  
# derdump, pp, vfychain, vfyserv
#

.SUFFIXES: .html .txt .1 .xml

COMPILE.1 = xmlto -o nroff man
COMPILE.html = xmlto -o html html

# the name of the tar ball
name = nss-man
date = `date +"%Y%m%d"`

all: prepare all-man all-html

prepare: date-and-version
	mkdir -p html
	mkdir -p nroff
	
clean:
	rm -f date.xml version.xml *.tar.bz2
	rm -fr $(name) ascii html nroff

date-and-version: date.xml version.xml

date.xml:
	date +"%e %B %Y" | tr -d '\n' > $@

version.xml:
	echo -n ${VERSION} > $@

.PHONY : $(MANPAGES)
.PHONY : $(HTMLPAGES)
.PHONY : $(TXTPAGES)

#------------------------------------------
# Package a tar ball for building in fedora
# Include the makefile and .xml files only
# man pages will be created at build time
#------------------------------------------

tarball:
	rm -rf $(name); \
	mkdir -p $(name)/nroff; \
	cp Makefile $(name); \
	cp *.xml $(name); \
	tar cvjf $(name)-$(date).tar.bz2 $(name)

#--------------------------------------------------------
# manpages
#--------------------------------------------------------

%.1 : %.xml prepare
	$(COMPILE.1) $<
	
MANPAGES = \
certutil.1 cmsutil.1 crlutil.1 pk12util.1 \
modutil.1 ssltap.1 derdump.1 signtool.1 signver.1 \
pp.1 vfychain.1 vfyserv.1

all-man: prepare $(MANPAGES)

#--------------------------------------------------------
# html pages
#--------------------------------------------------------

%.html : %.xml
	$(COMPILE.html) $<
	mv html/index.html html/$@

HTMLPAGES = \
certutil.html cmsutil.html crlutil.html pk12util.html  modutil.html \
ssltap.html derdump.html signtool.html signver.html pp.html \
vfychain.html vfyserv.html

all-html: prepare $(HTMLPAGES)

