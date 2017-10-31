FROM python:2-slim-stretch

MAINTAINER release@mozilla.com

RUN apt-get -q update \
    # p7zip-full is for extracting Windows and OS X packages
    # Mercurial and Git are for cloning required repositories
    # wget is for downloading update.xml, installers, and MARs
    # libgtk-3-0 is required to run the Firefox updater
    && apt-get -q --yes install p7zip-full git mercurial wget libgtk-3-0 \
    && apt-get clean
