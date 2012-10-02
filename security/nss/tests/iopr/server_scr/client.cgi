#!/usr/bin/perl

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#--------------------------------------------------------------
# cgi script that parses request argument to appropriate 
# open ssl or tstclntw options and starts ssl client.
#

use CGI qw/:standard/;

use subs qw(debug);

#--------------------------------------------------------------
# Prints out an error string and exits the script with an
# exitStatus.
# Param:
#    str : an error string
#    exitStat: an exit status of the program
#
sub svr_error {
    my ($str, $exitStat) = @_;

    if (!defined $str || $str eq "") {
        $str = $ERR;
    }
    print "SERVER ERROR: $str\n";
    if ($exitStat) {
        print end_html if ($osDataArr{wservRun});
        exit $exitStat;
    }
}

#--------------------------------------------------------------
# Prints out a debug message
# Params:
#     str: debug message
#     inVal: additional value to print(optional)
#
sub debug {
    my ($str, $inVal) = @_;
    
    print "-- DEBUG: $str ($inVal)\n" if ($DEBUG == 1);
}


#--------------------------------------------------------------
# Initializes execution context depending on a webserver the
# script is running under.
#
sub init {
    %osDataArr = (
                  loadSupportedCipthersFn => \&osSpecific,
                  cipherIsSupportedFn => \&verifyCipherSupport,
                  cipherListFn => \&convertCipher,
                  buildCipherTableFn => \&buildCipherTable,
                  execCmdFn => \&osSpecific,
                  );

    $scriptName = $ENV{'SCRIPT_NAME'};
    if (!defined $scriptName) {
        $DEBUG=1;
        debug "Debug is ON";
    }
    $DEBUG=1;
    
    $svrSoft = $ENV{'SERVER_SOFTWARE'};
    if (defined $svrSoft) {
        $_ = $svrSoft;
        /.*Microsoft.*/ && ($osDataArr{wserv} = "IIS");
        /.*Apache.*/ && ($osDataArr{wserv} = "Apache");
        $osDataArr{wservRun} = 1;
    } else {
        $osDataArr{wserv} = "Apache";
        $osDataArr{wservRun} = 0;
    }
}

#--------------------------------------------------------------
# Function-spigot to handle errors is OS specific functions are
# not implemented for a particular OS.
# Returns:
#   always returns 0(failure)
#
sub osSpecific {
    $ERR = "This function should be swapped to os specific function.";
    return 0;
}

#--------------------------------------------------------------
# Sets os specific execution context values.
# Returns:
#    1 upon success, or 0 upon failure(if OS was not recognized)
#
sub setFunctRefs {
    
    debug("Entering setFunctRefs function", $osDataArr{wserv});

    if ($osDataArr{wserv} eq "Apache") {
        $osDataArr{osConfigFile} = "apache_unix.cfg";
        $osDataArr{suppCiphersCmd} = '$opensslb ciphers ALL:NULL';
        $osDataArr{clientRunCmd} = '$opensslb s_client -host $in_host -port $in_port -cert $certDir/$in_cert.crt -key $certDir/$in_cert.key -CAfile $caCertFile $proto $ciphers -ign_eof < $reqFile';
        $osDataArr{loadSupportedCipthersFn} = \&getSupportedCipherList_Unix;
        $osDataArr{execCmdFn} = \&execClientCmd_Unix;
    } elsif ($osDataArr{wserv} eq "IIS") {
        $osDataArr{osConfigFile} = "iis_windows.cfg";
        $osDataArr{suppCiphersCmd} = '$tstclntwb';
        $osDataArr{clientRunCmd} = '$tstclntwb -h $in_host -p $in_port -n $in_cert $proto $ciphers < $reqFile';
        $osDataArr{loadSupportedCipthersFn} = \&getSupportedCipherList_Win;
        $osDataArr{execCmdFn} = \&execClientCmd_Win;
    } else {
        $ERR = "Unknown Web Server  type.";
        return 0;
    }
    return 1;
}

