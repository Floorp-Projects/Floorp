#! /bin/ksh  
#
# This is just a quick script so we can still run our testcases.
# Longer term we need a scriptable test environment..
#
. ../common/init.sh
CURDIR=`pwd`

SMIMEDIR=${HOSTDIR}/smime
CADIR=${SMIMEDIR}/cadir
ALICEDIR=${SMIMEDIR}/alicedir
BOBDIR=${SMIMEDIR}/bobdir

echo "<HTML><BODY>" >> ${RESULTS}

#temporary files
TMP=${TMP-/tmp}
PWFILE=${TMP}/tests.pw.$$
CERTSCRIPT=${TMP}/tests_certs.$$
NOISE_FILE=${TMP}/tests_noise.$$

TEMPFILES="${PWFILE} ${CERTSCRIPT} ${NOISE_FILE}"

#
# should also try to kill any running server
#
trap "rm -f ${TEMPFILES};  exit"  2 3

# Generate noise for our CA cert.
#
# NOTE: these keys are only suitable for testing, as this whole thing bypasses
# the entropy gathering. Don't use this method to generate keys and certs for
# product use or deployment.
#
ps -efl > ${NOISE_FILE} 2>&1
ps aux >> ${NOISE_FILE} 2>&1
netstat >> ${NOISE_FILE} 2>&1
date >> ${NOISE_FILE} 2>&1

mkdir -p ${SMIMEDIR}
mkdir -p ${CADIR}
mkdir -p ${ALICEDIR}
mkdir -p ${BOBDIR}
#
# build the TEMP CA used for testing purposes
# 
echo "<TABLE BORDER=1><TR><TH COLSPAN=3>Certutil Tests</TH></TR>" >> ${RESULTS}
echo "<TR><TH width=500>Test Case</TH><TH width=50>Result</TH></TR>" >> ${RESULTS}
echo "********************** Creating a CA Certificate **********************"
echo nss > ${PWFILE}
echo "   certutil -N -d ${CADIR} -f ${PWFILE}"
certutil -N -d ${CADIR} -f ${PWFILE}

echo initialized
echo 5 > ${CERTSCRIPT}
echo 9 >> ${CERTSCRIPT}
echo n >> ${CERTSCRIPT}
echo y >> ${CERTSCRIPT}
echo 3 >> ${CERTSCRIPT}
echo n >> ${CERTSCRIPT}
echo 5 >> ${CERTSCRIPT}
echo 6 >> ${CERTSCRIPT}
echo 7 >> ${CERTSCRIPT}
echo 9 >> ${CERTSCRIPT}
echo n >> ${CERTSCRIPT}
echo    "certutil -S -n \"TestCA\" -s \"CN=NSS Test CA, O=BOGUS NSS, L=Mountain View, ST=California, C=US\" -t \"CTu,CTu,CTu\" -v 60 -x -d ${CADIR} -1 -2 -5 -f ${PWFILE} -z ${NOISE_FILE}"
certutil -S -n "TestCA" -s "CN=NSS Test CA, O=BOGUS NSS, L=Mountain View, ST=California, C=US" -t "CTu,CTu,CTu" -v 60 -x -d ${CADIR} -1 -2 -5 -f ${PWFILE} -z ${NOISE_FILE} < ${CERTSCRIPT}

if [ $? -ne 0 ]; then
    echo "<TR><TD>Creating CA Cert</TD><TD bgcolor=red>Failed</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Creating CA Cert</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi
cd ${CADIR}
echo "   certutil -L -n \"TestCA\" -r -d ${CADIR} > root.cert"
certutil -L -n "TestCA" -r -d ${CADIR} > root.cert
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Export Root"}
fi

echo "**************** Creating Client CA Issued Certificates ****************"
certutil -N -d ${ALICEDIR} -f ${PWFILE}
netstat >> ${NOISE_FILE} 2>&1
date >> ${NOISE_FILE} 2>&1
cd ${ALICEDIR}
echo "Import the root CA"
echo "   certutil -A -n \"TestCA\" -t \"TC,TC,TC\" -f ${PWFILE} -d ${ALICEDIR} -i ${CADIR}/root.cert"
certutil -A -n "TestCA" -t "TC,TC,TC" -f ${PWFILE} -d ${ALICEDIR} -i ${CADIR}/root.cert
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Import Root"}
fi
echo "Generate a Certificate request"
echo  "  certutil -R -s \"CN=Alice, E=alice@bogus.com, O=BOGUS Netscape, L=Mountain View, ST=California, C=US\" -d ${ALICEDIR}  -f ${PWFILE} -z ${NOISE_FILE} -o req"
certutil -R -s "CN=Alice, E=alice@bogus.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US" -d ${ALICEDIR}  -f ${PWFILE} -z ${NOISE_FILE} -o req
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Generate Request"}
fi
echo "Sign the Certificate request"
echo  "certutil -C -c "TestCA" -m 3 -v 60 -d ${CADIR} -f ${PWFILE} -i req -o alice.cert"
certutil -C -c "TestCA" -m 3 -v 60 -d ${CADIR} -i req -o alice.cert -f ${PWFILE}
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Sign Alice's Cert"}
fi
echo "Import the new Cert"
echo "certutil -A -n \"Alice\" -t \"u,u,u\" -d ${ALICEDIR} -f ${PWFILE} -i alice.cert"
certutil -A -n "Alice" -t "u,u,u" -d ${ALICEDIR} -f ${PWFILE} -i alice.cert
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Import Alice's cert"}
fi
if [ -n "${CERTFAILED}" ]; then
    echo "<TR><TD>Creating Alice's email cert</TD><TD bgcolor=red>Failed ($CERTFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Creating Alice's email cert</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

