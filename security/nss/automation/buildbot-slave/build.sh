#! /bin/bash

# Ensure a failure of the first command inside a pipe
# won't be hidden by commands later in the pipe.
# (e.g. as in ./dosomething | grep)

set -o pipefail

proc_args()
{
    while [ -n "$1" ]; do
        OPT=$(echo $1 | cut -d= -f1)
        VAL=$(echo $1 | cut -d= -f2)

        case $OPT in
            "--build-nss")
                BUILD_NSS=1
                ;;
            "--test-nss")
                TEST_NSS=1
                ;;
            "--check-abi")
                CHECK_ABI=1
                ;;
            "--build-jss")
                BUILD_JSS=1
                ;;
            "--test-jss")
                TEST_JSS=1
                ;;
            "--memtest")
                NSS_TESTS="memleak"
                export NSS_TESTS
                ;;
            "--nojsssign")
                NO_JSS_SIGN=1
                ;;
            *)
                echo "Usage: $0 ..."
                echo "    --memtest   - run the memory leak tests"
                echo "    --nojsssign - try to sign jss"
                echo "    --build-nss"
                echo "    --build-jss"
                echo "    --test-nss"
                echo "    --test-jss"
                echo "    --check-abi"
                exit 1
                ;;
        esac 

        shift
    done
}

set_env()
{
    TOPDIR=$(pwd)
    HGDIR=$(pwd)$(echo "/hg")
    OUTPUTDIR=$(pwd)$(echo "/output")
    LOG_ALL="${OUTPUTDIR}/all.log"
    LOG_TMP="${OUTPUTDIR}/tmp.log"

    echo "hello" |grep --line-buffered hello >/dev/null 2>&1
    [ $? -eq 0 ] && GREP_BUFFER="--line-buffered"
}

print_log()
{
    DATE=$(date "+TB [%Y-%m-%d %H:%M:%S]")
    echo "${DATE} $*"
    echo "${DATE} $*" >> ${LOG_ALL}
}

print_result()
{
    TESTNAME=$1
    RET=$2
    EXP=$3

    if [ ${RET} -eq ${EXP} ]; then
        print_log "${TESTNAME} PASSED"
    else
        print_log "${TESTNAME} FAILED"
    fi
}

print_env()
{
    print_log "######## Environment variables ########"

    uname -a | tee -a ${LOG_ALL}
    if [ -e "/etc/redhat-release" ]; then
        cat "/etc/redhat-release" | tee -a ${LOG_ALL}
    fi
    # don't print the MAIL command, it might contain a password    
    env | grep -v "^MAIL=" | tee -a ${LOG_ALL}
}

set_cycle()
{
    BITS=$1
    OPT=$2

    if [ "${BITS}" = "64" ]; then
        USE_64=1
        JAVA_HOME=${JAVA_HOME_64} 
        PORT_DBG=${PORT_64_DBG}
        PORT_OPT=${PORT_64_OPT}
    else
        USE_64=
        JAVA_HOME=${JAVA_HOME_32} 
        PORT_DBG=${PORT_32_DBG}
        PORT_OPT=${PORT_32_OPT}
    fi
    export USE_64
    export JAVA_HOME

    BUILD_OPT=
    if [ "${OPT}" = "OPT" ]; then
        BUILD_OPT=1
        XPCLASS=xpclass.jar
        PORT=${PORT_OPT}
    else
        BUILD_OPT=
        XPCLASS=xpclass_dbg.jar
        PORT=${PORT_DBG}
    fi
    export BUILD_OPT

    PORT_JSS_SERVER=$(expr ${PORT} + 20)
    PORT_JSSE_SERVER=$(expr ${PORT} + 40)

    export PORT
    export PORT_JSS_SERVER
    export PORT_JSSE_SERVER
}

build_nss()
{
    print_log "######## NSS - build - ${BITS} bits - ${OPT} ########"

    print_log "$ cd ${HGDIR}/nss"
    cd ${HGDIR}/nss

    print_log "$ ${MAKE} ${NSS_BUILD_TARGET}"
    #${MAKE} ${NSS_BUILD_TARGET} 2>&1 | tee -a ${LOG_ALL} | grep ${GREP_BUFFER} "^${MAKE}"
    ${MAKE} ${NSS_BUILD_TARGET} 2>&1 | tee -a ${LOG_ALL}
    RET=$?
    print_result "NSS - build - ${BITS} bits - ${OPT}" ${RET} 0

    if [ ${RET} -eq 0 ]; then
        return 0
    else
        tail -100 ${LOG_ALL}
        return ${RET}
    fi
}

