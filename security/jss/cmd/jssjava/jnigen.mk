#! gmake
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

#######################################################################
# (1) Include initial platform-independent assignments (MANDATORY).   #
#######################################################################

include manifest.mn

JNI_GEN +=	com.netscape.jss.ssl.SSLInputStream			\
		com.netscape.jss.ssl.SSLOutputStream			\
		com.netscape.jss.ssl.SSLSocketImpl			\
		com.netscape.jss.pkcs11.PrivateKeyProxy		\
		com.netscape.jss.pkcs11.PublicKeyProxy		\
		com.netscape.jss.CryptoManager       			\
		com.netscape.jss.NSSInit						\
		com.netscape.jss.DatabaseCloser                 \
        com.netscape.jss.crypto.Algorithm     \
        com.netscape.jss.crypto.EncryptionAlgorithm     \
		com.netscape.jss.crypto.PQGParams				\
		com.netscape.jss.pkcs11.PK11Token				\
		com.netscape.jss.pkcs11.CertProxy				\
        com.netscape.jss.pkcs11.CipherContextProxy      \
		com.netscape.jss.pkcs11.ModuleProxy				\
		com.netscape.jss.pkcs11.PK11RSAPublicKey		\
		com.netscape.jss.pkcs11.PK11DSAPublicKey		\
		com.netscape.jss.pkcs11.PK11KeyPairGenerator	\
		com.netscape.jss.pkcs11.PK11KeyGenerator		\
		com.netscape.jss.pkcs11.PK11Cert				\
		com.netscape.jss.pkcs11.PK11Cipher				\
		com.netscape.jss.pkcs11.PK11MessageDigest		\
		com.netscape.jss.pkcs11.PK11Module				\
		com.netscape.jss.pkcs11.PK11PrivKey				\
		com.netscape.jss.pkcs11.PK11PubKey				\
		com.netscape.jss.pkcs11.PK11SymKey				\
        com.netscape.jss.pkcs11.SymKeyProxy             \
		com.netscape.jss.pkcs11.SigContextProxy			\
		com.netscape.jss.pkcs11.PK11Signature			\
		com.netscape.jss.pkcs11.PK11Store				\
        com.netscape.jss.pkcs11.PK11KeyWrapper          \
		com.netscape.jss.util.Password                  \
        com.netscape.jss.util.Debug                     \
        com.netscape.jss.pkcs11.PK11SecureRandom        \
		$(NULL)

#######################################################################
# (2) Include "global" configuration information. (OPTIONAL)          #
#######################################################################

include $(CORE_DEPTH)/coreconf/config.mk

#######################################################################
# (3) Include "component" configuration information. (OPTIONAL)       #
#######################################################################

include $(CORE_DEPTH)/$(MODULE)/config/config.mk

#######################################################################
# (4) Include "local" platform-dependent assignments (OPTIONAL).      #
#######################################################################

ALL_TRASH += nativeMethods.h

#######################################################################
# (5) Execute "global" rules. (OPTIONAL)                              #
#######################################################################

include $(CORE_DEPTH)/coreconf/rules.mk

#######################################################################
# (6) Execute "component" rules. (OPTIONAL)                           #
#######################################################################

include $(CORE_DEPTH)/$(MODULE)/config/rules.mk

#######################################################################
# (7) Execute "local" rules. (OPTIONAL).                              #
#######################################################################

export::
	@if test ! -d nativeMethods.h; then						\
		echo perl GenNativesToRegister.pl nativeMethods.h $(JNI_GEN) ;		\
		perl GenNativesToRegister.pl nativeMethods.h $(JNI_GEN) ;		\
	else										\
		echo "Checking to see if nativeMethods.h file is out of date" ;		\
		cmd="perl regen_nativeMethods.pl $(PERLARG)				\
				-d $(JNI_GEN_DIR) nativeMethods.h $(JNI_GEN)";		\
		echo $$cmd;								\
		list=`$$cmd`;								\
		if test "$${list}x" != "x"; then					\
			echo perl GenNativesToRegister.pl nativeMethods.h $(JNI_GEN) ;	\
			perl GenNativesToRegister.pl nativeMethods.h $(JNI_GEN)	;	\
		fi									\
	fi