#--------------------------------------------------------------
# Parses data from HTTP request. Will print a form if request
# does not contain sufficient number of parameters.
# Returns: 
#     1 if request has sufficient number of parameters
#     0 if not.
sub getReqData {
    my $debug = param('debug');
    $in_host = param('host');
    $in_port = param('port');
    $in_cert = param('cert');
    $in_cipher = param('cipher');

    if (!$osDataArr{wservRun}) {
        $in_host="goa1";
        $in_port="443";
        $in_cert="TestUser511";
        $in_cipher = "SSL3_RSA_WITH_NULL_SHA";
    }

    debug("Entering getReqData function", "$in_port:$in_host:$in_cert:$in_cipher");

    if (defined $debug && $debug == "debug on") {
        $DEBUG = 1;
    }

    if (!defined $in_host || $in_host eq "" ||
        !defined $in_port || $in_port eq "" ||
        !defined $in_cert || $in_cert eq "") {
        if ($osDataArr{wservRun}) {
            print h1('Command description form:'),
            start_form(-method=>"get"),
            "Host: ",textfield('host'),p,
            "Port: ",textfield('port'),p,
            "Cert: ",textfield('cert'),p,
            "Cipher: ",textfield('cipher'),p,
            checkbox_group(-name=>'debug',
                           -values=>['debug on  ']),
            submit,
            end_form,
            hr;
        } else {
            print "Printing html form to get client arguments\n";
        }
        $ERR = "the following parameters are required: host, port, cert";
        return 0;
    } else {
        print "<pre>" if ($osDataArr{wservRun});
        return 1;
    }
}


#--------------------------------------------------------------
# Building cipher conversion table from file based on the OS.
# Params:
#     tfile: cipher conversion file.
#     sysName: system name
#     tblPrt: returned pointer to a table.
sub buildCipherTable {
    my ($tfile, $sysName, $tblPrt) = @_;
    my @retArr = @$tblPrt;
    my %table, %rtable;
    my $strCount = 0;

    debug("Entering getReqData function", "$tfile:$sysName:$tblPrt");

    ($ERR = "No system name supplied" && return 0) if ($sysName =~ /^$/);
    if (!open(TFILE, "$tfile")) {
        $ERR = "Missing cipher conversion table file.";
        return 0;
    }
    foreach (<TFILE>) {
        chop;
        /^#.*/ && next;
        /^\s*$/ && next;
        if ($strCount++ == 0) {
            my @sysArr =  split /\s+/;
            $colCount = 0;
            for (;$colCount <= $#sysArr;$colCount++) {
                last if ($sysArr[$colCount] =~ /(.*:|^)$sysName.*/);
            }
            next;
        }
        my @ciphArr =  split /\s+/, $_;
        $table{$ciphArr[0]} = $ciphArr[$colCount];
        $rtable{$ciphArr[$colCount]} = $ciphArr[0];
    }
    close(TFILE);
    $cipherTablePtr[0] = \%table;
    $cipherTablePtr[1] = \%rtable;
    return 1
}

#--------------------------------------------------------------
# Client configuration function. Loads client configuration file.
# Initiates cipher table. Loads cipher list supported by ssl client.
#
sub configClient {

    debug "Entering configClient function";

    my $res = &setFunctRefs();
    return $res if (!$res);

    open(CFILE, $osDataArr{'osConfigFile'}) ||
        ($ERR = "Missing configuration file." && return 0);
    foreach (<CFILE>) {
        /^#.*/ && next;
        chop;
        eval $_;
    }
    close(CFILE);
   
    local @cipherTablePtr = ();
    $osDataArr{'buildCipherTableFn'}->($cipherTableFile, $clientSys) || return 0;
    $osDataArr{cipherTable} = $cipherTablePtr[0];
    $osDataArr{rcipherTable} = $cipherTablePtr[1];
    
    local $suppCiphersTablePrt;
    &{$osDataArr{'loadSupportedCipthersFn'}} || return 0;
    $osDataArr{suppCiphersTable} = $suppCiphersTablePrt;
}

