# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape Mailstone utility, 
# released March 17, 2000.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1997-2000 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s):	Dan Christian <robodan@netscape.com>
#			Marcel DePaolis <marcel@netcape.com>
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU Public License (the "GPL"), in which case the
# provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the NPL, indicate your decision by
# deleting the provisions above and replace them with the notice
# and other provisions required by the GPL.  If you do not delete
# the provisions above, a recipient may use your version of this
# file under either the NPL or the GPL.
#####################################################

# This define the structures that hold summary, client, and graph data,

# This sets the names used for display.  Can be internationalized.
# All top level names are here (both timers and scalars).
# Any unlisted names will map to themselves.
%timerNames
    = (
       #internal name,	Printed name
       "total",		"total",
       "conn",		"connect",
       "reconn",	"reconnect",
       "banner",	"banner",
       "login",		"login",
       "cmd",		"command",
       "submit",	"submit",
       "retrieve",	"retrieve",
       "logout",	"logout",
       "idle",		"idle",
       "connections",	"connections",
       "blocks",	"blocks",
       );

# This sets the names used for display.  Can be internationalized.
%fieldNames
    = (
       #internal name,	Printed name
       "Try",		"Try",
       "Error",		"Error",
       "BytesR",	"BytesR",
       "BytesW",	"BytesW",
       "Time",		"Time",
       "TimeMin",	"TMin",
       "TimeMax",	"TMax",
       "Time2",		"TStd",
       );


# hold time graphs for each protocol
%graphs = ();

# Totals are done during plotting, if needed
%finals = ();			# create base finals hash

# These are sections that dont get passed to mailclient (case insensitive)
@scriptWorkloadSections
    = (
       "Config",		# special, references %params
       "Client",		# testbed client(s)
       "Graph",			# graph generation
       "Setup",			# things to run with ./setup
       "Startup",		# things to run before test
       "Monitor",		# other performance monitoring
       "PreTest",		# things to run before test
       "PostTest",		# things to run after test
       );

# These are sections that arent protocols.  Anything else must be.
@nonProtocolSections
    = (@scriptWorkloadSections, ("Default"));

# These are the known workload parameters (as they will print)
# These are coerced to upper case internally (do NOT internationize)
@workloadParameters
    = (
       "addressFormat",
       "arch",
       "blockID",
       "blockTime",
       "chartHeight",
       "chartPoints",
       "chartWidth",
       "clientCount",
       "command",
       "comments",
       "file",
       "firstAddress",
       "firstLogin",
       "frequency",
       "gnuplot",
       "group",
       "idleTime",
       "leaveMailOnServer",
       "loginFormat",
       "loopDelay",
       "numAddresses",
       "numLogins",
       "numLoops",
       "numRecips",
       "mailClient",
       "maxBlocks",
       "maxClients",
       "maxErrors",
       "maxThreads",
       "maxProcesses",
       "passwdFormat",
       "processes",
       "rampTime",
       "rcp",
       "rsh",
       "sequentialAddresses",
       "sequentialLogins",
       "server",
       "smtpMailFrom",
       "sysConfig",
       "threads",
       "telemetry",
       "tempDir",
       "time",
       "title",
       "TStamp",
       "useAuthLogin",
       "useEHLO",
       "weight",
       "wmapBannerCmds",
       "wmapClientHeader",
       "wmapInBoxCmds",
       "wmapLoginCmd",
       "wmapLoginData",
       "wmapLogoutCmds",
       "wmapMsgReadCmds",
       "wmapMsgWriteCmds",
       "workload",
       );
       
return 1;
