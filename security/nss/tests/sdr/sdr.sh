#! /bin/ksh  
#
# This is just a quick script so we can still run our testcases.
# Longer term we need a scriptable test environment..
#
. ../common/init.sh
CURDIR=`pwd`

#temporary files
VALUE1=/tmp/tests.v1.$$
VALUE2=/tmp/tests.v2.$$

TEMPFILES="${VALUE1} ${VALUE2} cert7.db key3.db"

#
# should also try to kill any running server
#
trap "rm -f ${TEMPFILES};  exit"  2 3

T1=Test1
T2="The quick brown fox jumped over the lazy dog"

SDRDIR=${HOSTDIR}/SDR
if [ ! -d ${SDRDIR} ]; then
  mkdir -p ${SDRDIR}
fi

cd ${SDRDIR}
echo "<TABLE BORDER=1><TR><TH COLSPAN=3>SDR Tests</TH></TR>" >> ${RESULTS}
echo "<TR><TH width=500>Test Case</TH><TH width=50>Result</TH></TR>" >> ${RESULTS}
echo "********************** Creating an SDR key/Encrypt **********************"
echo "sdrtest -d . -o ${VALUE1} -t Test1"
sdrtest -d . -o ${VALUE1} -t Test1

if [ $? -ne 0 ]; then
    echo "<TR><TD>Creating SDR Key</TD><TD bgcolor=red>Failed</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Creating SDR Key</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "**************** SDR Encrypt - Second Value ****************"
echo "sdrtest -d . -o ${VALUE2} -t '${T2}'"
sdrtest -d . -o ${VALUE2} -t "${T2}"

if [ $? -ne 0 ]; then
    echo "<TR><TD>Encrypt - Value 2</TD><TD bgcolor=red>Failed</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Encrypt - Value 2</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "***** Decrypt - Value 1 *****"
echo "sdrtest -d . -i ${VALUE1} -t Test1"
sdrtest -d . -i ${VALUE1} -t Test1
if [ $? -ne 0 ]; then
    echo "<TR><TD>Decrypt - Value 1</TD><TD bgcolor=red>Failed</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Decrypt - Value 1</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "***** Decrypt - Value 2 *****"
echo "sdrtest -d . -i ${VALUE2} -t ${T2}"
sdrtest -d . -i ${VALUE2} -t "${T2}"
if [ $? -ne 0 ]; then
    echo "<TR><TD>Decrypt - Value 2</TD><TD bgcolor=red>Failed</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Decrypt - Value 2</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "</TABLE><BR>" >> ${RESULTS}

rm -f ${TEMPFILES}
