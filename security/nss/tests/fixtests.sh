#!/bin/sh
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
# The Original Code is the elliptic curve test suite.
#
# The Initial Developer of the Original Code is Sun Microsystems, Inc.
# Portions created by Sun Microsystems, Inc. are Copyright (C) 2003
# Sun Microsystems, Inc. All Rights Reserved.
#
# Contributor(s):
#	Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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

####################### fix_test_scripts #######################
#
# Depending on the argument either enable or disable EC based
# tests in the cert and ssl directories.
#
################################################################
fix_test_scripts()
{
    FLAG=$1
    CERT_DIR=cert
    CERT_SCRIPT=cert.sh
    SSL_DIR=ssl
    SSLAUTH=sslauth.txt
    SSLCOV=sslcov.txt
    SSL_SCRIPT=ssl.sh
    SSLSTRESS=sslstress.txt
    TOOLS_DIR=tools
    TOOLS_SCRIPT=tools.sh
    EC_PREFIX=ec
    NOEC_PREFIX=noec

    if [ xx$FLAG = xx"enable_ecc" ]; then
    	if [ -f $CERT_DIR/$NOEC_PREFIX$CERT_SCRIPT -a \
	     -f $SSL_DIR/$NOEC_PREFIX$SSLAUTH -a \
	     -f $SSL_DIR/$NOEC_PREFIX$SSLCOV -a \
	     -f $SSL_DIR/$NOEC_PREFIX$SSL_SCRIPT -a \
	     -f $SSL_DIR/$NOEC_PREFIX$SSLSTRESS -a \
	     -f $TOOLS_DIR/$NOEC_PREFIX$TOOLS_SCRIPT ]; then
	     echo "noecc files exist"
	else
	     echo "noecc files are missing"
             echo "Saving files as noec"
	     cp $CERT_DIR/$CERT_SCRIPT $CERT_DIR/$NOEC_PREFIX$CERT_SCRIPT
	     cp $SSL_DIR/$SSLAUTH $SSL_DIR/$NOEC_PREFIX$SSLAUTH
	     cp $SSL_DIR/$SSLCOV $SSL_DIR/$NOEC_PREFIX$SSLCOV
	     cp $SSL_DIR/$SSL_SCRIPT $SSL_DIR/$NOEC_PREFIX$SSL_SCRIPT
	     cp $SSL_DIR/$SSLSTRESS $SSL_DIR/$NOEC_PREFIX$SSLSTRESS
	     cp $TOOLS_DIR/$TOOLS_SCRIPT $TOOLS_DIR/$NOEC_PREFIX$TOOLS_SCRIPT
	fi
	echo "Overwriting with ec versions"
	cp $CERT_DIR/$EC_PREFIX$CERT_SCRIPT $CERT_DIR/$CERT_SCRIPT
	cp $SSL_DIR/$EC_PREFIX$SSLAUTH $SSL_DIR/$SSLAUTH
	cp $SSL_DIR/$EC_PREFIX$SSLCOV $SSL_DIR/$SSLCOV
	cp $SSL_DIR/$EC_PREFIX$SSL_SCRIPT $SSL_DIR/$SSL_SCRIPT
	cp $SSL_DIR/$EC_PREFIX$SSLSTRESS $SSL_DIR/$SSLSTRESS
	cp $TOOLS_DIR/$EC_PREFIX$TOOLS_SCRIPT $TOOLS_DIR/$TOOLS_SCRIPT 
    elif [ xx$FLAG = xx"disable_ecc" ]; then
    	if [ -f $CERT_DIR/$NOEC_PREFIX$CERT_SCRIPT -a \
	     -f $SSL_DIR/$NOEC_PREFIX$SSLAUTH -a \
	     -f $SSL_DIR/$NOEC_PREFIX$SSLCOV -a \
	     -f $SSL_DIR/$NOEC_PREFIX$SSL_SCRIPT -a \
	     -f $SSL_DIR/$NOEC_PREFIX$SSLSTRESS -a \
	     -f $TOOLS_DIR/$NOEC_PREFIX$TOOLS_SCRIPT ]; then
	     echo "noecc files exist"
	     echo "Overwriting with noec versions"
	     cp $CERT_DIR/$NOEC_PREFIX$CERT_SCRIPT $CERT_DIR/$CERT_SCRIPT
	     cp $SSL_DIR/$NOEC_PREFIX$SSLAUTH $SSL_DIR/$SSLAUTH
	     cp $SSL_DIR/$NOEC_PREFIX$SSLCOV $SSL_DIR/$SSLCOV
	     cp $SSL_DIR/$NOEC_PREFIX$SSL_SCRIPT $SSL_DIR/$SSL_SCRIPT
	     cp $SSL_DIR/$NOEC_PREFIX$SSLSTRESS $SSL_DIR/$SSLSTRESS
	     cp $TOOLS_DIR/$NOEC_PREFIX$TOOLS_SCRIPT $TOOLS_DIR/$TOOLS_SCRIPT 
	else
	     echo "Already disabled."
	fi
    else 
        echo "Needs either \"enable_ecc\" or \"disable_ecc\" as argument."
    fi
}


fix_test_scripts $1
