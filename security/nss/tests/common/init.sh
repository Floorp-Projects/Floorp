#! /bin/sh
#
#  Initialize a bunch of variables that may tests would be interested in
#
#

mozilla_root=`(cd ../../../..; pwd)`
MOZILLA_ROOT=${MOZILLA_ROOT-$mozilla_root}

common=`(cd ../common; pwd)`
COMMON=${TEST_COMMON-$common}

qascript_dir=`(cd ..; pwd)`
QASCRIPT_DIR=${QASCRIPT_DIR-$qascript_dir}
export QASCRIPT_DIR

DIST=${DIST-${MOZILLA_ROOT}/dist}
SECURITY_ROOT=${SECURITY_ROOT-${MOZILLA_ROOT}/security/nss}
TESTDIR=${TESTDIR-${MOZILLA_ROOT}/tests_results/security}
OBJDIR=`cd ../common; gmake objdir_name` 
OS_ARCH=`cd ../common; gmake os_arch`

if [ ${OS_ARCH} = "WINNT" ]; then
	PATH=${DIST}/${OBJDIR}/bin\;${DIST}/${OBJDIR}/lib\;$PATH
else
	PATH=${DIST}/${OBJDIR}/bin:${DIST}/${OBJDIR}/lib:$PATH
fi
PATH=`perl $QASCRIPT_DIR/path_uniq -d ';' "$PATH"`
export PATH

LD_LIBRARY_PATH=${DIST}/${OBJDIR}/lib
SHLIB_PATH=${DIST}/${OBJDIR}/lib
LIBPATH=${DIST}/${OBJDIR}/lib
export LD_LIBRARY_PATH SHLIB_PATH LIBPATH
echo "LD_LIBRARY_PATH SHLIB_PATH LIBPATH=$LD_LIBRARY_PATH"

echo "Creating ${TESTDIR}"
if [ ! -d ${TESTDIR} ]; then
   mkdir -p ${TESTDIR}
fi

if [ -z "${HOST}" ]; then 
  echo "HOST environment variable is not defined."; exit 1
fi
if [ -z "${DOMSUF}" ]; then 
  echo "DOMSUF environment variable is not defined."; exit 1
fi

if [ ! -s "${HOSTDIR}" ]; then
    version=1
    if [ -f ${TESTDIR}/${HOST} ]; then
		version=`cat ${TESTDIR}/${HOST}`
    fi
    expr $version + 1 > ${TESTDIR}/${HOST}

    HOSTDIR=${TESTDIR}/${HOST}'.'$version
fi

if [ ! -d ${HOSTDIR} ]; then
   mkdir -p ${HOSTDIR}
fi

RESULTS=${HOSTDIR}/results.html
if [ ! -f "${RESULTS}" ]; then

	cp ${COMMON}/results_header.html ${RESULTS}
	echo "<H4>Platform: ${OBJDIR}<BR>" >> ${RESULTS}
	echo "Test Run: ${HOST}.$version</H4>" >> ${RESULTS}
	echo "<HR><BR>" >> ${RESULTS}

	echo "********************************************"
	echo "   Platform: ${OBJDIR}"
	echo "   Results: ${HOST}.$version"
	echo "********************************************"
fi


KILL="kill"
if  [ ${OS_ARCH} = "Linux" ]; then
        SLEEP="sleep 30"
fi

export  KILL

INIT_SOURCED=TRUE