build_jss()
{
    print_log "######## JSS - build - ${BITS} bits - ${OPT} ########"

    print_log "$ cd ${HGDIR}/jss"
    cd ${HGDIR}/jss

    print_log "$ ${MAKE} ${JSS_BUILD_TARGET}"
    #${MAKE} ${JSS_BUILD_TARGET} 2>&1 | tee -a ${LOG_ALL} | grep ${GREP_BUFFER} "^${MAKE}"
    ${MAKE} ${JSS_BUILD_TARGET} 2>&1 | tee -a ${LOG_ALL}
    RET=$?
    print_result "JSS build - ${BITS} bits - ${OPT}" ${RET} 0
    [ ${RET} -eq 0 ] || return ${RET}

    print_log "$ cd ${HGDIR}/dist"
    cd ${HGDIR}/dist

    if [ -z "${NO_JSS_SIGN}" ]; then
	print_log "cat ${TOPDIR}/keystore.pw | ${JAVA_HOME}/bin/jarsigner -keystore ${TOPDIR}/keystore -internalsf ${XPCLASS} jssdsa"
	cat ${TOPDIR}/keystore.pw | ${JAVA_HOME}/bin/jarsigner -keystore ${TOPDIR}/keystore -internalsf ${XPCLASS} jssdsa >> ${LOG_ALL} 2>&1
	RET=$?
	print_result "JSS - sign JAR files - ${BITS} bits - ${OPT}" ${RET} 0
	[ ${RET} -eq 0 ] || return ${RET}
    fi
    print_log "${JAVA_HOME}/bin/jarsigner -verify -certs ${XPCLASS}"
    ${JAVA_HOME}/bin/jarsigner -verify -certs ${XPCLASS} >> ${LOG_ALL} 2>&1
    RET=$?
    print_result "JSS - verify JAR files - ${BITS} bits - ${OPT}" ${RET} 0
    [ ${RET} -eq 0 ] || return ${RET}

    return 0
}

