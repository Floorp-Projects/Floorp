# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape Security Services for Java.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.

#
# automatically generate all the stubs and associated cruft to call
# from the Java thread into the Mozilla thread and get a return value
# back from Mozilla.
#
#

# input looks like so:
#
# set <variable> <value>
# gen <return-type> <error-value> <function-name> ([<type>, [<type>,...]])

# if <return-type> is more than one word (i.e.: "struct foo") then
# surround it with parentheses

# variables that are worth setting:
#     prefix -- string prepended to all generated functions and structs
#     filename -- name for .c and .h file outputs
#     getError -- function which returns system error number
#     setError -- function which sets system error number

#
# example input:
#   set prefix nsn
#   gen int -1 SSL_Socket int int int
#   gen int -1 SSL_Connect (char *, const struct sockaddr_in *, int)

if($#ARGV != 0) {
    printf STDERR "Usage: perl mozStubGen.pl inputfile.data\n";
    exit 1;
}

$inputFileName = $ARGV[0];
open(DATA, $inputFileName) || die "Can't open $inputFileName: $!\n";

while(<DATA>) {
    chop;     # nuke trailing end-of-line
    s/^\s*//; # nuke leading whitespace
    s/#.*$//; # nuke comments
    next if /^$/;

    @tokens = split(/\s+/);

    if($tokens[0] eq "set") {
	$vars{$tokens[1]} = $tokens[2];
    } elsif ($tokens[0] eq "gen") {
	&validVars();
	&openFiles();
	&genSrvStub($_);
    } else {
	warn "bogus input ignored";
    }
}

printf OUTH <<EOF, join("\n", @stuberrList);
%s
EOF

sub validVars {
    die("prefix must be defined before generating a stub.\n")
	unless exists($vars{"prefix"});
    die("filename must be defined before generating a stub.\n")
	unless exists($vars{"filename"});
    die("getError must be defined before generating a stub.\n")
	unless exists($vars{"getError"});
    die("setError must be defined before generating a stub.\n")
	unless exists($vars{"setError"});
    die("threadsafeError must be defined before generating a stub.\n")
	unless exists($vars{"threadsafeError"});
}

$filesOpen = 0;
sub openFiles {
    return if $filesOpen;

    open(OUTC, "> $vars{filename}.c") || die "Can't write $vars{filename}.c\n";
    open(OUTH, "> $vars{filename}.h") || die "Can't write $vars{filename}.h\n";
    $filesOpen = 1;

    printf OUTC <<EOF;
/*
 * THIS FILE IS MACHINE GENERATED.  DO NOT EDIT!
 *
 * Instead, go edit $inputFileName and re-run mozStubGen.pl
 * (or you should be able to just run nmake/gmake on NT or Unix)
 *
 * You are meant to create another ".c" file which includes all the
 * appropriate header files for your stubs and include *this* C file
 * in *that* one.
 *
 * Copyright (c) 1997, Netscape Communications Corp.
 */

#include "$vars{filename}.h"

/*
 * If mozilla_event_queue is non-null, then we have to do the crazy
 * function calls.  This is commented out because it's defined for
 * real in one of the files we include.
 */
/* extern void *mozilla_event_queue; */

/*
 * error handling is unfortunately complex -- if error values are
 * not thread-safe, then we should only ever call the get and set
 * methods while on the Mozilla thread.
 */

#define THREADSAFE_ERROR $vars{threadsafeError}
#define GET_ERROR() ($vars{getError}())
#define SET_ERROR(x) ($vars{setError}(x))

EOF
    printf OUTH <<EOF;
/*
 * THIS FILE IS MACHINE GENERATED.  DO NOT EDIT!
 *
 * Instead, go edit $inputFileName and re-run mozStubGen.pl
 * (or you should be able to just run nmake/gmake on NT or Unix)
 *
 * Include this file to use the stubs in $vars{filename}.c and you're all set.  
 *
 * Copyright (c) 1997, Netscape Communications Corp.
 */

typedef PRFileDesc * SSLFD;

EOF
}