#--------------------------------------------------------------
# Verifies that a particular cipher is supported.
# Params:
#    checkCipher: cipher name
# Returns:
#    1 - cipher is supported(also echos the cipher).
#    0 - not supported.
#
sub verifyCipherSupport {
    my ($checkCipher) = @_;
    my @suppCiphersTable = @{$osDataArr{suppCiphersTable}};

    debug("Entering verifyCipherSupport", $checkCipher);
    foreach (@suppCiphersTable) {
        return 1 if ($checkCipher eq $_);
    }
    $ERR = "cipher is not supported.";
    return 0;
}

#--------------------------------------------------------------
# Converts long(?name of the type?) cipher name to 
# openssl/tstclntw cipher name.
# Returns:
#   0 if cipher was not listed. 1 upon success.
#
sub convertCipher {
    my ($cipher) = @_;
    my @retList;
    my $resStr;
    my %cipherTable = %{$osDataArr{cipherTable}};

    debug("Entering convertCipher", $cipher);
    if (defined $cipher) {
        my $cphr = $cipherTable{$cipher};
        if (!defined $cphr) {
            $ERR = "cipher is not listed.";
            return 0;
        }        
        &{$osDataArr{'cipherIsSupportedFn'}}($cphr) || return 0;
        $ciphers = "$cphr";
        return 1;
    }
    return 0;
}

#################################################################
#  UNIX Apache Specific functions
#----------------------------------------------------------------

#--------------------------------------------------------------
# Executes ssl client command to get a list of ciphers supported
# by client.
#
sub getSupportedCipherList_Unix {
    my @arr, @suppCiphersTable;

    debug "Entering getSupportedCipherList_Unix function";

    eval '$sLisrCmd = "'.$osDataArr{'suppCiphersCmd'}.'"';
    if (!open (OUT, "$sLisrCmd|")) {
        $ERR="Can not run command to verify supported cipher list.";
        return 0;
    }
    @arr = <OUT>;
    chop $arr[0];
    @suppCiphersTable = split /:/, $arr[0];
    debug("Supported ciphers", $arr[0]);
    $suppCiphersTablePrt = \@suppCiphersTable;
    close(OUT);
    return 1;
}

#--------------------------------------------------------------
# Lunches ssl client command in response to a request.
#
#
sub execClientCmd_Unix {
    my $proto;
    local $ciphers;

    debug "Entering execClientCmd_Unix";
    if (defined $in_cipher && $in_cipher ne "") {
        my @arr = split /_/, $in_cipher, 2;
        $proto = "-".$arr[0];
        $proto =~ tr /SLT/slt/;
        $proto = "-tls1" if ($proto eq "-tls");
        return 0 if (!&{$osDataArr{'cipherListFn'}}($in_cipher));
        $ciphers = "-cipher $ciphers";
        debug("Return from cipher conversion", "$ciphers");
    }

    eval '$command = "'.$osDataArr{'clientRunCmd'}.'"';
    debug("Executing command", $command);
    if (!open CMD_OUT, "$command 2>&1 |") {
       $ERR = "can not launch client";
       return 0;
    }

    my @cmdOutArr = <CMD_OUT>;
    
    foreach (@cmdOutArr) {
        print $_;
    }

    my $haveVerify = 0;
    my $haveErrors = 0;
    foreach (@cmdOutArr) {
        chop;
        if (/unknown option/) {
            $haveErrors++;
            svr_error "unknown option\n";
            next;
        }
        if (/:no ciphers available/) {
            $haveErrors++;
            svr_error "no cipthers available\n";
            next;
        }
        if (/verify error:/) {
            $haveErrors++;
            svr_error "unable to do verification\n";
            next;
        }
        if (/alert certificate revoked:/) {
            $haveErrors++;
            svr_error "attempt to connect with revoked sertificate\n";
            next;
        }
        if (/(error|ERROR)/) {
            $haveErrors++;
            svr_error "found errors in server log\n";
            next;
        }
        /verify return:1/ && ($haveVerify = 1);
    }
     if ($haveVerify == 0) {
         svr_error "no 'verify return:1' found in server log\n";
         $haveErrors++;
     }

    if ($haveErrors > 0) {
        $ERR = "Have $haveErrors server errors";
        debug "Exiting execClientCmd_Unix";
        return 0;
    }
    debug "Exiting execClientCmd_Unix";
    return 1;
}