test_nss()
{
    print_log "######## NSS - tests - ${BITS} bits - ${OPT} ########"

    if [ "${OS_TARGET}" = "Android" ]; then
	print_log "$ cd ${HGDIR}/nss/tests/remote"
	cd ${HGDIR}/nss/tests/remote
	print_log "$ make test_android"
	make test_android 2>&1 | tee ${LOG_TMP} | grep ${GREP_BUFFER} ": #"
	OUTPUTFILE=${HGDIR}/tests_results/security/*.1/output.log
    else
	print_log "$ cd ${HGDIR}/nss/tests"
	cd ${HGDIR}/nss/tests
	print_log "$ ./all.sh"
	./all.sh 2>&1 | tee ${LOG_TMP} | egrep ${GREP_BUFFER} ": #|^\[.{10}\] "
	OUTPUTFILE=${LOG_TMP}
    fi

    cat ${LOG_TMP} >> ${LOG_ALL}
    tail -n2 ${HGDIR}/tests_results/security/*.1/results.html | grep END_OF_TEST >> ${LOG_ALL}
    RET=$?

    print_log "######## details of detected failures (if any) ########"
    grep -B50 FAILED ${OUTPUTFILE}
    [ $? -eq 1 ] || RET=1

    print_result "NSS - tests - ${BITS} bits - ${OPT}" ${RET} 0
    return ${RET}
}

check_abi()
{
    print_log "######## NSS ABI CHECK - ${BITS} bits - ${OPT} ########"
    print_log "######## creating temporary HG clones ########"

    rm -rf ${HGDIR}/baseline
    mkdir ${HGDIR}/baseline
    BASE_NSS=`cat ${HGDIR}/nss/automation/abi-check/previous-nss-release`
    hg clone -u "${BASE_NSS}" "${HGDIR}/nss" "${HGDIR}/baseline/nss"
    if [ $? -ne 0 ]; then
        echo "invalid tag in automation/abi-check/previous-nss-release"
        return 1
    fi

    BASE_NSPR=NSPR_$(head -1 ${HGDIR}/baseline/nss/automation/release/nspr-version.txt | cut -d . -f 1-2 | tr . _)_BRANCH
    hg clone -u "${BASE_NSPR}" "${HGDIR}/nspr" "${HGDIR}/baseline/nspr"
    if [ $? -ne 0 ]; then
        echo "nonexisting tag ${BASE_NSPR} derived from ${BASE_NSS} automation/release/nspr-version.txt"
        # Assume that version hasn't been released yet, fall back to trunk
        pushd "${HGDIR}/baseline/nspr"
        hg update default
        popd
    fi

    print_log "######## building baseline NSPR/NSS ########"
    pushd ${HGDIR}/baseline/nss

    print_log "$ ${MAKE} ${NSS_BUILD_TARGET}"
    ${MAKE} ${NSS_BUILD_TARGET} 2>&1 | tee -a ${LOG_ALL}
    RET=$?
    print_result "NSS - build - ${BITS} bits - ${OPT}" ${RET} 0
    if [ ${RET} -ne 0 ]; then
        tail -100 ${LOG_ALL}
        return ${RET}
    fi
    popd

    ABI_PROBLEM_FOUND=0
    ABI_REPORT=${OUTPUTDIR}/abi-diff.txt
    rm -f ${ABI_REPORT}
    PREVDIST=${HGDIR}/baseline/dist
    NEWDIST=${HGDIR}/dist
    ALL_SOs="libfreebl3.so libfreeblpriv3.so libnspr4.so libnss3.so libnssckbi.so libnssdbm3.so libnsssysinit.so libnssutil3.so libplc4.so libplds4.so libsmime3.so libsoftokn3.so libssl3.so"
    for SO in ${ALL_SOs}; do
        if [ ! -f ${HGDIR}/nss/automation/abi-check/expected-report-$SO.txt ]; then
            touch ${HGDIR}/nss/automation/abi-check/expected-report-$SO.txt
        fi
        abidiff --hd1 $PREVDIST/public/ --hd2 $NEWDIST/public \
            $PREVDIST/*/lib/$SO $NEWDIST/*/lib/$SO \
            > ${HGDIR}/nss/automation/abi-check/new-report-$SO.txt
        RET=$?
        ABIDIFF_ERROR=$((($RET & 0x01) != 0))
        ABIDIFF_USAGE_ERROR=$((($RET & 0x02) != 0))
        ABIDIFF_ABI_CHANGE=$((($RET & 0x04) != 0))
        ABIDIFF_ABI_INCOMPATIBLE_CHANGE=$((($RET & 0x08) != 0))
        ABIDIFF_UNKNOWN_BIT_SET=$((($RET & 0xf0) != 0))

        # If abidiff reports an error, or a usage error, or if it sets a result
        # bit value this script doesn't know yet about, we'll report failure.
        # For ABI changes, we don't yet report an error. We'll compare the
        # result report with our whitelist. This allows us to silence changes
        # that we're already aware of and have been declared acceptable.

        REPORT_RET_AS_FAILURE=0
        if [ $ABIDIFF_ERROR -ne 0 ]; then
            print_log "abidiff reported ABIDIFF_ERROR."
            REPORT_RET_AS_FAILURE=1
        fi
        if [ $ABIDIFF_USAGE_ERROR -ne 0 ]; then
            print_log "abidiff reported ABIDIFF_USAGE_ERROR."
            REPORT_RET_AS_FAILURE=1
        fi
        if [ $ABIDIFF_UNKNOWN_BIT_SET -ne 0 ]; then
            print_log "abidiff reported ABIDIFF_UNKNOWN_BIT_SET."
            REPORT_RET_AS_FAILURE=1
        fi

        if [ $ABIDIFF_ABI_CHANGE -ne 0 ]; then
            print_log "Ignoring abidiff result ABI_CHANGE, instead we'll check for non-whitelisted differences."
        fi
        if [ $ABIDIFF_ABI_INCOMPATIBLE_CHANGE -ne 0 ]; then
            print_log "Ignoring abidiff result ABIDIFF_ABI_INCOMPATIBLE_CHANGE, instead we'll check for non-whitelisted differences."
        fi

        if [ $REPORT_RET_AS_FAILURE -ne 0 ]; then
            ABI_PROBLEM_FOUND=1
            print_log "abidiff {$PREVDIST , $NEWDIST} for $SO FAILED with result $RET, or failed writing to ${HGDIR}/nss/automation/abi-check/new-report-$SO.txt"
        fi
        if [ ! -f ${HGDIR}/nss/automation/abi-check/expected-report-$SO.txt ]; then
            ABI_PROBLEM_FOUND=1
            print_log "FAILED to access report file: ${HGDIR}/nss/automation/abi-check/expected-report-$SO.txt"
        fi

        diff -wB -u ${HGDIR}/nss/automation/abi-check/expected-report-$SO.txt \
                ${HGDIR}/nss/automation/abi-check/new-report-$SO.txt >> ${ABI_REPORT}
        if [ ! -f ${ABI_REPORT} ]; then
            ABI_PROBLEM_FOUND=1
            print_log "FAILED to compare exepcted and new report: ${HGDIR}/nss/automation/abi-check/new-report-$SO.txt"
        fi
    done

    if [ -s ${ABI_REPORT} ]; then
        print_log "FAILED: there are new unexpected ABI changes"
        cat ${ABI_REPORT}
        return 1
    elif [ $ABI_PROBLEM_FOUND -ne 0 ]; then
        print_log "FAILED: failure executing the ABI checks"
        cat ${ABI_REPORT}
        return 1
    fi

    return 0
}

test_jss()
{
    print_log "######## JSS - tests - ${BITS} bits - ${OPT} ########"

    print_log "$ cd ${HGDIR}/jss"
    cd ${HGDIR}/jss

    print_log "$ ${MAKE} platform"
    PLATFORM=$(${MAKE} platform)
    print_log "PLATFORM=${PLATFORM}"

    print_log "$ cd ${HGDIR}/jss/org/mozilla/jss/tests"
    cd ${HGDIR}/jss/org/mozilla/jss/tests

    print_log "$ perl all.pl dist ${HGDIR}/dist/${PLATFORM}"
    perl all.pl dist ${HGDIR}/dist/${PLATFORM} 2>&1 | tee ${LOG_TMP}
    cat ${LOG_TMP} >> ${LOG_ALL}

    tail -n2 ${LOG_TMP} | grep JSSTEST_RATE > /dev/null
    RET=$?

    grep FAIL ${LOG_TMP} 
    [ $? -eq 1 ] || RET=1

    print_result "JSS - tests - ${BITS} bits - ${OPT}" ${RET} 0
    return ${RET}
}

create_objdir_dist_link()
{
    # compute relevant 'dist' OBJDIR_NAME subdirectory names for JSS and NSS
    OS_TARGET=`uname -s`
    OS_RELEASE=`uname -r | sed 's/-.*//' | sed 's/-.*//' | cut -d . -f1,2`
    CPU_TAG=_`uname -m`
    # OBJDIR_NAME_COMPILER appears to be defined for NSS but not JSS
    OBJDIR_NAME_COMPILER=_cc
    LIBC_TAG=_glibc
    IMPL_STRATEGY=_PTH
    if [ "${RUN_BITS}" = "64" ]; then
        OBJDIR_TAG=_${RUN_BITS}_${RUN_OPT}.OBJ
    else
        OBJDIR_TAG=_${RUN_OPT}.OBJ
    fi

    # define NSS_OBJDIR_NAME
    NSS_OBJDIR_NAME=${OS_TARGET}${OS_RELEASE}${CPU_TAG}${OBJDIR_NAME_COMPILER}
    NSS_OBJDIR_NAME=${NSS_OBJDIR_NAME}${LIBC_TAG}${IMPL_STRATEGY}${OBJDIR_TAG}
    print_log "create_objdir_dist_link(): NSS_OBJDIR_NAME='${NSS_OBJDIR_NAME}'"

    # define JSS_OBJDIR_NAME
    JSS_OBJDIR_NAME=${OS_TARGET}${OS_RELEASE}${CPU_TAG}
    JSS_OBJDIR_NAME=${JSS_OBJDIR_NAME}${LIBC_TAG}${IMPL_STRATEGY}${OBJDIR_TAG}
    print_log "create_objdir_dist_link(): JSS_OBJDIR_NAME='${JSS_OBJDIR_NAME}'"

    if [ -e "${HGDIR}/dist/${NSS_OBJDIR_NAME}" ]; then
        SOURCE=${HGDIR}/dist/${NSS_OBJDIR_NAME}
        TARGET=${HGDIR}/dist/${JSS_OBJDIR_NAME}
        ln -s ${SOURCE} ${TARGET} >/dev/null 2>&1
    fi
}