netstat >> ${NOISE_FILE} 2>&1
date >> ${NOISE_FILE} 2>&1
certutil -N -d ${BOBDIR} -f ${PWFILE}
cd ${BOBDIR}
echo "Import the root CA"
echo "   certutil -A -n \"TestCA\" -t \"TC,TC,TC\" -f ${PWFILE} -d ${BOBDIR} -i ${CADIR}/root.cert"
certutil -A -n "TestCA" -t "TC,TC,TC" -f ${PWFILE} -d ${BOBDIR} -i ${CADIR}/root.cert
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Import Root"}
fi
echo "Generate a Certificate request"
echo  "  certutil -R -s \"CN=Bob, E=bob@bogus.com, O=BOGUS Netscape, L=Mountain View, ST=California, C=US\" -d ${BOBDIR}  -f ${PWFILE} -z ${NOISE_FILE} -o req"
certutil -R -s "CN=Bob, E=bob@bogus.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US" -d ${BOBDIR}  -f ${PWFILE} -z ${NOISE_FILE} -o req
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Generate Request"}
fi
echo "Sign the Certificate request"
echo  "certutil -C -c "TestCA" -m 4 -v 60 -d ${CADIR} -f ${PWFILE} -i req -o bob.cert"
certutil -C -c "TestCA" -m 4 -v 60 -d ${CADIR} -i req -o bob.cert -f ${PWFILE}
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Sign Bob's cert"}
fi
echo "Import the new Cert"
echo "certutil -A -n \"Bob\" -t \"u,u,u\" -d ${BOBDIR} -f ${PWFILE} -i bob.cert"
certutil -A -n "Bob" -t "u,u,u" -d ${BOBDIR} -f ${PWFILE} -i bob.cert
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Import Bob's cert"}
fi
if [ -n "${CERTFAILED}" ]; then
    echo "<TR><TD>Creating Bob's email cert</TD><TD bgcolor=red>Failed ($CERTFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Creating Bob's email cert</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

netstat >> ${NOISE_FILE} 2>&1
date >> ${NOISE_FILE} 2>&1
cd ${CADIR}
echo "Generate a third cert"
echo "certutil -S -n \"Dave\" -c \"TestCA\" -t \"u,u,u\" -s \"CN=Dave, E=dave@bogus.com, O=BOGUS Netscape, L=Mountain View, ST=California, C=US\" -d ${CADIR} -f ${PWFILE} -z ${NOISE_FILE} -m 5 -v 60"
certutil -S -n "Dave" -c "TestCA" -t "u,u,u" -s "CN=Dave, E=dave@bogus.com, O=BOGUS Netscape, L=Mountain View, ST=California, C=US" -d ${CADIR} -f ${PWFILE} -z ${NOISE_FILE} -m 5 -v 60

echo "Import Alices's cert into Bob's db"
echo "certutil -E -t \"u,u,u\" -d ${BOBDIR} -f ${PWFILE} -i ${ALICEDIR}/alice.cert"
certutil -E -t "u,u,u" -d ${BOBDIR} -f ${PWFILE} -i ${ALICEDIR}/alice.cert
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Import Alice's cert into Bob's db"}
fi
echo "Import Bob's cert into Alice's db"
echo "certutil -E -t \"u,u,u\" -d ${ALICEDIR} -f ${PWFILE} -i ${BOBDIR}/bob.cert"
certutil -E -t "u,u,u" -d ${ALICEDIR} -f ${PWFILE} -i ${BOBDIR}/bob.cert
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Import Bob's cert into Alice's db"}
fi
echo "Import Dave's cert into Alice's and Bob's dbs"
echo "   certutil -L -n \"Dave\" -r -d ${CADIR} > dave.cert"
certutil -L -n "Dave" -r -d ${CADIR} > dave.cert
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Export Dave's cert"}
fi
echo "certutil -E -t \"u,u,u\" -d ${ALICEDIR} -f ${PWFILE} -i ${CADIR}/dave.cert"
certutil -E -t "u,u,u" -d ${ALICEDIR} -f ${PWFILE} -i ${CADIR}/dave.cert
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Import Dave's cert into Alice's db"}
fi
echo "certutil -E -t \"u,u,u\" -d ${BOBDIR} -f ${PWFILE} -i ${CADIR}/dave.cert"
certutil -E -t "u,u,u" -d ${BOBDIR} -f ${PWFILE} -i ${CADIR}/dave.cert
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Import Dave's cert into Bob's db"}
fi
echo "</TABLE><BR>" >> ${RESULTS}

