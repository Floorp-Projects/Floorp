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
					com.netscape.jss.NSSInit						\
					com.netscape.jss.KeyDatabaseException			\
					com.netscape.jss.CertDatabaseException			\
					com.netscape.jss.crypto.AlreadyInitializedException \
					com.netscape.jss.util.Password					\
					com.netscape.jss.util.PasswordCallback 			\
					com.netscape.jss.util.PasswordCallbackInfo		\
					com.netscape.jss.util.ConsolePasswordCallback 	\
					com.netscape.jss.ssl.PrintOutputStreamWriter	\
					com.netscape.jss.ssl.SSLHandshakeCompletedEvent	\
					com.netscape.jss.ssl.SSLHandshakeCompletedListener\
					com.netscape.jss.ssl.SSLInputStream				\
					com.netscape.jss.ssl.SSLOutputStream			\
					com.netscape.jss.ssl.SSLSecurityStatus			\
					com.netscape.jss.ssl.SSLServerSocket			\
					com.netscape.jss.ssl.SSLSocket					\
					com.netscape.jss.ssl.SSLSocketImpl				\
					$(NULL)
else
JAVADOC_TARGETS=                                    \
					com.netscape.jss.util.Assert	\
					com.netscape.jss.util.AssertionException	\
					com.netscape.jss.util.Password	\
					com.netscape.jss.util.PasswordCallback \
					com.netscape.jss.util.IncorrectPasswordException \
					com.netscape.jss.util.NotImplementedException \
					com.netscape.jss.util.NullPasswordCallback\
					com.netscape.jss.util.ConsolePasswordCallback\
					com.netscape.jss.util.PasswordCallbackInfo\
					com.netscape.jss.util.UTF8Converter \
					util/Debug.java					\
                    com.netscape.jss.crypto         \
					com.netscape.jss					\
					com.netscape.jss.pkcs11.PK11Module	\
                    com.netscape.jss.ssl            \
                    com.netscape.jss.asn1                           \
                    com.netscape.jss.pkix.primitive                 \
                    com.netscape.jss.pkix.cert                      \
                    com.netscape.jss.pkix.crmf                      \
					com.netscape.jss.pkix.cmmf							\
                    com.netscape.jss.pkcs7 							\
                    com.netscape.jss.pkcs12 						\
                    $(NULL)
endif

ifndef NUKE_JAVA_FILES
javadoc:
	@echo \
"Generating Javadoc will destroy any .java files in the top-level"
	@echo \
"ns/ninja/com/netscape/jss directory.  You must set the NUKE_JAVA_FILES"
	@echo \
"environment variable to confirm that this is OK."
else
javadoc:
	cp manage/*.java .
	if [ ! -d "$(CORE_DEPTH)/ninja/doc" ] ; then mkdir $(CORE_DEPTH)/ninja/doc; fi
	/share/builds/components/cms_jdk/SunOS/1.2.2_05a/bin/javadoc -native -public -sourcepath $(CORE_DEPTH)/ninja -d $(CORE_DEPTH)/ninja/doc  $(JAVADOC_TARGETS)
	rm *.java
endif