sub genStub {
    local($junk, $retType, $errorValue, $funcName, $argTypeData);
    $junk = 0;  # make warning go away
    local(@argTypes);
    local($prefix) = $vars{"prefix"};
    local($getError) = $vars{"getError"};
    local($setError) = $vars{"setError"};
    local($formalArgs) = "";
    local($passthruArgs) = "";
    local($safePassthruArgs) = "";
    local($structElements) = "";
    local($formalsToStruct) = "";
    local($structToArgs) = "";
    local($varIndex);

    ($junk, $retType) = $_[0] =~ /^([^\s]+)\s+([^\s]+)\s+/;

    # magic to handle return type if it starts with parenthesis
    if($retType =~ /^\(/) {
	($junk, $retType, $errorValue, $funcName, $argTypeData) = $_[0] =~
	    /^([^\s]+)\s+\(([^)]+)\)\s*([^\s]+)\s+([^\s]+)\s*\(([^)]*)\)/;
    } else {
	($junk, $retType, $errorValue, $funcName, $argTypeData) = $_[0] =~
	    /^([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s*\(([^)]*)\)/;
    }
    @argTypes = split(/,\s*/, $argTypeData);

    local($stuberrFunc) = "${prefix}_stuberr_${funcName}";
    local($sysStubFunc) = "${prefix}_sysStub_${funcName}";
    local($closureType) = "struct ${prefix}_args_${funcName}";
    $structElements = "$closureType {\n";
#                                          }
    $structElements .= "    $retType result;\n"
	unless $retType eq "void";

    $varIndex = 0;
    foreach $arg (@argTypes) {
	$formalArgs       .= "$arg in${varIndex}, ";
	$passthruArgs     .= "in${varIndex},";
	$safePassthruArgs .= "(in${varIndex}),";
	$structElements   .= "    $arg in${varIndex};\n";
	$formalsToStruct  .= "    closure->in${varIndex} = in${varIndex};\n";
	$structToArgs     .= "closure->in${varIndex}, ";

	$varIndex++;
    }

    # nuke trailing commas
    chop($structToArgs);
    chop($structToArgs);
    chop($formalArgs);
    chop($formalArgs);
    chop($passthruArgs);
    chop($safePassthruArgs);

    local($optComma) = "";
    $optComma = ", " if $varIndex > 0;
    

    push(@stuberrList, "extern $retType ${stuberrFunc}(int* err_code${optComma} $formalArgs);");

#
# unfortunately, the error version of this can't easily be turned
# into a macro, so we won't bother.
#
# Hack: If you're doing passthru, you don't get the error versions.
#
#    push(@passthruList, "extern $retType ${stuberrFunc}(int* err_code${optComma} $formalArgs);");

# #######################################################################
#	Client Template
# Here is the template for the body of code generated for each function.
# Each of the "%s"es in the template corresponds to a positional parameter
# to the printf command immediately following this template.
# If you change the number or order of these "%s"es, you must change the 
# parameters to the printf below, also.
# {

    $printStr = <<EOF;

/*******************************************************************
 * automatically generated stubs for ${funcName}
 * $_[0]
 *******************************************************************/

$structElements
    int prErrno;
};

/* entry point for system thread -- do not call this directly! */
PR_STATIC_CALLBACK(void)
${sysStubFunc}(${closureType}* closure) {
    %s /* closure->result or nothing */
    closure->prErrno = GET_ERROR();
}


/*
 * stub for $funcName -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
$retType
${stuberrFunc}(int* err_code${optComma} $formalArgs) {
    /* return type defn or nothing */
    %s

    $closureType *closure;
#if !(defined(XP_PC) && !defined(_WIN32))   /* then we are *NOT* win 16 */ 
    $closureType closureStorage;
#endif
    
    *err_code = 0;
    /* if no event queue, then call directly */
    PR_ASSERT( mozilla_event_queue != NULL);
    if (mozilla_event_queue == NULL) {
	%s${funcName}($passthruArgs);
#if !(THREADSAFE_ERROR)
	*err_code = GET_ERROR();
#endif
        %s
    }

#if defined(XP_PC) && !defined(_WIN32)   /* then we are win 16 */ 
    closure = PR_NEW($closureType);
#else
    closure = &closureStorage;
#endif

    if(closure == NULL) {
#if THREADSAFE_ERROR
	*err_code = GET_ERROR();	/* XXX should be some out-of-mem err */
#endif
	/* error return */
	%s
    }

    closure->prErrno = 0;