#################################################################
#  Windows IIS Specific functions
#----------------------------------------------------------------

#--------------------------------------------------------------
# Executes ssl client command to get a list of ciphers supported
# by client.
#
sub getSupportedCipherList_Win {
    my @arr, @suppCiphersTable;

    debug "Entering getSupportedCipherList_Win function";

    eval '$sLisrCmd = "'.$osDataArr{'suppCiphersCmd'}.'"';
    if (!open (OUT, "$sLisrCmd|")) {
        $ERR="Can not run command to verify supported cipher list.";
        return 0;
    }
    my $startCipherList = 0;
    foreach (<OUT>) {
        chop;
        if ($startCipherList) {
            /^([a-zA-Z])\s+/ && push @suppCiphersTable, $1;
            next;
        }
        /.*from list below.*/ && ($startCipherList = 1);
    }
    debug("Supported ciphers", join ':', @suppCiphersTable);
    $suppCiphersTablePrt = \@suppCiphersTable;
    close(OUT);
    return 1;
}

#--------------------------------------------------------------
# Lunches ssl client command in response to a request.
#
#
sub execClientCmd_Win {
    my $proto;
    local $ciphers;

    debug "Entering execClientCmd_Win";
    if (defined $in_cipher && $in_cipher ne "") {
        my @arr = split /_/, $in_cipher, 2;
        $proto = "-2 -3 -T";

        $proto =~ s/-T// if ($arr[0] eq "TLS");
        $proto =~ s/-3// if ($arr[0] eq "SSL3");
        $proto =~ s/-2// if ($arr[0] eq "SSL2");
	return 0 if (!&{$osDataArr{'cipherListFn'}}($in_cipher));
        $ciphers = "-c $ciphers";
        debug("Return from cipher conversion", $ciphers);
    }

    eval '$command = "'.$osDataArr{'clientRunCmd'}.'"';
    debug("Executing command", $command);
    if (!open CMD_OUT, "$command 2>&1 |") {
        $ERR = "can not launch client";
        return 0;
    }

    my @cmdOutArr = <CMD_OUT>;
    
    foreach (@cmdOutArr) {
        print $_;
    }

    my $haveVerify = 0;
    my $haveErrors = 0;
    foreach (@cmdOutArr) {
        chop;
        if (/unknown option/) {
            $haveErrors++;
            svr_error "unknown option\n";
            next;
        }
        if (/Error performing handshake/) {
            $haveErrors++;
            svr_error "Error performing handshake\n";
            next;
        }
        if (/Error creating credentials/) {
            $haveErrors++;
            svr_error "Error creating credentials\n";
            next;
        }
        if (/Error .* authenticating server credentials!/) {
            $haveErrors++;
            svr_error "Error authenticating server credentials\n";
            next;
        }
        if (/(error|ERROR|Error)/) {
            $haveErrors++;
            svr_error "found errors in server log\n";
            next;
        }
    }

    if ($haveErrors > 0) {
        $ERR = "Have $haveErrors server errors";
        debug "Exiting execClientCmd_Win";
        return 0;
    }
    debug "Exiting execClientCmd_Win";
    return 1;
}

#################################################################
#  Main line of execution
#----------------------------------------------------------------
&init;

if ($osDataArr{wservRun}) {
    print header('text/html').
        start_html('iopr client');
}
 
print "SCRIPT=OK\n";

if (!&getReqData) { 
    svr_error($ERR, 1);
}

if (!&configClient) { 
    svr_error($ERR, 1);
}

&{$osDataArr{'execCmdFn'}} || svr_error;

if ($osDataArr{wservRun}) {
    print "</pre>";
    print end_html;
}
