#! /bin/sh  
#
# This is just a quick script so we can still run our testcases.
# Longer term we need a scriptable test environment..
#
. ../common/init.sh
CURDIR=`pwd`

TOOLSDIR=${HOSTDIR}/tools
CADIR=${TOOLSDIR}/cadir
CERTDIR=${TOOLSDIR}/certdir
COPYDIR=${TOOLSDIR}/copydir
if [ ${OS_ARCH} = "WINNT" ]; then
ROOTMODULE=${LIBPATH}/nssckbi.dll
else
ROOTMODULE=${LIBPATH}/libnssckbi.so
fi

echo "<HTML><BODY>" >> ${RESULTS}

SONMI_DEBUG=ON	#we see starnge problems on hpux 64 - save all output
		# for now

#temporary files
if [ -n "$SONMI_DEBUG" -a "$SONMI_DEBUG" = "ON" ]
then
	TMP=${TOOLSDIR}
	PWFILE=${TMP}/tests.pw
	CERTSCRIPT=${TMP}/tests_certs
	MODSCRIPT=${TMP}/tests_mod
	MODLIST=${TMP}/tests_modlist
	SIGNSCRIPT=${TMP}/tests_sign
	NOISE_FILE=${TMP}/tests_noise
	CERTUTILOUT=${TMP}/certutil_out

	TEMPFILES=""
else
	TMP=${TMP-/tmp}
	PWFILE=${TMP}/tests.pw.$$
	CERTSCRIPT=${TMP}/tests_certs.$$
	MODSCRIPT=${TMP}/tests_mod.$$
	MODLIST=${TMP}/tests_modlist.$$
	SIGNSCRIPT=${TMP}/tests_sign.$$
	NOISE_FILE=${TMP}/tests_noise.$$
	CERTUTILOUT=${TMP}/certutil_out.$$

	TEMPFILES="${PWFILE} ${CERTSCRIPT} ${MODSCRIPT} ${MODLIST} ${SIGNSCRIPT} ${NOISE_FILE} ${CERTUTILOUT}"
	#
	# should also try to kill any running server
	#
	trap "rm -f ${TEMPFILES};  exit"  2 3
fi

mkdir -p ${TOOLSDIR}
mkdir -p ${CADIR}
mkdir -p ${CERTDIR}
mkdir -p ${COPYDIR}
cd ${CADIR}

rm ${CERTUTILOUT} 2>/dev/null

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

#
# build the TEMP CA used for testing purposes
# 
echo "<TABLE BORDER=1><TR><TH COLSPAN=3>Certutil Tests</TH></TR>" >> ${RESULTS}
echo "<TR><TH width=500>Test Case</TH><TH width=50>Result</TH></TR>" >> ${RESULTS}
echo "********************** Creating a CA Certificate **********************"
echo nss > ${PWFILE}
echo "   certutil -N -d ${CADIR} -f ${PWFILE} " 
certutil -N -d ${CADIR} -f ${PWFILE}  2>&1

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
echo    "certutil -S -n \"TestCA\" -s \"CN=NSS Test CA, O=BOGUS NSS, L=Mountain View, ST=California, C=US\" -t \"CTu,CTu,CTu\" -v 60 -x -d ${CADIR} -1 -2 -5 -f ${PWFILE} -z ${NOISE_FILE} " 
certutil -S -n "TestCA" -s "CN=NSS Test CA, O=BOGUS NSS, L=Mountain View, ST=California, C=US" -t "CTu,CTu,CTu" -v 60 -x -d ${CADIR} -1 -2 -5 -f ${PWFILE} -z ${NOISE_FILE} < ${CERTSCRIPT}  2>&1
if [ $? -ne 0 ]; then
    echo "<TR><TD>Creating CA Cert</TD><TD bgcolor=red>Failed</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Creating CA Cert</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi
echo "   certutil -L -n \"TestCA\" -r -d ${CADIR} > root.cert"
certutil -L -n "TestCA" -r -d ${CADIR} > root.cert 2>${CERTUTILOUT}
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Export Root"}
fi
cat ${CERTUTILOUT}
rm ${CERTUTILOUT} 2>/dev/null

echo "   certutil -N -d ${COPYDIR} -f ${PWFILE} "
echo "**************** Creating Client CA Issued Certificates ****************"
echo "   certutil -N -d ${CERTDIR} -f ${PWFILE} "
certutil -N -d ${CERTDIR} -f ${PWFILE}  2>&1
netstat >> ${NOISE_FILE} 2>&1
date >> ${NOISE_FILE} 2>&1
cd ${CERTDIR}
echo "Import the root CA"
echo "   certutil -A -n \"TestCA\" -t \"TC,TC,TC\" -f ${PWFILE} -d ${CERTDIR} -i ${CADIR}/root.cert "
certutil -A -n "TestCA" -t "TC,TC,TC" -f ${PWFILE} -d ${CERTDIR} -i ${CADIR}/root.cert  2>&1
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Import Root"}
fi
echo "Generate a Certificate request"
echo  "  certutil -R -s \"CN=Alice, E=alice@bogus.com, O=BOGUS Netscape, L=Mountain View, ST=California, C=US\" -d ${CERTDIR}  -f ${PWFILE} -z ${NOISE_FILE} -o req "
certutil -R -s "CN=Alice, E=alice@bogus.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US" -d ${CERTDIR}  -f ${PWFILE} -z ${NOISE_FILE} -o req  2>&1
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Generate Request"}
fi
echo "Sign the Certificate request"
echo  "certutil -C -c \"TestCA\" -m 3 -v 60 -d ${CADIR} -f ${PWFILE} -i req -o alice.cert "
certutil -C -c "TestCA" -m 3 -v 60 -d ${CADIR} -i req -o alice.cert -f ${PWFILE}  2>&1
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Sign Alice's Cert"}
fi
echo "Import the new Cert"
echo "certutil -A -n \"Alice\" -t \"u,u,u\" -d ${CERTDIR} -f ${PWFILE} -i alice.cert "
certutil -A -n "Alice" -t "u,u,u" -d ${CERTDIR} -f ${PWFILE} -i alice.cert  2>&1
if [ $? -ne 0 ]; then
   CERTFAILED=${CERTFAILED-"Import Alice's cert"}
