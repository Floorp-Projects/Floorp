#! gmake
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

MODULE = calendar

LIBRARY_NAME = cal

DEPTH = ../../../..

EXPORTS = \
	htmlform/include/julianform.h \
	util/jdefines.h

CSRCS = \
	htmlform/src/julianform.c \
	$(NULL)

CPPSRCS = \
	htmlform/src/form.cpp	     \
	htmlform/src/formFactory.cpp \
	ical/attendee.cpp	     \
	ical/bprprty.cpp	     \
	ical/bydmgntr.cpp	     \
	ical/bydwgntr.cpp	     \
	ical/bydygntr.cpp	     \
	ical/byhgntr.cpp	     \
	ical/bymdgntr.cpp	     \
	ical/bymgntr.cpp	     \
	ical/bymogntr.cpp	     \
	ical/bywngntr.cpp	     \
	ical/byydgntr.cpp	     \
	ical/capiredr.cpp	     \
	ical/dategntr.cpp	     \
	ical/datetime.cpp	     \
	ical/deftgntr.cpp	     \
	ical/dprprty.cpp	     \
	ical/duprprty.cpp	     \
	ical/duration.cpp	     \
	ical/freebusy.cpp	     \
	ical/icalcomp.cpp	     \
	ical/icalfrdr.cpp	     \
	ical/icalprm.cpp	     \
	ical/icalredr.cpp	     \
	ical/icalsrdr.cpp	     \
	ical/icompfy.cpp	     \
	ical/iprprty.cpp	     \
	ical/jparser.cpp	     \
	ical/jutility.cpp	     \
	ical/keyword.cpp	     \
	ical/nscal.cpp		     \
	ical/orgnzr.cpp		     \
	ical/period.cpp		     \
	ical/pprprty.cpp	     \
	ical/prprty.cpp		     \
	ical/prprtyfy.cpp	     \
	ical/rcrrence.cpp	     \
	ical/recid.cpp		     \
	ical/rrday.cpp		     \
	ical/sdprprty.cpp	     \
	ical/sprpty.cpp		     \
	ical/tmbevent.cpp	     \
	ical/tzpart.cpp		     \
	ical/unistrto.cpp	     \
	ical/uri.cpp		     \
	ical/vevent.cpp		     \
	ical/vfrbsy.cpp		     \
	ical/vjournal.cpp            \
	ical/vtimezne.cpp	     \
	ical/vtodo.cpp               \
	util/jatom.cpp		     \
	util/jlog.cpp		     \
	util/julnstr.cpp	     \
	util/ptrarray.cpp	     \
	util/uidrgntr.cpp	     \
	itip/detxnobj.cpp            \
	itip/getxnobj.cpp            \
	itip/sttxnobj.cpp            \
	itip/txnobj.cpp		     \
	itip/txnobjfy.cpp	     \
	itip/user.cpp		     \
	$(NULL)

REQUIRES = calendar nspr nls libmime

include $(DEPTH)/config/rules.mk

DEFINES		+= -DNLS_DEFINE_STANDARD_TYPES=1 -DNLSAPI_PUBLIC_CLASS
#
NSTOM_DEPTH = $(NS_DEPTH)/../mozilla
INCLUDES    += -I./ical -I./itip -I./ui -I./util -I./htmlform/include \
			-I$(NSTOM_DEPTH)/dist/public/util \
			-I$(NSTOM_DEPTH)/dist/public/img \
			-I$(NSTOM_DEPTH)/dist/public/layer









