#!/bin/sh
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
# Startup file for sh, ksh and bash.  It is meant to work on:
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

OS_NAME=`uname -s`
export OS_NAME

OS_RELEASE=`uname -r`
export OS_RELEASE

##########################################################
#  Set environment variables based upon operating system #
##########################################################

case $OS_NAME in

	SunOS)
		##############################
		# Sun
		#

		case $OS_RELEASE in

			4.1.3_U1)
				##############################
				# SunOS 4.1.3_U1
				#

				NO_MDUPDATE=1
				export NO_MDUPDATE

				PATH=/tools/ns/soft/gcc-2.6.3/run/default/sparc_sun_sunos4.1.3_U1/bin:tools/ns/bin:/sbin:/usr/bin:/usr/openwin/bin:/usr/openwin/include:/usr/ucb:/usr/local/bin:/etc:/usr/etc:/usr/etc/install:.
				export PATH
				;;

			*)
				################################
				#  Assume it is Sun Solaris
				#

				# To build Navigator on Solaris 2.5, I must set the environment
				# variable NO_MDUPDATE and use gcc-2.6.3.
				NO_MDUPDATE=1
				export NO_MDUPDATE

				PATH=/share/builds/components/jdk/1.2.2_01/SunOS:/usr/ccs/bin:/usr/opt/bin:/tools/ns/bin:/usr/sbin:/sbin:/usr/bin:/usr/dt/bin:/usr/openwin/bin:/usr/openwin/include:/usr/ucb:/usr/opt/java/bin:/usr/local/bin:/etc:/usr/etc:/usr/etc/install:/opt/Acrobat3/bin:.
				export PATH

				# To get the native Solaris cc
				OS_TEST=`uname -m`
				export OS_TEST

				case $OS_TEST in

					i86pc)
						PATH=/h/solx86/export/home/opt/SUNWspro/SC3.0.1/bin:$PATH
						export PATH
						;;

					*)
						PATH=/tools/ns/workshop/bin:/tools/ns/soft/gcc-2.6.3/run/default/sparc_sun_solaris2.4/bin:$PATH
						export PATH
						;;
				esac

				LD_LIBRARY_PATH=/share/builds/components/jdk/1.2.2_01/SunOS/lib/sparc/native_threads
				export LD_LIBRARY_PATH

				MANPATH=/usr/local/man:/usr/local/lib/mh/man:/usr/local/lib/rcscvs/man:/usr/local/lib/fvwm/man:/usr/local/lib/xscreensaver/man:/usr/share/man:/usr/openwin/man:/usr/opt/man
				export MANPATH

				# For Purify
				PURIFYHOME=/usr/local-sparc-solaris/pure/purify-4.0-solaris2
				export PURIFYHOME
				PATH=/usr/local-sparc-solaris/pure/purify-4.0-solaris2:$PATH
				export PATH
				MANPATH=$PURIFYHOME/man:$MANPATH
				export MANPATH
				LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local-sparc-solaris/pure/purify-4.0-solaris2
				export LD_LIBRARY_PATH
				PURIFYOPTIONS="-max_threads=1000 -follow-child-processes=yes"
				export PURIFYOPTIONS
				;;
		esac
		;;

	IRIX | IRIX64)
		#############
		#  SGI Irix
		#

		PATH=/share/builds/components/jdk/1.2.1/IRIX:/tools/ns/bin:/tools/contrib/bin:/usr/local/bin:/usr/sbin:/usr/bsd:/usr/bin:/bin:/etc:/usr/etc:/usr/bin/X11:.
		export PATH
		;;

	UNIX_SV)
		#################
		# UNIX_SV
		#

		PATH=/usr/local/bin:/tools/ns/bin:/bin:/usr/bin:/usr/bin/X11:/X11/bin:/usr/X/bin:/usr/ucb:/usr/sbin:/sbin:/usr/ccs/bin:.
		export PATH
		;;

	AIX)
		#################
		#  IBM AIX
		#

		PATH=/share/builds/components/jdk/1.2.2/AIX:/usr/ucb/:/tools/ns-arch/rs6000_ibm_aix4.1/bin:/tools/ns-arch/rs6000_ibm_aix3.2.5/bin:/share/tools/ns/soft/cvs-1.8/run/default/rs6000_ibm_aix3.2.5/bin:/bin:/usr/bin:/usr/ccs/bin:/usr/sbin:/usr/local/bin:/usr/bin/X11:/usr/etc:/etc:/sbin:.
		export PATH
		;;

	HP-UX)
		#################
		# HP UX
		#

		PATH=/share/builds/components/jdk/1.1.6/HP-UX:/usr/bin:/opt/ansic/bin:/usr/ccs/bin:/usr/contrib/bin:/opt/nettladm/bin:/opt/graphics/common/bin:/usr/bin/X11:/usr/contrib/bin/X11:/opt/upgrade/bin:/opt/CC/bin:/opt/aCC/bin:/opt/langtools/bin:/opt/imake/bin:/etc:/usr/etc:/usr/local/bin:/tools/ns/bin:/tools/contrib/bin:/usr/sbin:/usr/local/bin:/tools/ns/bin:/tools/contrib/bin:/usr/sbin:/usr/include/X11R5:.
		export PATH
		;;

	SCO_SV)
		#################
		# SCO
		#

		PATH=/bin:/usr/bin:/tools/ns/bin:/tools/contrib/bin:/usr/sco/bin:/usr/bin/X11:/usr/local/bin:.
		export PATH
		;;

	FreeBSD)

		#################
		# FreeBSD
		#

		PATH=/usr/bin:/bin:/usr/sbin:/sbin:/usr/X11R6/bin:/usr/local/java/bin:/usr/local/bin:/usr/ucb:/usr/ccs/bin:/tools/contrib/bin:/tools/ns/bin:.
		export PATH
		;;

	OSF1)
		#################
		# DEC OSF1
		#

		PATH=/share/builds/components/jdk/1.2.2_3/OSF1:/tools/ns-arch/alpha_dec_osf4.0/bin:/tools/ns-arch/soft/cvs-1.8.3/run/default/alpha_dec_osf2.0/bin:/usr/local-alpha-osf/bin:/usr3/local/bin:/usr/local/bin:/usr/sbin:/usr/bin:/bin:/usr/bin/X11:/usr/ucb:.
		export PATH
		;;

	Linux)

		#################
		# Linux
		#

		PATH=/share/builds/components/jdk/1.2.2/Linux:$PATH
		export PATH
		;;
esac

###############################
# Reset any "tracked" aliases #
###############################

hash -r
