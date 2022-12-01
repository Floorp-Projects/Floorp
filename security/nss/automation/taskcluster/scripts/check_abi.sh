#! /bin/bash

set_env()
{
  cd /home/worker
  HGDIR=/home/worker
  OUTPUTDIR=$(pwd)$(echo "/output")
  DATE=$(date "+TB [%Y-%m-%d %H:%M:%S]")

  if [ ! -d "${OUTPUTDIR}" ]; then
    echo "Creating output dir"
    mkdir "${OUTPUTDIR}"
  fi

  if [ ! -d "nspr" ]; then
    for i in 0 2 5; do
      sleep $i
      hg clone -r "default" "https://hg.mozilla.org/projects/nspr" "${HGDIR}/nspr" && break
      rm -rf nspr
    done
  fi

  if [[ -f nss/nspr.patch && "$ALLOW_NSPR_PATCH" == "1" ]]; then
    pushd nspr
    cat ../nss/nspr.patch | patch -p1
    popd
  fi

  cd nss
  ./build.sh -v -c
  cd ..
}

check_abi()
{
  set_env
  set +e #reverses set -e from build.sh to allow possible hg clone failures
  if [[ "$1" != --nobuild ]]; then # Start nobuild block

    echo "######## NSS ABI CHECK ########"
    echo "######## creating temporary HG clones ########"

    rm -rf ${HGDIR}/baseline
    mkdir ${HGDIR}/baseline
    BASE_NSS=`cat ${HGDIR}/nss/automation/abi-check/previous-nss-release`  #Reads the version number of the last release from the respective file
    NSS_CLONE_RESULT=0
    for i in 0 2 5; do
        sleep $i
        hg clone -u "${BASE_NSS}" "https://hg.mozilla.org/projects/nss" "${HGDIR}/baseline/nss"
        if [ $? -eq 0 ]; then
          NSS_CLONE_RESULT=0
          break
        fi
        rm -rf "${HGDIR}/baseline/nss"
        NSS_CLONE_RESULT=1
    done
    if [ ${NSS_CLONE_RESULT} -ne 0 ]; then
      echo "invalid tag in automation/abi-check/previous-nss-release"
      return 1
    fi

    BASE_NSPR=NSPR_$(head -1 ${HGDIR}/baseline/nss/automation/release/nspr-version.txt | cut -d . -f 1-2 | tr . _)_BRANCH
    hg clone -u "${BASE_NSPR}" "https://hg.mozilla.org/projects/nspr" "${HGDIR}/baseline/nspr"
    NSPR_CLONE_RESULT=$?

    if [ ${NSPR_CLONE_RESULT} -ne 0 ]; then
      rm -rf "${HGDIR}/baseline/nspr"
      for i in 0 2 5; do
          sleep $i
          hg clone -u "default" "https://hg.mozilla.org/projects/nspr" "${HGDIR}/baseline/nspr" && break
          rm -rf "${HGDIR}/baseline/nspr"
      done
      echo "Nonexisting tag ${BASE_NSPR} derived from ${BASE_NSS} automation/release/nspr-version.txt"
      echo "Using default branch instead."
    fi

    echo "######## building baseline NSPR/NSS ########"
    echo "${HGDIR}/baseline/nss/build.sh"
    cd ${HGDIR}/baseline/nss
    ./build.sh -v -c
    cd ${HGDIR}
  else  # Else nobuild block
    echo "######## using existing baseline NSPR/NSS build ########"
  fi # End nobuild block

  set +e #reverses set -e from build.sh to allow abidiff failures

  echo "######## Starting abidiff procedure ########"
  abi_diff
}

