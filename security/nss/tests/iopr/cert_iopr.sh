#! /bin/sh
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

########################################################################
#
# mozilla/security/nss/tests/iopr/cert_iopr.sh
#
# Certificate generating and handeling for NSS interoperability QA. This file
# is included from cert.sh
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
# FIXME - Netscape - NSS
########################################################################

IOPR_CERT_SOURCED=1

########################################################################
# function wraps calls to pk12util, also: writes action and options
# to stdout. 
# Params are the same as to pk12util.
# Returns pk12util status
#
pk12u()
{
    echo "${CU_ACTION} --------------------------"

    echo "pk12util $@"
    pk12util $@
    RET=$?

    return $RET
}

########################################################################
# Initializes nss db directory and files if they don't exists
# Params:
#      $1 - directory location
#
createDBDir() {
    trgDir=$1

    if [ -z "`ls $trgDir | grep db`" ]; then
        CU_ACTION="Initializing DB at $dir"
        certu -N -d "${trgDir}" -f "${R_PWFILE}" 2>&1
        if [ "$RET" -ne 0 ]; then
            return $RET
        fi
    fi
}
########################################################################
# takes care of downloading config, cert and crl files from remote
# location. 
# Params:
#      $1 - name of the host file will be downloaded from
#      $2 - path to the file as it appeared in url
#      $3 - target directory the file will be saved at.
# Returns tstclnt status.
#
download_file() {
    host=$1
    filePath=$2
    trgDir=$3

    file=$trgDir/`basename $filePath`

    createDBDir $trgDir || return $RET

#    echo wget -O $file http://${host}${filePath}
#    wget -O $file http://${host}${filePath}
#    ret=$?

    req=$file.$$
    echo "GET $filePath HTTP/1.0" > $req
    echo >> $req

    tstclnt -d $trgDir -S -h $host -p $IOPR_DOWNLOAD_PORT \
        -w ${R_PWFILE} -o < $req > $file
    ret=$?
    rm -f $_tmp;
    return $ret
}

########################################################################
# Uses pk12util, certutil of cerlutil to import files to an nss db located
# at <dir>(the value of $1 parameter). Chooses a utility to use based on
# a file extension. Initializing a db if it does not exists.
# Params:
#      $1 - db location directory
#      $2 - file name to import
#      $3 - nick name an object in the file will be associated with
#      $4 - trust arguments 
# Returns status of import
#      
importFile() {
    dir=$1\
    file=$2
    certName=$3
    certTrust=$4

    [ ! -d $dir ] && mkdir -p $dir;

    createDBDir $dir || return $RET
            
    case `basename $file | sed 's/^.*\.//'` in
        p12)
            CU_ACTION="Importing p12 $file to DB at $dir"
            pk12u -d $dir -i $file -k ${R_PWFILE} -W iopr
            [ $? -ne 0 ] && return 1
            CU_ACTION="Modifying trust for cert $certName at $dir"
            certu -M -n "$certName" -t "$certTrust" -f "${R_PWFILE}" -d "${dir}"
            return $?
            ;;
        
        crl) 
            CU_ACTION="Importing crl $file to DB at $dir"
            crlu -d ${dir} -I -n TestCA -i $file
            return $?
            ;;

        crt | cert)
            CU_ACTION="Importing cert $certName with trust $certTrust to $dir"
            certu -A -n "$certName" -t "$certTrust" -f "${R_PWFILE}" -d "${dir}" \
                -i "$file"
            return $?
            ;;

        *)
            echo "Unknown file extension: $file:"
            return 1
            ;;
    esac
}


