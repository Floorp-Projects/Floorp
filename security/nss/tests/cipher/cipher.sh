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

echo "bltest -T -m des_ecb -E -d ${CIPHERTESTDIR}"
bltest -T -m des_ecb -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"DES ECB Encrypt"}
fi
echo "bltest -T -m des_ecb -D -d ${CIPHERTESTDIR}"
bltest -T -m des_ecb -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"DES ECB Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>DES ECB</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>DES ECB</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "bltest -T -m des_cbc -E -d ${CIPHERTESTDIR}"
bltest -T -m des_cbc -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"DES CBC Encrypt"}
fi
echo "bltest -T -m des_cbc -D -d ${CIPHERTESTDIR}"
bltest -T -m des_cbc -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"DES CBC Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>DES CBC</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>DES CBC</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "bltest -T -m des3_ecb -E -d ${CIPHERTESTDIR}"
bltest -T -m des3_ecb -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"3DES ECB Encrypt"}
fi
echo "bltest -T -m des3_ecb -D -d ${CIPHERTESTDIR}"
bltest -T -m des3_ecb -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"3DES ECB Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>3DES ECB</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>3DES ECB</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "bltest -T -m des3_cbc -E -d ${CIPHERTESTDIR}"
bltest -T -m des3_cbc -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"3DES CBC Encrypt"}
fi
echo "bltest -T -m des3_cbc -D -d ${CIPHERTESTDIR}"
bltest -T -m des3_cbc -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"3DES CBC Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>3DES CBC</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>3DES CBC</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "bltest -T -m rc2_ecb -E -d ${CIPHERTESTDIR}"
bltest -T -m rc2_ecb -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RC2 ECB Encrypt"}
fi
echo "bltest -T -m rc2_ecb -D -d ${CIPHERTESTDIR}"
bltest -T -m rc2_ecb -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RC2 ECB Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>RC2 ECB</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>RC2 ECB</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "bltest -T -m rc2_cbc -E -d ${CIPHERTESTDIR}"
bltest -T -m rc2_cbc -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RC2 CBC Encrypt"}
fi
echo "bltest -T -m rc2_cbc -D -d ${CIPHERTESTDIR}"
bltest -T -m rc2_cbc -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RC2 CBC Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>RC2 CBC</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>RC2 CBC</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "bltest -T -m rc4 -E -d ${CIPHERTESTDIR}"
bltest -T -m rc4 -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RC4 Encrypt"}
fi
echo "bltest -T -m rc4 -D -d ${CIPHERTESTDIR}"
bltest -T -m rc4 -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RC4 Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>RC4</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>RC4</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "bltest -T -m rsa -E -d ${CIPHERTESTDIR}"
bltest -T -m rsa -E -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RSA Encrypt"}
fi
echo "bltest -T -m rsa -D -d ${CIPHERTESTDIR}"
bltest -T -m rsa -D -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"RSA Encrypt"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>RSA</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>RSA</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "bltest -T -m dsa -S -d ${CIPHERTESTDIR}"
bltest -T -m dsa -S -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"DSA Sign"}
fi
echo "bltest -T -m dsa -V -d ${CIPHERTESTDIR}"
bltest -T -m dsa -V -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"DSA Verify"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>DSA</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>DSA</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "bltest -T -m md2 -H -d ${CIPHERTESTDIR}"
bltest -T -m md2 -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"MD2 Hash"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>MD2</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>MD2</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "bltest -T -m md5 -H -d ${CIPHERTESTDIR}"
bltest -T -m md5 -H -d ${CIPHERTESTDIR}
if [ $? -ne 0 ]; then
   CIPHERFAILED=${CIPHERFAILED-"MD5 Hash"}
fi
if [ -n "${CIPHERFAILED}" ]; then
    echo "<TR><TD>MD5</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>MD5</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "bltest -T -m sha1 -H -d ${CIPHERTESTDIR}"
bltest -T -m sha1 -H -d ${CIPHERTESTDIR}
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
