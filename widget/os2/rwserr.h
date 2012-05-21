/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/****************************************************************************/
/* RWS - beta version 0.80                                                  */
/****************************************************************************/

// RWSERR.H
// Remote Workplace Server - return codes

/****************************************************************************/
/* RWSSRV Errors                                                            */
/****************************************************************************/

#define RWSSRV_EXCEPTION            1000
#define RWSSRV_UNKNOWNERROR         1001
#define RWSSRV_DOSGETMEM            1002
#define RWSSRV_BADARGCNT            1003
#define RWSSRV_PROCBADDSC           1004
#define RWSSRV_PROCTOOMANYARGS      1005
#define RWSSRV_PROCTOOFEWARGS       1006
#define RWSSRV_PROCBADGIVEQ         1007
#define RWSSRV_PROCNOPFN            1008
#define RWSSRV_PROCNOMETHODNAME     1009
#define RWSSRV_PROCNOORD            1010
#define RWSSRV_PROCBADCMD           1011
#define RWSSRV_PROCBADTYPE          1012
#define RWSSRV_PROCNOWPOBJ          1013
#define RWSSRV_PROCRESOLVEFAILED    1014
#define RWSSRV_PROCQRYPROCFAILED    1015
#define RWSSRV_ARGBADDSC            1016
#define RWSSRV_ARGNOVALUE           1017
#define RWSSRV_ARGNOGIVESIZE        1018
#define RWSSRV_ARGNOWPOBJ           1019
#define RWSSRV_WPALLOCFAILED        1020
#define RWSSRV_SOMALLOCFAILED       1021
#define RWSSRV_ARGBADGIVEQ          1022
#define RWSSRV_ARGBADGIVECONV       1023
#define RWSSRV_ARGGIVECONVFAILED    1024
#define RWSSRV_ARGBADGIVE           1025
#define RWSSRV_ARGNULLPTR           1026
#define RWSSRV_ARGBADGET            1027
#define RWSSRV_FUNCTIONFAILED       1028
#define RWSSRV_RTNBADDSC            1029
#define RWSSRV_RTNCOPYICONFAILED    1030
#define RWSSRV_RTNBADGET            1031
#define RWSSRV_OUTVALUEZERO         1032
#define RWSSRV_OUTCNTZERO           1033
#define RWSSRV_OUTBUFTOOSMALL       1034
#define RWSSRV_OUTBADGETCONV        1035
#define RWSSRV_OUTGETCONVFAILED     1036
#define RWSSRV_CONVERTFAILED        1037
#define RWSSRV_SETHANDLER           1038

#define RWSSRV_CMDFAILED            1101
#define RWSSRV_CMDBADARGCNT         1102
#define RWSSRV_CMDBADRTNTYPE        1103
#define RWSSRV_CMDBADARGTYPE        1104
#define RWSSRV_CMDNOMENUWND         1105
#define RWSSRV_CMDPOSTMSGFAILED     1106
#define RWSSRV_CMDNOTACMD           1107
#define RWSSRV_CMDBADCMD            1108
#define RWSSRV_CMDBADARG            1109


/****************************************************************************/
/* RWSCLI Errors                                                            */
/****************************************************************************/

#define RWSCLI_UNKNOWNERROR         2001
#define RWSCLI_NOATOM               2002
#define RWSCLI_NOCLIENTWND          2003
#define RWSCLI_DOSALLOCMEM          2004
#define RWSCLI_DOSSUBSETMEM         2005
#define RWSCLI_OUTOFMEM             2006
#define RWSCLI_BADMEMSIZE           2007
#define RWSCLI_BADMEMPTR            2008
#define RWSCLI_BADDISPATCHARG       2009
#define RWSCLI_SERVERNOTFOUND       2010
#define RWSCLI_NOTIMER              2011
#define RWSCLI_WINPOSTMSG           2012
#define RWSCLI_WM_QUIT              2013
#define RWSCLI_TIMEOUT              2014
#define RWSCLI_MISSINGARGS          2015
#define RWSCLI_SETPROC              2016
#define RWSCLI_SETRTN               2017
#define RWSCLI_SETARG               2018
#define RWSCLI_MEMINUSE             2019
#define RWSCLI_TERMINATEFAILED      2020
#define RWSCLI_PROCBADGIVEQ         2021
#define RWSCLI_PROCNOVALUE          2022
#define RWSCLI_PROCBADSIZE          2023
#define RWSCLI_PROCBADCMD           2024
#define RWSCLI_PROCBADTYPE          2025
#define RWSCLI_RTNBADGIVE           2026
#define RWSCLI_RTNBADGETQ           2027
#define RWSCLI_RTNBADSIZE           2028
#define RWSCLI_RTNBADGETCONV        2029
#define RWSCLI_RTNBADGET            2030
#define RWSCLI_ARGBADGIVEQ          2031
#define RWSCLI_ARGNOVALUE           2032
#define RWSCLI_ARGBADSIZE           2033
#define RWSCLI_ARGBADGIVECONV       2034
#define RWSCLI_ARGBADGIVE           2035
#define RWSCLI_ARGBADGETQ           2036
#define RWSCLI_ARGBADGETSIZE        2037
#define RWSCLI_ARGBADGETCONV        2038
#define RWSCLI_ARGBADGET            2039
#define RWSCLI_BADARGNDX            2040
#define RWSCLI_ERRMSGNOTFOUND       2041
#define RWSCLI_BADTIMEOUT           2042
#define RWSCLI_RECURSIVECALL        2043
#define RWSCLI_UNEXPECTEDMSG        2044
#define RWSCLI_BADTID               2045
#define RWSCLI_NOTINDISPATCH        2046

/****************************************************************************/

