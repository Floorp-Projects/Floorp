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

    if [ -n "${BUILD_JSS}" ]; then
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
        done
    done

    if [ ${VALID} -ne 1 ]; then
        echo "Need to set valid bits/opt values."
        return 1
    fi

    return ${RET}
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

#RET=$?
#rm $IS_RUNNING_FILE
#exit ${RET}
