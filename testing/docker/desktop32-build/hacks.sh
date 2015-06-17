#!/usr/bin/env bash
##
# Install compiler hacks, necessary for builds to work properly,
# but ultimately to be removed in favor of something cleaner
##

ln -s /usr/include/linux/a.out.h /usr/include/a.out.h
