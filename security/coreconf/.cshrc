#!/bin/csh
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
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
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
# Startup file for csh and tcsh.  It is meant to work on:
#
#	SunOS 4.1.3_U1,
#	Sun Solaris,
#	Sun Solaris on Intel,
#	SGI IRIX,
#	SGI IRIX64,
#	UNIX_SV,
#	IBM AIX,
#	Hewlett-Packard HP-UX,
#	SCO_SV,
#	FreeBSD,
#	DEC OSF/1,
#	Linux,
#	and everything else.
#

###############################################
# Set operating system name and release level #
###############################################

set os_name=`uname -s`
set os_release=`uname -r`

##########################################################
#  Set environment variables based upon operating system #
##########################################################

if ($os_name == "SunOS" && $os_release == "4.1.3_U1") then
	##############################
	# SunOS 4.1.3_U1
	#

	setenv NO_MDUPDATE 1

	set path = (	/tools/ns/soft/gcc-2.6.3/run/default/sparc_sun_sunos4.1.3_U1/bin \
			/tools/ns/bin \
			/sbin \
			/usr/bin \
			/usr/openwin/bin \
			/usr/openwin/include \
			/usr/ucb \
			/usr/local/bin \
			/etc \
			/usr/etc \
			/usr/etc/install \
			. )

else if ($os_name == "SunOS") then
	################################
	#  Assume it is Sun Solaris
	#

	# To build Navigator on Solaris 2.5, I must set the environment
	# variable NO_MDUPDATE and use gcc-2.6.3.
	setenv NO_MDUPDATE 1

	set path = (	/share/builds/components/jdk/1.2.2_01/SunOS \
			/usr/ccs/bin \
			/usr/opt/bin \
			/tools/ns/bin \
			/usr/sbin \
			/sbin \
			/usr/bin \
			/usr/dt/bin \
			/usr/openwin/bin \
			/usr/openwin/include \
			/usr/ucb \
			/usr/opt/java/bin \
			/usr/local/bin \
			/etc \
			/usr/etc \
			/usr/etc/install \
			/opt/Acrobat3/bin \
			. )

	# To get the native Solaris cc
	if (`uname -m` == i86pc) then
		set path = (	/h/solx86/export/home/opt/SUNWspro/SC3.0.1/bin \
				$path )
	else
		set path = (	/tools/ns/workshop/bin \
				/tools/ns/soft/gcc-2.6.3/run/default/sparc_sun_solaris2.4/bin \
				$path )
	endif

	setenv LD_LIBRARY_PATH /share/builds/components/jdk/1.2.2_01/SunOS/lib/sparc/native_threads

	setenv MANPATH /usr/local/man:/usr/local/lib/mh/man:/usr/local/lib/rcscvs/man:/usr/local/lib/fvwm/man:/usr/local/lib/xscreensaver/man:/usr/share/man:/usr/openwin/man:/usr/opt/man

	# For Purify
	setenv PURIFYHOME /usr/local-sparc-solaris/pure/purify-4.0-solaris2
	setenv PATH ${PURIFYHOME}:$PATH
	setenv MANPATH $PURIFYHOME/man:$MANPATH
	setenv LD_LIBRARY_PATH ${LD_LIBRARY_PATH}:$PURIFYHOME
	setenv PURIFYOPTIONS "-max_threads=1000 -follow-child-processes=yes"

else if ($os_name == "IRIX" || $os_name == "IRIX64") then
	#############
	#  SGI Irix
	#

	set path = (	/share/builds/components/jdk/1.2.1/IRIX \
			/tools/ns/bin \
			/tools/contrib/bin \
			/usr/local/bin \
			/usr/sbin \
			/usr/bsd \
			/usr/bin \
			/bin \
			/etc \
			/usr/etc \
			/usr/bin/X11 \
			. )

else if ($os_name == "UNIX_SV") then
	#################
	# UNIX_SV
	#

	set path = (	/usr/local/bin \
			/tools/ns/bin \
			/bin \
			/usr/bin \
			/usr/bin/X11 \
			/X11/bin \
			/usr/X/bin \
			/usr/ucb \
			/usr/sbin \
			/sbin \
			/usr/ccs/bin \
			. )

else if ($os_name == "AIX") then
	#################
	#  IBM AIX
	#

	set path = (	/share/builds/components/jdk/1.2.2/AIX \
			/usr/ucb/ \
			/tools/ns-arch/rs6000_ibm_aix4.1/bin \
			/tools/ns-arch/rs6000_ibm_aix3.2.5/bin \
			/share/tools/ns/soft/cvs-1.8/run/default/rs6000_ibm_aix3.2.5/bin \
			/bin \
			/usr/bin \
			/usr/ccs/bin \
			/usr/sbin \
			/usr/local/bin \
			/usr/bin/X11 \
			/usr/etc \
			/etc \
			/sbin \
			. )

else if ($os_name == "HP-UX") then
	#################
	# HP UX
	#

	set path = (	/share/builds/components/jdk/1.1.6/HP-UX \
			/usr/bin \
			/opt/ansic/bin \
			/usr/ccs/bin \
			/usr/contrib/bin \
			/opt/nettladm/bin \
			/opt/graphics/common/bin \
			/usr/bin/X11 \
			/usr/contrib/bin/X11 \
			/opt/upgrade/bin \
			/opt/CC/bin \
			/opt/aCC/bin	\
			/opt/langtools/bin \
			/opt/imake/bin \
			/etc \
			/usr/etc \
			/usr/local/bin \
			/tools/ns/bin \
			/tools/contrib/bin \
			/usr/sbin \
			/usr/local/bin \
			/tools/ns/bin \
			/tools/contrib/bin \
			/usr/sbin \
			/usr/include/X11R5 \
			. )

else if ($os_name == "SCO_SV") then
	#################
	# SCO
	#

	set path = (	/bin \
			/usr/bin \
			/tools/ns/bin \
			/tools/contrib/bin \
			/usr/sco/bin \
			/usr/bin/X11 \
			/usr/local/bin \
			. )

else if ($os_name == "FreeBSD") then
	#################
	# FreeBSD
	#

	setenv PATH /usr/bin:/bin:/usr/sbin:/sbin:/usr/X11R6/bin:/usr/local/java/bin:/usr/local/bin:/usr/ucb:/usr/ccs/bin:/tools/contrib/bin:/tools/ns/bin:.

else if ($os_name == "OSF1") then
	#################
	# DEC OSF1
	#

	set path = (	/share/builds/components/jdk/1.2.2_3/OSF1 \
			/tools/ns-arch/alpha_dec_osf4.0/bin \
			/tools/ns-arch/soft/cvs-1.8.3/run/default/alpha_dec_osf2.0/bin \
			/usr/local-alpha-osf/bin \
			/usr3/local/bin \
			/usr/local/bin \
			/usr/sbin \
			/usr/bin \
			/bin \
			/usr/bin/X11 \
			/usr/ucb \
			. )

else if ($os_name == "Linux") then
	#################
	# Linux
	#

	set path = (	/share/builds/components/jdk/1.2.2/Linux \
			$path )

endif

###############################
# Reset any "tracked" aliases #
###############################

rehash
