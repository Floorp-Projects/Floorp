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

#
# This rule 'imports' the msgsdk headers and libs for our use
#



export::
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/include/*.h $(SOURCE_XP_DIR)/public/msgsdk/
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/protocol/Smtp/include/*.h $(SOURCE_XP_DIR)/public/msgsdk/
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/protocol/Imap4/include/*.h $(SOURCE_XP_DIR)/public/msgsdk/
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/protocol/Mime/include/*.h $(SOURCE_XP_DIR)/public/msgsdk/
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/protocol/Pop3/include/*.h $(SOURCE_XP_DIR)/public/msgsdk/
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/built/$(OS_ARCH)-export-full-sdk/lib/*.lib $(SOURCE_XP_DIR)/$(OBJDIR)/lib/
ifeq ($(OS_ARCH),WINNT)
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/built/$(OS_ARCH)-export-full-sdk/lib/*.dll $(SOURCE_XP_DIR)/$(OBJDIR)/bin/
else
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/built/$(OS_ARCH)-export-full-sdk/lib/*.so $(SOURCE_XP_DIR)/$(OBJDIR)/bin/
endif





