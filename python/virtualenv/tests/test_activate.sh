#!/bin/sh

set -u

ROOT="$(dirname $0)/.."
VIRTUALENV="${ROOT}/virtualenv.py"
TESTENV="/tmp/test_virtualenv_activate.venv"

rm -rf ${TESTENV}

echo "$0: Creating virtualenv ${TESTENV}..." 1>&2

${VIRTUALENV} ${TESTENV} | tee ${ROOT}/tests/test_activate_output.actual
if ! diff ${ROOT}/tests/test_activate_output.expected ${ROOT}/tests/test_activate_output.actual; then
    echo "$0: Failed to get expected output from ${VIRTUALENV}!" 1>&2
    exit 1
fi

echo "$0: Created virtualenv ${TESTENV}." 1>&2

echo "$0: Activating ${TESTENV}..." 1>&2
. ${TESTENV}/bin/activate
echo "$0: Activated ${TESTENV}." 1>&2

echo "$0: Checking value of \$VIRTUAL_ENV..." 1>&2

if [ "$VIRTUAL_ENV" != "${TESTENV}" ]; then
    echo "$0: Expected \$VIRTUAL_ENV to be set to \"${TESTENV}\"; actual value: \"${VIRTUAL_ENV}\"!" 1>&2
    exit 2
fi

echo "$0: \$VIRTUAL_ENV = \"${VIRTUAL_ENV}\" -- OK." 1>&2

echo "$0: Checking output of \$(which python)..." 1>&2

if [ "$(which python)" != "${TESTENV}/bin/python" ]; then
    echo "$0: Expected \$(which python) to return \"${TESTENV}/bin/python\"; actual value: \"$(which python)\"!" 1>&2
    exit 3
fi

echo "$0: Output of \$(which python) is OK." 1>&2

echo "$0: Checking output of \$(which pip)..." 1>&2

if [ "$(which pip)" != "${TESTENV}/bin/pip" ]; then
    echo "$0: Expected \$(which pip) to return \"${TESTENV}/bin/pip\"; actual value: \"$(which pip)\"!" 1>&2
    exit 4
fi

echo "$0: Output of \$(which pip) is OK." 1>&2

echo "$0: Checking output of \$(which easy_install)..." 1>&2

if [ "$(which easy_install)" != "${TESTENV}/bin/easy_install" ]; then
    echo "$0: Expected \$(which easy_install) to return \"${TESTENV}/bin/easy_install\"; actual value: \"$(which easy_install)\"!" 1>&2
    exit 5
fi

echo "$0: Output of \$(which easy_install) is OK." 1>&2

echo "$0: Executing a simple Python program..." 1>&2

TESTENV=${TESTENV} python <<__END__
import os, sys

expected_site_packages = os.path.join(os.environ['TESTENV'], 'lib','python%s' % sys.version[:3], 'site-packages')
site_packages = os.path.join(os.environ['VIRTUAL_ENV'], 'lib', 'python%s' % sys.version[:3], 'site-packages')

assert site_packages == expected_site_packages, 'site_packages did not have expected value; actual value: %r' % site_packages

open(os.path.join(site_packages, 'pydoc_test.py'), 'w').write('"""This is pydoc_test.py"""\n')
__END__

if [ $? -ne 0 ]; then
    echo "$0: Python script failed!" 1>&2
    exit 6
fi

echo "$0: Execution of a simple Python program -- OK." 1>&2

echo "$0: Testing pydoc..." 1>&2

if ! PAGER=cat pydoc pydoc_test | grep 'This is pydoc_test.py' > /dev/null; then
    echo "$0: pydoc test failed!" 1>&2
    exit 7
fi

echo "$0: pydoc is OK." 1>&2

echo "$0: Deactivating ${TESTENV}..." 1>&2
deactivate
echo "$0: Deactivated ${TESTENV}." 1>&2
echo "$0: OK!" 1>&2

rm -rf ${TESTENV}

