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

ifeq ($(JAVADOC_NSS),1)
JAVADOC_TARGETS=                                    				\
					org.mozilla.jss.NSSInit						\
					org.mozilla.jss.KeyDatabaseException			\
					org.mozilla.jss.CertDatabaseException			\
					org.mozilla.jss.crypto.AlreadyInitializedException \
					org.mozilla.jss.util.Password					\
					org.mozilla.jss.util.PasswordCallback 			\
					org.mozilla.jss.util.PasswordCallbackInfo		\
					org.mozilla.jss.util.ConsolePasswordCallback 	\
					org.mozilla.jss.ssl.PrintOutputStreamWriter	\
					org.mozilla.jss.ssl.SSLHandshakeCompletedEvent	\
					org.mozilla.jss.ssl.SSLHandshakeCompletedListener\
					org.mozilla.jss.ssl.SSLInputStream				\
					org.mozilla.jss.ssl.SSLOutputStream			\
					org.mozilla.jss.ssl.SSLSecurityStatus			\
					org.mozilla.jss.ssl.SSLServerSocket			\
					org.mozilla.jss.ssl.SSLSocket					\
					org.mozilla.jss.ssl.SSLSocketImpl				\
					$(NULL)
else
JAVADOC_TARGETS=                                    \
					org.mozilla.jss.util.Assert	\
					org.mozilla.jss.util.AssertionException	\
					org.mozilla.jss.util.Password	\
					org.mozilla.jss.util.PasswordCallback \
					org.mozilla.jss.util.IncorrectPasswordException \
					org.mozilla.jss.util.NotImplementedException \
					org.mozilla.jss.util.NullPasswordCallback\
					org.mozilla.jss.util.ConsolePasswordCallback\
					org.mozilla.jss.util.PasswordCallbackInfo\
					org.mozilla.jss.util.UTF8Converter \
					util/Debug.java					\
                    org.mozilla.jss.crypto         \
					org.mozilla.jss					\
					org.mozilla.jss.pkcs11.PK11Module	\
                    org.mozilla.jss.ssl            \
                    org.mozilla.jss.asn1                           \
                    org.mozilla.jss.pkix.primitive                 \
                    org.mozilla.jss.pkix.cert                      \
                    org.mozilla.jss.pkix.crmf                      \
					org.mozilla.jss.pkix.cmmf							\
                    org.mozilla.jss.pkcs7 							\
                    org.mozilla.jss.pkcs12 						\
                    $(NULL)
endif

ifndef NUKE_JAVA_FILES
javadoc:
	@echo \
"Generating Javadoc will destroy any .java files in the top-level"
	@echo \
"ns/ninja/org/mozilla/jss directory.  You must set the NUKE_JAVA_FILES"
	@echo \
"environment variable to confirm that this is OK."
else
javadoc:
	cp manage/*.java .
	if [ ! -d "$(CORE_DEPTH)/ninja/doc" ] ; then mkdir $(CORE_DEPTH)/ninja/doc; fi
	/share/builds/components/cms_jdk/SunOS/1.2.2_05a/bin/javadoc -native -public -sourcepath $(CORE_DEPTH)/ninja -d $(CORE_DEPTH)/ninja/doc  $(JAVADOC_TARGETS)
	rm *.java
endif
