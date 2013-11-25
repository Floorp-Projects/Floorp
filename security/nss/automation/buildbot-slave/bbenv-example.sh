#! /bin/bash

# Each buildbot-slave requires a bbenv.sh file that defines
# machine specific variables. This is an example file.


HOST=$(hostname | cut -d. -f1)
export HOST

# if your machine's IP isn't registered in DNS,
# you must set appropriate environment variables
# that can be resolved locally.
# For example, if localhost.localdomain works on your system, set:
#HOST=localhost
#DOMSUF=localdomain
#export DOMSUF

ARCH=$(uname -s)

ulimit -c unlimited 2> /dev/null

export NSS_ENABLE_ECC=1
export NSS_ECC_MORE_THAN_SUITE_B=1
export NSPR_LOG_MODULES="pkix:1"

#export JAVA_HOME_32=
#export JAVA_HOME_64=

#enable if you have PKITS data
#export PKITS_DATA=$HOME/pkits/data/

NSS_BUILD_TARGET="clean nss_build_all"
JSS_BUILD_TARGET="clean all"

MAKE=gmake
AWK=awk
PATCH=patch

if [ "${ARCH}" = "SunOS" ]; then
    AWK=nawk
    PATCH=gpatch
    ARCH=SunOS/$(uname -p)
fi

if [ "${ARCH}" = "Linux" -a -f /etc/system-release ]; then
   VERSION=`sed -e 's; release ;;' -e 's; (.*)$;;' -e 's;Red Hat Enterprise Linux Server;RHEL;' -e 's;Red Hat Enterprise Linux Workstation;RHEL;' /etc/system-release`
   ARCH=Linux/${VERSION}
   echo ${ARCH}
fi

PROCESSOR=$(uname -p)
if [ "${PROCESSOR}" = "ppc64" ]; then
    ARCH="${ARCH}/ppc64"
fi
if [ "${PROCESSOR}" = "powerpc" ]; then
    ARCH="${ARCH}/ppc"
fi

PORT_64_DBG=8543
PORT_64_OPT=8544
PORT_32_DBG=8545
PORT_32_OPT=8546

if [ "${NSS_TESTS}" = "memleak" ]; then
    PORT_64_DBG=8547
    PORT_64_OPT=8548
    PORT_32_DBG=8549
    PORT_32_OPT=8550
fi
