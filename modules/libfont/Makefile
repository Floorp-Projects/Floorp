#!gmake
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.



DEPTH	= ../..
DIRS	= classes public src producers

include $(DEPTH)/config/rules.mk

#
# Update jmcgen.
#
# WARNING: make sure that the following directories are built:
#		classes public src producers sdkstubs
update-jmcgen:
	cp src/_jmc/*.c jmcgen/src/.
	cp src/_jmc/*.h jmcgen/include/.
	cp public/_jmc/*.c jmcgen/src/.
	cp public/_jmc/*.h jmcgen/include/.
	cp sdkstubs/_jmc/*.c jmcgen/src/.
	cp sdkstubs/_jmc/*.h jmcgen/include/.
