#! /bin/ksh  
#
# This is just a quick script so we can still run our testcases.
# Longer term we need a scriptable test environment..
#
. ../common/init.sh
CURDIR=`pwd`

CIPHERDIR=${HOSTDIR}/cipher
CIPHERTESTDIR=${CURDIR}/../../cmd/bltest

echo "<HTML><BODY>" >> ${RESULTS}

#temporary files
TMP=${TMP-/tmp}

#TEMPFILES="${NOISE_FILE}"

#
# should also try to kill any running server
#
#trap "rm -f ${TEMPFILES};  exit"  2 3

mkdir -p ${CIPHERDIR}

echo "<TABLE BORDER=1><TR><TH COLSPAN=3>Cipher Tests</TH></TR>" >> ${RESULTS}
echo "<TR><TH width=500>Test Case</TH><TH width=50>Result</TH></TR>" >> ${RESULTS}

bltest -T des_ecb -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"DES ECB Encrypt"}
fi
bltest -T des_ecb -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"DES ECB Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>DES ECB</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>DES ECB</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

bltest -T des_cbc -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"DES CBC Encrypt"}
fi
bltest -T des_cbc -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"DES CBC Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>DES CBC</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>DES CBC</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

bltest -T des3_ecb -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"3DES ECB Encrypt"}
fi
bltest -T des3_ecb -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"3DES ECB Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>3DES ECB</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>3DES ECB</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

bltest -T des3_cbc -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"3DES CBC Encrypt"}
fi
bltest -T des3_cbc -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"3DES CBC Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>3DES CBC</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>3DES CBC</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

bltest -T rc2_ecb -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RC2 ECB Encrypt"}
fi
bltest -T rc2_ecb -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RC2 ECB Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>RC2 ECB</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>RC2 ECB</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

bltest -T rc2_cbc -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RC2 CBC Encrypt"}
fi
bltest -T rc2_cbc -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RC2 CBC Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>RC2 CBC</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>RC2 CBC</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

bltest -T rc4 -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RC4 Encrypt"}
fi
bltest -T rc4 -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RC4 Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>RC4</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>RC4</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

bltest -T rsa -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RSA Encrypt"}
fi
bltest -T rsa -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RSA Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>RSA</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>RSA</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

bltest -T dsa -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"DSA Sign"}
fi
bltest -T dsa -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"DSA Verify"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>DSA</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>DSA</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

bltest -T md2 -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"MD2 Hash"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>MD2</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>MD2</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

bltest -T md5 -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"MD5 Hash"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>MD5</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>MD5</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

bltest -T sha1 -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"SHA1 Hash"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>SHA1</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>SHA1</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "</TABLE><BR>" >> ${RESULTS}

#rm -f ${TEMPFILES}
cd ${CURDIR}

echo "</BODY></HTML>" >> ${RESULTS}