fi
if [ -n "${CERTFAILED}" ]; then
    echo "<TR><TD>Creating Alice's email cert</TD><TD bgcolor=red>Failed ($CERTFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Creating Alice's email cert</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

cd ${TOOLSDIR}

echo "Load the root cert module"
echo "" > ${MODSCRIPT}
echo "modutil -add \"Builtin Object Token\" -libfile ${ROOTMODULE} -dbdir ${CERTDIR}"
modutil -add "Builtin Object Token" -libfile ${ROOTMODULE} -dbdir ${CERTDIR} < ${MODSCRIPT} 2>&1
if [ $? -ne 0 ]; then
   MODFAILED=${MODFAILED-"Load Builtin Root Module"}
fi
if [ -n "${MODFAILED}" ]; then
    echo "<TR><TD>Loading Builtin Root Module</TD><TD bgcolor=red>Failed ($CERTFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Loading Builtin Root Module</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi
echo "Listing roots from builtin module"
echo "certutil -L -d ${CERTDIR} -h all | grep \"Builtin Object Token:\""
certutil -L -d ${CERTDIR} -h all | grep "Builtin Object Token:" > ${MODLIST}
if [ -s ${MODLIST} ]; then
    echo "<TR><TD>Listing Builtin Root Module</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Listing Builtin Root Module</TD><TD bgcolor=red>Failed ($CERTFAILED)</TD><TR>" >> ${RESULTS}
fi

echo "Export cert and key"
echo "pk12util -o alice.p12 -n \"Alice\" -d ${CERTDIR} -k ${PWFILE} -w ${PWFILE}"
pk12util -o alice.p12 -n "Alice" -d ${CERTDIR} -k ${PWFILE} -w ${PWFILE} 2>&1
if [ $? -ne 0 ]; then
   P12FAILED=${P12FAILED-"Export cert and key"}
fi
if [ -n "${P12FAILED}" ]; then
    echo "<TR><TD>Exporting Alice's email cert & key</TD><TD bgcolor=red>Failed ($CERTFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Exporting Alice's email cert & key</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "Import cert and key"
echo "pk12util -i alice.p12 -d ${COPYDIR} -k ${PWFILE} -w ${PWFILE}"
pk12util -i alice.p12 -d ${COPYDIR} -k ${PWFILE} -w ${PWFILE} 2>&1
if [ $? -ne 0 ]; then
   P12FAILED=${P12FAILED-"Import cert and key"}
fi
if [ -n "${P12FAILED}" ]; then
    echo "<TR><TD>Importing Alice's email cert & key</TD><TD bgcolor=red>Failed ($P12FAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Importing Alice's email cert & key</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "Create objsign cert"
echo "signtool -G \"objectsigner\" -d ${CERTDIR} -p \"nss\""
echo "y"    > ${SIGNSCRIPT}
echo "TEST" >> ${SIGNSCRIPT}
echo "MOZ"  >> ${SIGNSCRIPT}
echo "NSS"  >> ${SIGNSCRIPT}
echo "NY"   >> ${SIGNSCRIPT}
echo "US"   >> ${SIGNSCRIPT}
echo "liz"  >> ${SIGNSCRIPT}
echo "liz@moz.org" >> ${SIGNSCRIPT}
signtool -G "objsigner" -d ${CERTDIR} -p "nss" < ${SIGNSCRIPT} 2>&1

echo "Sign files in a directory"
mkdir -p ${TOOLSDIR}/html
cp ${CURDIR}/sign*.html ${TOOLSDIR}/html
echo "signtool -Z nojs.jar -d ${CERTDIR} -p \"nss\" -k objsigner ${TOOLSDIR}/html"
signtool -Z nojs.jar -d ${CERTDIR} -p "nss" -k objsigner ${TOOLSDIR}/html
if [ $? -ne 0 ]; then
   SIGNFAILED=${SIGNFAILED-"Sign files in directory"}
fi
if [ -n "${SIGNFAILED}" ]; then
    echo "<TR><TD>Signing a set of files</TD><TD bgcolor=red>Failed ($SIGNFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Signing a set of files</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "signtool -w nojs.jar -d ${CERTDIR}"
signtool -w nojs.jar -d ${CERTDIR}
if [ $? -ne 0 ]; then
   SIGNFAILED=${SIGNFAILED-"Show files in jar"}
fi
if [ -n "${SIGNFAILED}" ]; then
    echo "<TR><TD>Listing signed files</TD><TD bgcolor=red>Failed ($SIGNFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Listing signed files</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "signtool -w nojs.jar -d ${CERTDIR}"
signtool -w nojs.jar -d ${CERTDIR}
if [ $? -ne 0 ]; then
   SIGNFAILED=${SIGNFAILED-"Check signer"}
fi
if [ -n "${SIGNFAILED}" ]; then
    echo "<TR><TD>Show who signed jar</TD><TD bgcolor=red>Failed ($SIGNFAILED)</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Show who signed jar</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "</TABLE><BR>" >> ${RESULTS}

if [ "$SONMI_DEBUG" != "ON"  -a -n "$TEMPFILES" ]
then
	rm -f ${TEMPFILES}
fi
cd ${CURDIR}

echo "</BODY></HTML>" >> ${RESULTS}