$formalsToStruct

    ET_moz_CallFunction((ETVoidPtrFunc) ${sysStubFunc}, closure);

    /* retrieve error from closure or nothing */
    %s

    *err_code = closure->prErrno;

#if defined(XP_PC) && !defined(_WIN32)   /* then we are win 16 */ 
    PR_DELETE(closure);
#endif

    /* return result or nothing */
    %s
}

EOF

#       End of template of code generated for each function
#
# ##################################################################
#

    if($retType eq "void") {
	printf OUTC $printStr,
	    "${funcName}($structToArgs);",
	    "",
	    "",
	    "return;",
	    "return;",
	    "",
	    "return;";
    } else {
	printf OUTC $printStr,
	    "closure->result = ${funcName}($structToArgs);",
	    "$retType retVal;",
	    "$retType retVal = ",
	    "return retVal;",
	    "return $errorValue;",
	    "retVal = closure->result;",
	    "return retVal;";
    }
}

#
#######################################################################
#   Server Implementation
#
#
#

sub genSrvStub {
    local($junk, $retType, $errorValue, $funcName, $argTypeData);
    $junk = 0;  # make warning go away
    local(@argTypes);
    local($prefix) = $vars{"prefix"};
    local($getError) = $vars{"getError"};
    local($setError) = $vars{"setError"};
    local($formalArgs) = "";
    local($passthruArgs) = "";
    local($safePassthruArgs) = "";
    local($varIndex);

    ($junk, $retType) = $_[0] =~ /^([^\s]+)\s+([^\s]+)\s+/;

    # magic to handle return type if it starts with parenthesis
    if($retType =~ /^\(/) {
	($junk, $retType, $errorValue, $funcName, $argTypeData) = $_[0] =~
	    /^([^\s]+)\s+\(([^)]+)\)\s*([^\s]+)\s+([^\s]+)\s*\(([^)]*)\)/;
    } else {
	($junk, $retType, $errorValue, $funcName, $argTypeData) = $_[0] =~
	    /^([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s*\(([^)]*)\)/;
    }
    @argTypes = split(/,\s*/, $argTypeData);

    local($stuberrFunc) = "${prefix}_stuberr_${funcName}";

    $varIndex = 0;
    foreach $arg (@argTypes) {
	$formalArgs       .= "$arg in${varIndex}, ";
	$passthruArgs     .= "in${varIndex},";
	$safePassthruArgs .= "(in${varIndex}),";

	$varIndex++;
    }

    # nuke trailing commas
    chop($formalArgs);
    chop($formalArgs);
    chop($passthruArgs);
    chop($safePassthruArgs);

    local($optComma) = "";
    $optComma = ", " if $varIndex > 0;
    

    push(@stuberrList, "extern $retType ${stuberrFunc}(int* err_code${optComma} $formalArgs);");

#
# unfortunately, the error version of this can't easily be turned
# into a macro, so we won't bother.
#
# Hack: If you're doing passthru, you don't get the error versions.
#
#    push(@passthruList, "extern $retType ${stuberrFunc}(int* err_code${optComma} $formalArgs);");


# #######################################################################
#	Server template
# Here is the template for the body of code generated for each function.
# Each of the "%s"es in the template corresponds to a positional parameter
# to the printf command immediately following this template.
# If you change the number or order of these "%s"es, you must change the 
# parameters to the printf below, also. 
#

    $printStr = <<EOF;

/*******************************************************************
 * automatically generated stubs for ${funcName}
 * $_[0]
 *******************************************************************/

/*
 * stub for $funcName -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
$retType
${stuberrFunc}(int* err_code${optComma} $formalArgs) {
    /* return type defn or nothing */
    %s
    
    *err_code = 0;
    %s${funcName}($passthruArgs);
    *err_code = GET_ERROR();

    /* return result or nothing */
    %s
}

EOF

#       End of template of code generated for each function
#
# ##################################################################
#

    if($retType eq "void") {
	printf OUTC $printStr,
	    "",
	    "",
	    "";
    } else {
	printf OUTC $printStr,
	    "$retType retVal;",
	    "retVal = ",
	    "return retVal;";
    }

}

