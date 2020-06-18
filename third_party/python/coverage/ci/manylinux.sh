#!/bin/bash
# From: https://github.com/pypa/python-manylinux-demo/blob/master/travis/build-wheels.sh
# which is in the public domain.
#
# This is run inside a CentOS 5 virtual machine to build manylinux wheels:
#
#   $ docker run -v `pwd`:/io quay.io/pypa/manylinux1_x86_64 /io/ci/build_manylinux.sh
#

set -e -x

action=$1
shift

if [[ $action == "build" ]]; then
    # Compile wheels
    cd /io
    for PYBIN in /opt/python/*/bin; do
        if [[ $PYBIN == *cp34* ]]; then
            # manylinux docker images have Python 3.4, but we don't use it.
            continue
        fi
        "$PYBIN/pip" install -r requirements/wheel.pip
        "$PYBIN/python" setup.py clean -a
        "$PYBIN/python" setup.py bdist_wheel -d ~/wheelhouse/
    done
    cd ~

    # Bundle external shared libraries into the wheels
    for whl in wheelhouse/*.whl; do
        auditwheel repair "$whl" -w /io/dist/
    done

elif [[ $action == "test" ]]; then
    # Create "pythonX.Y" links
    for PYBIN in /opt/python/*/bin/; do
        if [[ $PYBIN == *cp34* ]]; then
            # manylinux docker images have Python 3.4, but we don't use it.
            continue
        fi
        PYNAME=$("$PYBIN/python" -c "import sys; print('python{0[0]}.{0[1]}'.format(sys.version_info))")
        ln -sf "$PYBIN/$PYNAME" /usr/local/bin/$PYNAME
    done

    # Install packages and test
    TOXBIN=/opt/python/cp36-cp36m/bin
    "$TOXBIN/pip" install -r /io/requirements/tox.pip

    cd /io
    export PYTHONPYCACHEPREFIX=/opt/pyc
    if [[ $1 == "meta" ]]; then
        shift
        export COVERAGE_COVERAGE=yes
    fi
    TOXWORKDIR=.tox/linux "$TOXBIN/tox" "$@" || true
    cd ~

else
    echo "Need an action to perform!"
fi