#########################################################################
# Downloads and installs test certs and crl from a remote webserver.
# Generates server cert for reverse testing if reverse test run is turned on.
# Params:
#      $1 - host name to download files from.
#      $2 - directory at which CA cert will be installed and used for
#           signing a server cert.
#      $3 - path to a config file in webserver context.
#      $4 - ssl server db location
#      $5 - ssl client db location
#      $5 - ocsp client db location
#
# Returns 0 upon success, otherwise, failed command error code.
#
download_install_certs() {
    host=$1
    caDir=$2
    confPath=$3
    sslServerDir=$4
    sslClientDir=$5
    ocspClientDir=$6

    [ ! -d "$caDir" ] && mkdir -p $caDir;

    #=======================================================
    # Getting config file
    #
    download_file $host "$confPath/iopr_server.cfg" $caDir
    RET=$?
    if [ $RET -ne 0 -o ! -f $caDir/iopr_server.cfg ]; then
        html_failed "<TR><TD>Fail to download website config file(ws: $host)" 
        return 1
    fi

    . $caDir/iopr_server.cfg
    RET=$?
    if [ $RET -ne 0 ]; then
        html_failed "<TR><TD>Fail to source config file(ws: $host)" 
        return $RET
    fi

    #=======================================================
    # Getting CA file
    #

    #----------------- !!!WARNING!!! -----------------------
    # Do NOT copy this scenario. CA should never accompany its
    # cert with the private key when deliver cert to a customer.
    #----------------- !!!WARNING!!! -----------------------

    download_file $host $certDir/$caCertName.p12 $caDir
    RET=$?
    if [ $RET -ne 0 -o ! -f $caDir/$caCertName.p12 ]; then
        html_failed "<TR><TD>Fail to download $caCertName cert(ws: $host)" 
        return 1
    fi
    tmpFiles="$caDir/$caCertName.p12"

    importFile $caDir $caDir/$caCertName.p12 $caCertName "TC,C,C"
    RET=$?
    if [ $RET -ne 0 ]; then
        html_failed "<TR><TD>Fail to import $caCertName cert to CA DB(ws: $host)" 
        return $RET
    fi

    CU_ACTION="Exporting Root CA cert(ws: $host)"
    certu -L -n $caCertName -r -d ${caDir} -o $caDir/$caCertName.cert 
    if [ "$RET" -ne 0 ]; then
        Exit 7 "Fatal - failed to export $caCertName cert"
    fi

    #=======================================================
    # Check what tests we want to run
    #
    doSslTests=0; doOcspTests=0
    # XXX remove "_new" from variables below
    [ -n "`echo ${supportedTests_new} | grep -i ssl`" ] && doSslTests=1
    [ -n "`echo ${supportedTests_new} | grep -i ocsp`" ] && doOcspTests=1

    if [ $doSslTests -eq 1 ]; then
        if [ "$reverseRunCGIScript" ]; then
            [ ! -d "$sslServerDir" ] && mkdir -p $sslServerDir;
            #=======================================================
            # Import CA cert to server DB
            #
            importFile $sslServerDir $caDir/$caCertName.cert server-client-CA \
                        "TC,C,C"
            RET=$?
            if [ $RET -ne 0 ]; then
                html_failed "<TR><TD>Fail to import server-client-CA cert to \
                             server DB(ws: $host)" 
                return $RET
            fi
            
            #=======================================================
            # Creating server cert
            #
            CERTNAME=$HOSTADDR
            
            CU_ACTION="Generate Cert Request for $CERTNAME (ws: $host)"
            CU_SUBJECT="CN=$CERTNAME, E=${CERTNAME}@bogus.com, O=BOGUS NSS, \
                        L=Mountain View, ST=California, C=US"
            certu -R -d "${sslServerDir}" -f "${R_PWFILE}" -z "${R_NOISE_FILE}"\
                -o $sslServerDir/req 2>&1
            tmpFiles="$tmpFiles $sslServerDir/req"
            
            
            CU_ACTION="Sign ${CERTNAME}'s Request (ws: $host)"
            certu -C -c "$caCertName" -m `date +"%s"` -v 60 -d "${caDir}" \
                -i  ${sslServerDir}/req -o $caDir/${CERTNAME}.cert \
                -f "${R_PWFILE}" 2>&1
            
            importFile $sslServerDir $caDir/$CERTNAME.cert $CERTNAME ",,"
            RET=$?
            if [ $RET -ne 0 ]; then
                html_failed "<TR><TD>Fail to import $CERTNAME cert to server\
                             DB(ws: $host)" 
                return $RET
            fi
            tmpFiles="$tmpFiles $caDir/$CERTNAME.cert"
            
            #=======================================================
            # Download and import CA crl to server DB
            #
            download_file $host "$certDir/$caCrlName.crl" $sslServerDir
            RET=$?
            if [ $? -ne 0 ]; then
                html_failed "<TR><TD>Fail to download $caCertName crl\
                             (ws: $host)" 
                return $RET
            fi
            tmpFiles="$tmpFiles $sslServerDir/$caCrlName.crl"
            
            importFile $sslServerDir $sslServerDir/TestCA.crl
            RET=$?
            if [ $RET -ne 0 ]; then
                html_failed "<TR><TD>Fail to import TestCA crt to server\
                             DB(ws: $host)" 
                return $RET
            fi
        fi # if [ "$reverseRunCGIScript" ]
        
        [ ! -d "$sslClientDir" ] && mkdir -p $sslClientDir;
        #=======================================================
        # Import CA cert to ssl client DB
        #
        importFile $sslClientDir $caDir/$caCertName.cert server-client-CA \
                   "TC,C,C"
        RET=$?
        if [ $RET -ne 0 ]; then
            html_failed "<TR><TD>Fail to import server-client-CA cert to \
                         server DB(ws: $host)" 
            return $RET
        fi
    fi

    if [ $doOcspTests -eq 1 ]; then
        [ ! -d "$ocspClientDir" ] && mkdir -p $ocspClientDir;
        #=======================================================
        # Import CA cert to ocsp client DB
        #
        importFile $ocspClientDir $caDir/$caCertName.cert server-client-CA \
                   "TC,C,C"
        RET=$?
        if [ $RET -ne 0 ]; then
            html_failed "<TR><TD>Fail to import server-client-CA cert to \
                         server DB(ws: $host)" 
            return $RET
        fi
    fi

    #=======================================================
    # Import client certs to client DB
    #
    for fileName in $downloadFiles; do
        certName=`echo $fileName | sed 's/\..*//'`

        if [ -n "`echo $certName | grep ocsp`" -a $doOcspTests -eq 1 ]; then
            clientDir=$ocspClientDir
        elif [ $doSslTests -eq 1 ]; then
            clientDir=$sslClientDir
        else
            continue
        fi

        download_file $host "$certDir/$fileName" $clientDir
        RET=$?
        if [ $RET -ne 0 -o ! -f $clientDir/$fileName ]; then
            html_failed "<TR><TD>Fail to download $certName cert(ws: $host)" 
            return $RET
        fi
        tmpFiles="$tmpFiles $clientDir/$fileName"
        
        importFile $clientDir $clientDir/$fileName $certName ",,"
        RET=$?
        if [ $RET -ne 0 ]; then
            html_failed "<TR><TD>Fail to import $certName cert to client DB\
                        (ws: $host)" 
            return $RET
        fi
    done

    rm -f $tmpFiles

    return 0
}


