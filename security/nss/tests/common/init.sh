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
#echo "LD_LIBRARY_PATH SHLIB_PATH LIBPATH=$LD_LIBRARY_PATH"

if [ ! -d ${TESTDIR} ]; then
	echo "Creating ${TESTDIR}"
   mkdir -p ${TESTDIR}
fi

if [ -z "${HOST}" ]; then 
  echo "HOST environment variable is not defined."; exit 1
fi
if [ -z "${DOMSUF}" ]; then 
	DOMSUF=`domainname`
	export DOMSUF
	if  [ -z "${DOMSUF}" ]; then
  		echo "DOMSUF environment variable is not defined."; exit 1
	fi
fi

#if [ ! -s "${HOSTDIR}" ]; then -s means different things to different tests...
if [ ! -d "${HOSTDIR}" ]; then
	echo "No hostdir"
    if [ -f ${TESTDIR}/${HOST} ]; then
		version=`cat ${TESTDIR}/${HOST}`
	else
    	version=1
    fi
	if [ -z "${version}" ]; then 	# for some starnge reason this file 
									# gets truncated at times...
		for w in `ls -d ${TESTDIR}/${HOST}.[0-9]* 2>/dev/null | 
			sort -t '.' -n | sed -e "s/.*${HOST}.//"` ; do  
			version=`expr $w + 1`
		done
		if [ -z "${version}" ]; then
			version=1
		fi
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

