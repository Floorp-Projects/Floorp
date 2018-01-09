FROM python:2-slim-stretch

MAINTAINER release@mozilla.com

RUN dpkg --add-architecture i386 && apt-get -q update \
    # p7zip-full is for extracting Windows and OS X packages
    # Mercurial is for cloning required repositories
    # wget is for downloading update.xml, installers, and MARs
    # libgtk-3-0 and libgtk2.0-0 are required to run the Firefox updater
    && apt-get -q --yes install p7zip-full mercurial wget libgtk-3-0 libgtk-3.0:i386 libgtk2.0-0 libgtk2.0-0:i386 \
    && apt-get clean
