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
# The Original Code is the Netscape Security Services for Java.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

#!/usr/bin/tcsh


if ( `uname` == "AIX" ) then



	setenv LIBPATH ../../../../../dist/AIX4.2_DBG.OBJ/lib:/share/builds/components/jdk/1.1.6/AIX/lib/aix/native_threads
	echo Testing \"jssjava\" on `uname` `uname -v`.`uname -r` DBG platform . . .
	../../../../../dist/AIX4.2_DBG.OBJ/bin/jssjava -classpath ../../../../../dist/classes_DBG:/share/builds/components/jdk/1.1.6/AIX/lib/classes.zip org.mozilla.jss.crypto.PQGGen 512
	echo Testing \"jssjava_g\" on `uname` `uname -v`.`uname -r` DBG platform . . .
	../../../../../dist/AIX4.2_DBG.OBJ/bin/jssjava_g -classpath ../../../../../dist/classes_DBG:/share/builds/components/jdk/1.1.6/AIX/lib/classes.zip org.mozilla.jss.crypto.PQGGen 512


	setenv LIBPATH ../../../../../dist/AIX4.2_OPT.OBJ/lib:/share/builds/components/jdk/1.1.6/AIX/lib/aix/native_threads
	echo Testing \"jssjava\" on `uname` `uname -v`.`uname -r` OPT platform . . .
	../../../../../dist/AIX4.2_OPT.OBJ/bin/jssjava -classpath ../../../../../dist/classes:/share/builds/components/jdk/1.1.6/AIX/lib/classes.zip org.mozilla.jss.crypto.PQGGen 512





else if ( `uname` == "HP-UX" ) then


	setenv SHLIB_PATH ../../../../../dist/HP-UXB.11.00_DBG.OBJ/lib:/share/builds/components/jdk/1.1.5/HP-UX/lib/PA_RISC/native_threads
	echo Testing \"jssjava\" on `uname` `uname -r` DBG platform . . .
	../../../../../dist/HP-UXB.11.00_DBG.OBJ/bin/jssjava -classpath ../../../../../dist/classes_DBG:/share/builds/components/jdk/1.1.5/HP-UX/lib/classes.zip org.mozilla.jss.crypto.PQGGen 512
	echo Testing \"jssjava_g\" on `uname` `uname -r` DBG platform . . .
	../../../../../dist/HP-UXB.11.00_DBG.OBJ/bin/jssjava_g -classpath ../../../../../dist/classes_DBG:/share/builds/components/jdk/1.1.5/HP-UX/lib/classes.zip org.mozilla.jss.crypto.PQGGen 512


	setenv SHLIB_PATH ../../../../../dist/HP-UXB.11.00_OPT.OBJ/lib:/share/builds/components/jdk/1.1.5/HP-UX/lib/PA_RISC/native_threads
	echo Testing \"jssjava\" on `uname` `uname -r` OPT platform . . .
	../../../../../dist/HP-UXB.11.00_OPT.OBJ/bin/jssjava -classpath ../../../../../dist/classes:/share/builds/components/jdk/1.1.5/HP-UX/lib/classes.zip org.mozilla.jss.crypto.PQGGen 512





else if ( ( `uname` == "IRIX" ) || ( `uname` == "IRIX64" ) ) then


	setenv LD_LIBRARY_PATH ../../../../../dist/IRIX6.2_PTH_DBG.OBJ/lib:/share/builds/components/jdk/1.1.5/IRIX/lib32/sgi/native_threads
	echo Testing \"jssjava\" on `uname` `uname -r` DBG platform . . .
	../../../../../dist/IRIX6.2_PTH_DBG.OBJ/bin/jssjava -classpath ../../../../../dist/classes_DBG:/share/builds/components/jdk/1.1.5/IRIX/lib/rt.jar org.mozilla.jss.crypto.PQGGen 512
	echo Testing \"jssjava_g\" on `uname` `uname -r` DBG platform . . .
	../../../../../dist/IRIX6.2_PTH_DBG.OBJ/bin/jssjava_g -classpath ../../../../../dist/classes_DBG:/share/builds/components/jdk/1.1.5/IRIX/lib/rt.jar org.mozilla.jss.crypto.PQGGen 512


	setenv LD_LIBRARY_PATH ../../../../../dist/IRIX6.2_PTH_OPT.OBJ/lib:/share/builds/components/jdk/1.1.5/IRIX/lib32/sgi/native_threads
	echo Testing \"jssjava\" on `uname` `uname -r` OPT platform . . .
	../../../../../dist/IRIX6.2_PTH_OPT.OBJ/bin/jssjava -classpath ../../../../../dist/classes:/share/builds/components/jdk/1.1.5/IRIX/lib/rt.jar org.mozilla.jss.crypto.PQGGen 512





