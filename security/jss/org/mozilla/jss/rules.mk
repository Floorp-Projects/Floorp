# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape Security Services for Java.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

JAVADOC_TARGETS=                                                        \
                    org.mozilla.jss                                     \
                    org.mozilla.jss.asn1                                \
                    org.mozilla.jss.crypto                              \
                    org.mozilla.jss.pkcs7                               \
                    org.mozilla.jss.pkcs10                              \
                    org.mozilla.jss.pkcs11                              \
                    org.mozilla.jss.pkcs12                              \
                    org.mozilla.jss.pkix.primitive                      \
                    org.mozilla.jss.pkix.cert                           \
                    org.mozilla.jss.pkix.cmc                            \
                    org.mozilla.jss.pkix.cmmf                           \
                    org.mozilla.jss.pkix.cms                            \
                    org.mozilla.jss.pkix.crmf                           \
                    org.mozilla.jss.provider                            \
                    org.mozilla.jss.ssl                                 \
                    org.mozilla.jss.tests                               \
                    org.mozilla.jss.util.Assert                         \
                    org.mozilla.jss.util.AssertionException             \
                    org.mozilla.jss.util.Base64OutputStream             \
                    org.mozilla.jss.util.ConsolePasswordCallback        \
                    org.mozilla.jss.util.IncorrectPasswordException     \
                    org.mozilla.jss.util.InvalidNicknameException       \
                    org.mozilla.jss.util.NativeProxy                    \
                    org.mozilla.jss.util.NotImplementedException        \
                    org.mozilla.jss.util.NullPasswordCallback           \
                    org.mozilla.jss.util.Password                       \
                    org.mozilla.jss.util.PasswordCallback               \
                    org.mozilla.jss.util.PasswordCallbackInfo           \
                    org.mozilla.jss.util.Tunnel                         \
                    org.mozilla.jss.util.UTF8Converter                  \
                    util/Debug.java                                     \
                    $(NULL)

ifneq ($(HTML_HEADER),)
HTML_HEADER_OPT=-header '$(HTML_HEADER)'
endif

javadoc:
	cp -i manage/*.java .
	if [ ! -d "$(DIST)/jssdoc" ] ; then mkdir -p $(CORE_DEPTH)/jssdoc ; fi
	$(JAVADOC) -native -private -sourcepath $(CORE_DEPTH)/jss -d $(CORE_DEPTH)/jssdoc $(HTML_HEADER_OPT) $(JAVADOC_TARGETS)
	rm -i *.java