#Slightly modified from build.sh in this directory
abi_diff()
{
  ABI_PROBLEM_FOUND=0
  ABI_REPORT=${OUTPUTDIR}/abi-diff.txt
  rm -f ${ABI_REPORT}
  PREVDIST=${HGDIR}/baseline/dist
  NEWDIST=${HGDIR}/dist
  # libnssdbm3.so isn't built by default anymore, skip it.
  ALL_SOs="libfreebl3.so libfreeblpriv3.so libnspr4.so libnss3.so libnssckbi.so libnsssysinit.so libnssutil3.so libplc4.so libplds4.so libsmime3.so libsoftokn3.so libssl3.so"
  for SO in ${ALL_SOs}; do
      if [ ! -f ${HGDIR}/nss/automation/abi-check/expected-report-$SO.txt ]; then
          touch ${HGDIR}/nss/automation/abi-check/expected-report-$SO.txt
      fi
      abidiff --hd1 $PREVDIST/public/ --hd2 $NEWDIST/public \
          $PREVDIST/*/lib/$SO $NEWDIST/*/lib/$SO \
          > ${HGDIR}/nss/automation/abi-check/new-report-temp$SO.txt
      RET=$?
      cat ${HGDIR}/nss/automation/abi-check/new-report-temp$SO.txt \
          | grep -v "^Functions changes summary:" \
          | grep -v "^Variables changes summary:" \
          | sed -e 's/__anonymous_enum__[0-9]*/__anonymous_enum__/g' \
          > ${HGDIR}/nss/automation/abi-check/new-report-$SO.txt
      rm -f ${HGDIR}/nss/automation/abi-check/new-report-temp$SO.txt

      ABIDIFF_ERROR=$((($RET & 0x01) != 0))
      ABIDIFF_USAGE_ERROR=$((($RET & 0x02) != 0))
      ABIDIFF_ABI_CHANGE=$((($RET & 0x04) != 0))
      ABIDIFF_ABI_INCOMPATIBLE_CHANGE=$((($RET & 0x08) != 0))
      ABIDIFF_UNKNOWN_BIT_SET=$((($RET & 0xf0) != 0))

      # If abidiff reports an error, or a usage error, or if it sets a result
      # bit value this script doesn't know yet about, we'll report failure.
      # For ABI changes, we don't yet report an error. We'll compare the
      # result report with our allowlist. This allows us to silence changes
      # that we're already aware of and have been declared acceptable.

      REPORT_RET_AS_FAILURE=0
      if [ $ABIDIFF_ERROR -ne 0 ]; then
          echo "abidiff reported ABIDIFF_ERROR."
          REPORT_RET_AS_FAILURE=1
      fi
      if [ $ABIDIFF_USAGE_ERROR -ne 0 ]; then
          echo "abidiff reported ABIDIFF_USAGE_ERROR."
          REPORT_RET_AS_FAILURE=1
      fi
      if [ $ABIDIFF_UNKNOWN_BIT_SET -ne 0 ]; then
          echo "abidiff reported ABIDIFF_UNKNOWN_BIT_SET."
          REPORT_RET_AS_FAILURE=1
      fi

      if [ $ABIDIFF_ABI_CHANGE -ne 0 ]; then
          echo "Ignoring abidiff result ABI_CHANGE, instead we'll check for non-allowlisted differences."
      fi
      if [ $ABIDIFF_ABI_INCOMPATIBLE_CHANGE -ne 0 ]; then
          echo "Ignoring abidiff result ABIDIFF_ABI_INCOMPATIBLE_CHANGE, instead we'll check for non-allowlisted differences."
      fi

      if [ $REPORT_RET_AS_FAILURE -ne 0 ]; then
          ABI_PROBLEM_FOUND=1
          echo "abidiff {$PREVDIST , $NEWDIST} for $SO FAILED with result $RET, or failed writing to ${HGDIR}/nss/automation/abi-check/new-report-$SO.txt"
      fi
      if [ ! -f ${HGDIR}/nss/automation/abi-check/expected-report-$SO.txt ]; then
          ABI_PROBLEM_FOUND=1
          echo "FAILED to access report file: ${HGDIR}/nss/automation/abi-check/expected-report-$SO.txt"
      fi

      diff -wB -u ${HGDIR}/nss/automation/abi-check/expected-report-$SO.txt \
              ${HGDIR}/nss/automation/abi-check/new-report-$SO.txt >> ${ABI_REPORT}
      if [ ! -f ${ABI_REPORT} ]; then
          ABI_PROBLEM_FOUND=1
          echo "FAILED to compare exepcted and new report: ${HGDIR}/nss/automation/abi-check/new-report-$SO.txt"
      fi
  done

  if [ -s ${ABI_REPORT} ]; then
      echo "FAILED: there are new unexpected ABI changes"
      cat ${ABI_REPORT}
      return 1
  elif [ $ABI_PROBLEM_FOUND -ne 0 ]; then
      echo "FAILED: failure executing the ABI checks"
      cat ${ABI_REPORT}
      return 1
  fi

  return 0
}

check_abi $1
