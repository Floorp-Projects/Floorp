# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

#
# This rule 'imports' the msgsdk headers and libs for our use
#



export::
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/include/*.h $(SOURCE_XP_DIR)/public/msgsdk/
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/protocol/SMTP/include/*.h $(SOURCE_XP_DIR)/public/msgsdk/
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/protocol/IMAP4/include/*.h $(SOURCE_XP_DIR)/public/msgsdk/
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/protocol/MIME/include/*.h $(SOURCE_XP_DIR)/public/msgsdk/
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/protocol/POP3/include/*.h $(SOURCE_XP_DIR)/public/msgsdk/
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/built/$(OS_ARCH)-export-full-sdk/lib/*.lib $(SOURCE_XP_DIR)/$(OBJDIR)/lib/
ifeq ($(OS_ARCH),WINNT)
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/built/$(OS_ARCH)-export-full-sdk/lib/*.dll $(SOURCE_XP_DIR)/$(OBJDIR)/bin/
else
	$(INSTALL) -m 555 $(GDEPTH)/msgsdk/C/built/$(OS_ARCH)-export-full-sdk/lib/*.so $(SOURCE_XP_DIR)/$(OBJDIR)/bin/
endif