else if ( `uname` == "OSF1" ) then


	setenv LD_LIBRARY_PATH ../../../../../dist/OSF1V4.0D_DBG.OBJ/lib:/share/builds/components/jdk/1.1.6/OSF1/lib/alpha
	echo Testing \"jssjava\" on `uname` `uname -r`D DBG platform . . .
	../../../../../dist/OSF1V4.0D_DBG.OBJ/bin/jssjava -classpath ../../../../../dist/classes_DBG:/share/builds/components/jdk/1.1.5/OSF1/lib/classes.zip org.mozilla.jss.crypto.PQGGen 512
	echo Testing \"jssjava_g\" on `uname` `uname -r`D DBG platform . . .
	../../../../../dist/OSF1V4.0D_DBG.OBJ/bin/jssjava_g -classpath ../../../../../dist/classes_DBG:/share/builds/components/jdk/1.1.5/OSF1/lib/classes.zip org.mozilla.jss.crypto.PQGGen 512


	setenv LD_LIBRARY_PATH ../../../../../dist/OSF1V4.0D_OPT.OBJ/lib:/share/builds/components/jdk/1.1.6/OSF1/lib/alpha
	echo Testing \"jssjava\" on `uname` `uname -r`D OPT platform . . .
	../../../../../dist/OSF1V4.0D_OPT.OBJ/bin/jssjava -classpath ../../../../../dist/classes:/share/builds/components/jdk/1.1.5/OSF1/lib/classes.zip org.mozilla.jss.crypto.PQGGen 512





else if ( ( `uname` == "SunOS" ) && ( `uname -r` == "5.5.1" ) ) then

	setenv LD_LIBRARY_PATH ../../../../../dist/SunOS5.5.1_DBG.OBJ/lib:/share/builds/components/jdk/1.1.6/SunOS/lib/sparc/native_threads
	echo Testing \"jssjava\" on `uname` `uname -r` DBG platform . . .
	../../../../../dist/SunOS5.5.1_DBG.OBJ/bin/jssjava -classpath ../../../../../dist/classes_DBG:/share/builds/components/jdk/1.1.6/SunOS/lib/classes.zip org.mozilla.jss.crypto.PQGGen 512
	echo Testing \"jssjava_g\" on `uname` `uname -r` DBG platform . . .
	../../../../../dist/SunOS5.5.1_DBG.OBJ/bin/jssjava_g -classpath ../../../../../dist/classes_DBG:/share/builds/components/jdk/1.1.6/SunOS/lib/classes.zip org.mozilla.jss.crypto.PQGGen 512


	setenv LD_LIBRARY_PATH ../../../../../dist/SunOS5.5.1_OPT.OBJ/lib:/share/builds/components/jdk/1.1.6/SunOS/lib/sparc/native_threads
	echo Testing \"jssjava\" on `uname` `uname -r` OPT platform . . .
	../../../../../dist/SunOS5.5.1_OPT.OBJ/bin/jssjava -classpath ../../../../../dist/classes:/share/builds/components/jdk/1.1.6/SunOS/lib/classes.zip org.mozilla.jss.crypto.PQGGen 512





else if ( ( `uname` == "SunOS" ) && ( `uname -r` == "5.6" ) ) then

	setenv LD_LIBRARY_PATH ../../../../../dist/SunOS5.6_DBG.OBJ/lib:/share/builds/components/jdk/1.1.6/SunOS/lib/sparc/native_threads
	echo Testing \"jssjava\" on `uname` `uname -r` DBG platform . . .
	../../../../../dist/SunOS5.6_DBG.OBJ/bin/jssjava -classpath ../../../../../dist/classes_DBG:/share/builds/components/jdk/1.1.6/SunOS/lib/classes.zip org.mozilla.jss.crypto.PQGGen 512
	echo Testing \"jssjava_g\" on `uname` `uname -r` DBG platform . . .
	../../../../../dist/SunOS5.6_DBG.OBJ/bin/jssjava_g -classpath ../../../../../dist/classes_DBG:/share/builds/components/jdk/1.1.6/SunOS/lib/classes.zip org.mozilla.jss.crypto.PQGGen 512


	setenv LD_LIBRARY_PATH ../../../../../dist/SunOS5.6_OPT.OBJ/lib:/share/builds/components/jdk/1.1.6/SunOS/lib/sparc/native_threads
	echo Testing \"jssjava\" on `uname` `uname -r` OPT platform . . .
	../../../../../dist/SunOS5.6_OPT.OBJ/bin/jssjava -classpath ../../../../../dist/classes:/share/builds/components/jdk/1.1.6/SunOS/lib/classes.zip org.mozilla.jss.crypto.PQGGen 512





endif

