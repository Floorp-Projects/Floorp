# S02solaris_patchchecker.sh
# Note: This script is called "inline" by the "mozilla" shell script.

moz_spc_fatal_error()
{
    echo "$0: Solaris patch checker error: $1"
    moz_startstop_addon_scripts "stop"
    exit 1
}

# This is only for debugging purposes.
moz_spc_verbose_echo()
{
#    echo "$1"
    :
}

moz_spc_verbose_echo "# patchchecker running."
if [ "`uname -s`" = "SunOS" -a "${MOZILLA_SOLARIS_PATCHCHECKER}" != "disable_patchchecker" ] ; then
    moz_spc_verbose_echo "# Solaris"

    # Prechecks
    if [ -z "${dist_bin}" ] ; then
        moz_spc_fatal_error "dist_bin not set."
    fi
    if [ -z "${MOZ_USER_DIR}" ] ; then
        moz_spc_fatal_error "MOZ_USER_DIR not set."
    fi
    if [ ! -x "${dist_bin}/moz_patch_checker.dtksh" ] ; then
        moz_spc_fatal_error "Patch checker script ${dist_bin}/moz_patch_checker.dtksh not found or not executable."
    fi    
    
    # Generate a "key" which is (more or less) unique for each single mozilla installation
    keyfiledir="${HOME}/${MOZ_USER_DIR}"
    keyfile="${keyfiledir}/solaris_patchchecker_keys.txt"
    moz_spc_verbose_echo "# keyfile=$keyfile"   
    key1="`uname -n`:${dist_bin}" # host and path of mozilla
    key2="`ls -la ${dist_bin} | sum | tr -d ' '`" # file names and time stamps
    key="spckey=${key1};${key2}"
    moz_spc_verbose_echo "# key=$key"

    # Check whether we checked the patches for this installation
    if [ "`([ -r "${keyfile}" ] && cat "${keyfile}") | fgrep "${key}"`" != "" ] ; then
        moz_spc_verbose_echo "# match found in ${keyfile}"
    else
        moz_spc_verbose_echo "# no match found in ${keyfile}"

        # Run patch checker
        ${dist_bin}/moz_patch_checker.dtksh
        spc_exitcode=$?

        if [ ${spc_exitcode} -eq 0 ] ; then
            moz_spc_verbose_echo "# Patch checker returned OK, caching result"
            mkdir -p "${keyfiledir}"
            if [ ! -f "${keyfile}" ] ; then
                echo >"${keyfile}"  "# Solaris patch checker 'key' file"
                echo >>"${keyfile}" "# This script is for private use of the Mozilla patch checker for Solaris"
            fi
            echo "${key}" >>"${keyfile}"
        else
            echo "# Patch checker failure. Please apply the missing Solaris patches."
            moz_startstop_addon_scripts "stop"
            exit 1
        fi
    fi
fi 
moz_spc_verbose_echo "# patchchecker done."
# EOF.
