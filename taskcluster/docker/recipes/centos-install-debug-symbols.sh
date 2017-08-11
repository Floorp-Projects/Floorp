#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Install debuginfo packages

# enable debug info repository
yum-config-manager enable debuginfo

install_options="-q -t -y --skip-broken"
debuginfo_install="debuginfo-install $install_options"

install_debuginfo_for_installed_packages() {
    yum list installed \
        | awk 'NF >= 3 && $1 !~ /debuginfo/ {
                # Remove arch suffix
                print gensub(/\.(i.86|x86_64|noarch)/, "", "", $1)
            }' \
        | xargs $debuginfo_install \
        || : # ignore errors
}

echo "Installing debuginfo packages..."
install_debuginfo_for_installed_packages > /dev/null

# Now search for debuginfo for individual libraries in the system

# Get the length of a string in bytes.
# We have to set LANG=C to get the length in bytes, not chars.
strlen() {
    local old_lang byteslen
    old_lang=$LANG
    LANG=C
    byteslen=${#1}
    LANG=$old_lang
    echo $byteslen
}

echo "Searching for additional debuginfo packages..."

# libraries contains the list of libraries found in the system
libraries=""

# As we accumulate libraries in the $libraries variable, we have
# to constantly check we didn't extrapolate the command line
# argument length limit. arg_max stores the argument limit in
# bytes, discounting the $debuginfo_install command plus one
# space.
arg_max=$(( $(getconf ARG_MAX)-$(strlen $debuginfo_install)-$(strlen " ") ))

to_debuginfo() {
    # extracted from debuginfo-install script
    if [[ $1 == *-rpms ]]; then
        echo ${1%*-rpms}-debug-rpms
    else
        echo $1-debuginfo
    fi
}

get_debuginfo_package() {
    local package=${1%.so*}

    # Remove version suffix because some libraries have their debuginfo
    # package without it in the name.
    local unversioned_package=${package%-*}
    if [ "$unversioned_package" != "$package" ]; then
        package="$package $unversioned_package"
    fi

    echo $package
}

walk_dir() {
    local lib
    for i in $1/*; do
        # if we found a library...
        if [[ $i == *.so ]]; then
            lib="$(get_debuginfo_package $(basename $i))"
            if [ $(strlen "$debuginfo_install $libraries $lib") -ge $arg_max ]; then
                $debuginfo_install $libraries > /dev/null
                libraries=""
            fi
            libraries="$libraries $lib"
        fi
    done
}

for i in /usr/lib /usr/lib64 /lib /lib64; do
    walk_dir $i
done

if [ ${#libraries} -gt 0 ]; then
    $debuginfo_install $libraries > /dev/null
fi