build_and_test()
{
    if [ -n "${BUILD_NSS}" ]; then
        build_nss
        [ $? -eq 0 ] || return 1
    fi

    if [ -n "${TEST_NSS}" ]; then
        test_nss
        [ $? -eq 0 ] || return 1
    fi

    if [ -n "${CHECK_ABI}" ]; then
        check_abi
        [ $? -eq 0 ] || return 1
    fi

    if [ -n "${BUILD_JSS}" ]; then
        create_objdir_dist_link
        build_jss
        [ $? -eq 0 ] || return 1
    fi

    if [ -n "${TEST_JSS}" ]; then
        test_jss
        [ $? -eq 0 ] || return 1
    fi

    return 0
}

run_cycle()
{
    print_env
    build_and_test
    RET=$?

    grep ^TinderboxPrint ${LOG_ALL}

    return ${RET}
}

prepare()
{
    rm -rf ${OUTPUTDIR}.oldest >/dev/null 2>&1
    mv ${OUTPUTDIR}.older ${OUTPUTDIR}.oldest >/dev/null 2>&1
    mv ${OUTPUTDIR}.old ${OUTPUTDIR}.older >/dev/null 2>&1
    mv ${OUTPUTDIR}.last ${OUTPUTDIR}.old >/dev/null 2>&1
    mv ${OUTPUTDIR} ${OUTPUTDIR}.last >/dev/null 2>&1
    mkdir -p ${OUTPUTDIR}

    # Remove temporary test files from previous jobs, that weren't cleaned up
    # by move_results(), e.g. caused by unexpected interruptions.
    rm -rf ${HGDIR}/tests_results/

    cd ${HGDIR}/nss

    if [ -n "${FEWER_STRESS_ITERATIONS}" ]; then
        sed -i 's/-c_1000_/-c_500_/g' tests/ssl/sslstress.txt
    fi

    return 0
}