echo "********************* S/MIME testing  ****************************"
echo "<TABLE BORDER=1><TR><TH COLSPAN=3>S/MIME tests</TH></TR>" >> ${RESULTS}
echo "<TR><TH width=500>Test Case</TH><TH width=50>Result</TH></TR>" >> ${RESULTS}
cd ${SMIMEDIR}
cp ${CURDIR}/alice.txt ${SMIMEDIR}
# Test basic signed and enveloped messages from 1 --> 2
cmsutil -S -N Alice -i alice.txt -d ${ALICEDIR} -p nss -o alice.sig
if [ $? -ne 0 ]; then
   CMSFAILED=${CMSFAILED-"Create Signature Alice"}
fi
cmsutil -D -i alice.sig -d ${BOBDIR} -o alice.data1
if [ $? -ne 0 ]; then
   CMSFAILED=${CMSFAILED-"Decode Alice's Signature"}
fi
diff alice.txt alice.data1
if [ $? -ne 0 ]; then
   echo "<TR><TD>Signing attached message</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
   echo "<TR><TD>Signing attached message</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi
cmsutil -E -r bob@bogus.com -i alice.txt -d ${ALICEDIR} -p nss -o alice.env
if [ $? -ne 0 ]; then
   CMSFAILED=${CMSFAILED-"Create Enveloped Data Alice"}
fi
cmsutil -D -i alice.env -d ${BOBDIR} -p nss -o alice.data1
if [ $? -ne 0 ]; then
   CMSFAILED=${CMSFAILED-"Decode Enveloped Data Alice"}
fi
diff alice.txt alice.data1
if [ $? -ne 0 ]; then
   echo "<TR><TD>Enveloped Data</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
   echo "<TR><TD>Enveloped Data</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi
# multiple recip
#cmsutil -E -i alicecc.txt -d alicedir -o alicecc.env -r bob@bogus.com,dave@bogus.com
#cmsutil -D -i alicecc.env -d bobdir -p nss

#certs-only
echo "cmsutil -O -r \"Alice,bob@bogus.com,dave@bogus.com\" -d ${ALICEDIR} > co.der"
cmsutil -O -r "Alice,bob@bogus.com,dave@bogus.com" -d ${ALICEDIR} > co.der
if [ $? -ne 0 ]; then
   CMSFAILED=${CMSFAILED-"Create Certs-Only Alice"}
fi
cmsutil -D -i co.der -d ${CADIR}
if [ $? -ne 0 ]; then
   CMSFAILED=${CMSFAILED-"Verify Certs-Only by CA"}
fi
if [ -n "${CMSFAILED}" ]; then
    echo "<TR><TD>Sending certs-only message</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Sending certs-only message</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi
echo "cmsutil -C -i alice.txt -e alicehello.env -d ${ALICEDIR} -r \"bob@bogus.com\" > alice.enc"
cmsutil -C -i alice.txt -e alicehello.env -d ${ALICEDIR} -r "bob@bogus.com" > alice.enc
if [ $? -ne 0 ]; then
   CMSFAILED=${CMSFAILED-"Create Encrypted-Data"}
fi
#echo "cmsutil -C -i bob.txt -e alicehello.env -d ${ALICEDIR} -r \"alice@bogus.com\" > bob.enc"
#cmsutil -C -i bob.txt -e alicehello.env -d ${ALICEDIR} -r "alice@bogus.com" > bob.enc
if [ $? -ne 0 ]; then
   CMSFAILED=${CMSFAILED-"Create Encrypted-Data"}
fi
echo "cmsutil -D -i alice.enc -d ${BOBDIR} -e alicehello.env -p nss"
cmsutil -D -i alice.enc -d ${BOBDIR} -e alicehello.env -p nss -o alice.data2
diff alice.txt alice.data2
if [ $? -ne 0 ]; then
   CMSFAILED=${CMSFAILED-"Decode Encrypted-Data"}
fi
if [ -n "${CMSFAILED}" ]; then
    echo "<TR><TD>Encrypted-Data message</TD><TD bgcolor=red>Failed ($CMSFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Encrypted-Data message</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "</TABLE><BR>" >> ${RESULTS}

rm -f ${TEMPFILES}
cd ${CURDIR}

echo "</BODY></HTML>" >> ${RESULTS}