#########################################################################
# Initial point for downloading config, cert, crl files for multiple hosts
# involved in interoperability testing. Called from nss/tests/cert/cert.sh
# It will only proceed with downloading if environment variable 
# IOPR_HOSTADDR_LIST is set and has a value of host names separated by space.
#
# Returns 1 if interoperability testing is off, 0 otherwise. 
#
cert_iopr_setup() {

    if [ "$IOPR" -ne 1 ]; then
        return 1
    fi
    num=1
    IOPR_HOST_PARAM=`echo "${IOPR_HOSTADDR_LIST} " | cut -f 1 -d' '`
    while [ "$IOPR_HOST_PARAM" ]; do
        IOPR_HOSTADDR=`echo $IOPR_HOST_PARAM | cut -f 1 -d':'`
        IOPR_DOWNLOAD_PORT=`echo "$IOPR_HOST_PARAM:" | cut -f 2 -d':'`
        [ -z "$IOPR_DOWNLOAD_PORT" ] && IOPR_DOWNLOAD_PORT=443
        IOPR_CONF_PATH=`echo "$IOPR_HOST_PARAM:" | cut -f 3 -d':'`
        [ -z "$IOPR_CONF_PATH" ] && IOPR_CONF_PATH="/iopr"
        
        echo "Installing certs for $IOPR_HOSTADDR:$IOPR_DOWNLOAD_PORT:\
              $IOPR_CONF_PATH"
        
        download_install_certs ${IOPR_HOSTADDR} ${IOPR_CADIR}_${IOPR_HOSTADDR} \
            ${IOPR_CONF_PATH} ${IOPR_SSL_SERVERDIR}_${IOPR_HOSTADDR} \
            ${IOPR_SSL_CLIENTDIR}_${IOPR_HOSTADDR} \
            ${IOPR_OCSP_CLIENTDIR}_${IOPR_HOSTADDR}
        if [ $? -ne 0 ]; then
            echo "wsFlags=\"NOIOPR $wsParam\"" >> \
                ${IOPR_CADIR}_${IOPR_HOSTADDR}/iopr_server.cfg
        fi
        num=`expr $num + 1`
        IOPR_HOST_PARAM=`echo "${IOPR_HOSTADDR_LIST} " | cut -f $num -d' '`
    done
    
    return 0
}