move_results()
{
    cd ${HGDIR}
    if [ -n "${TEST_NSS}" ]; then
	mv -f tests_results ${OUTPUTDIR}
    fi
    tar -c -z --dereference -f ${OUTPUTDIR}/dist.tgz dist
    rm -rf dist
}

run_all()
{
    set_cycle ${BITS} ${OPT}
    prepare
    run_cycle
    RESULT=$?
    print_log "### result of run_cycle is ${RESULT}"
    move_results
    return ${RESULT}
}

main()
{
    VALID=0
    RET=1
    FAIL=0

    for BITS in 32 64; do
        echo ${RUN_BITS} | grep ${BITS} > /dev/null
        [ $? -eq 0 ] || continue
        for OPT in DBG OPT; do
            echo ${RUN_OPT} | grep ${OPT} > /dev/null
            [ $? -eq 0 ] || continue

            VALID=1
            set_env
            run_all
            RET=$?
            print_log "### result of run_all is ${RET}"
            if [ ${RET} -ne 0 ]; then
                FAIL=${RET}
            fi
        done
    done

    if [ ${VALID} -ne 1 ]; then
        echo "Need to set valid bits/opt values."
        return 1
    fi

    return ${FAIL}
}

#function killallsub()
#{
#    FINAL_RET=$?
#    for proc in `jobs -p`
#    do
#        kill -9 $proc
#    done
#    return ${FINAL_RET}
#}
#trap killallsub EXIT

#IS_RUNNING_FILE="./build-is-running"

#if [ -a $IS_RUNNING_FILE ]; then
#    echo "exiting, because old job is still running"
#    exit 1
#fi

#touch $IS_RUNNING_FILE

echo "tinderbox args: $0 $@"
. ${ENVVARS}
proc_args "$@"
main

RET=$?
print_log "### result of main is ${RET}"

#rm $IS_RUNNING_FILE
exit ${RET}
