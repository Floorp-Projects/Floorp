Content-Length: -1346
Received: by staff.mail.com (mbox kestes)
 (with Cubic Circle's cucipop (v1.22 1998/04/11) Wed Aug 11 11:11:37 1999)
X-From_: fetchmail-friends-request@ccil.org  Wed Aug 11 11:07:18 1999
Return-Path: <fetchmail-friends-request@ccil.org>
Received: from locke.ccil.org (slist@locke.ccil.org [192.190.237.102])
	by staff.mail.com (8.9.1/8.9.1) with ESMTP id LAA29008
	for <kestes@inamecorp.com>; Wed, 11 Aug 1999 11:07:18 -0400 (EDT)
Received: (from slist@localhost)
	by locke.ccil.org (8.8.5/8.8.5) id LAA07287;
	Wed, 11 Aug 1999 11:31:31 -0400 (EDT)
X-Authentication-Warning: locke.ccil.org: slist set sender to fetchmail-friends-request@ccil.org using -f
Message-Id: <199908110807.QAA01429@possum.os2.ami.com.au>
X-Mailer: exmh version 2.1.0 04/14/1999
X-Exmh-Isig-CompType: unknown
X-Exmh-Isig-Folder: fetchmail/1999_08
Mime-Version: 1.0
Content-Type: text/plain; charset=us-ascii
X-Mailing-List: <fetchmail-friends@ccil.org> archive/latest/3766
X-Loop: fetchmail-friends@ccil.org
Precedence: list
Resent-Date: Wed, 11 Aug 1999 11:31:31 -0400 (EDT)
Resent-Message-ID: <"5WIQvB.A.ZnB.2ZZs3"@locke.ccil.org>
Resent-From: fetchmail-friends@ccil.org
Resent-Sender: fetchmail-friends-request@ccil.org
From: John Summerfield <summer@OS2.ami.com.au>
To: fetchmail-friends@ccil.org
Subject: fetchmail doesn't quit
Date: Wed, 11 Aug 1999 16:07:16 +0800



tinderbox: tree: Project_A
tinderbox: builddate: 934395485
tinderbox: status: success
tinderbox: buildname: worms-SunOS-sparc-5.6-Depend-apprunner
tinderbox: errorparser: unix
tinderbox: buildfamily: unix
tinderbox: END


Real error messages culled from the tinderbox site


Creating .deps
../../config/./nsinstall -R -m 444 ../../../mozilla/xpcom/io/nsEscape.h ../../../mozilla/xpcom/io/nsFileSpec.h ../../../mozilla/xpcom/io/nsFileSpecStreaming.h ../../../mozilla/xpcom/io/nsFileStream.h ../../../mozilla/xpcom/io/nsIFileStream.h ../../../mozilla/xpcom/io/nsIStringStream.h ../../../mozilla/xpcom/io/nsIUnicharInputStream.h ../../../mozilla/xpcom/io/nsSpecialSystemDirectory.h ../../dist/include
../../config/./nsinstall -R -m 444 ../../../mozilla/xpcom/io/nsIFileSpec.idl ../../../mozilla/xpcom/io/nsIBaseStream.idl ../../../mozilla/xpcom/io/nsIInputStream.idl ../../../mozilla/xpcom/io/nsIOutputStream.idl ../../../mozilla/xpcom/io/nsIBufferInputStream.idl ../../../mozilla/xpcom/io/nsIBufferOutputStream.idl ../../dist/idl
../../dist/./bin/xpidl -m header -w -I ../../dist/idl -I../../../mozilla/xpcom/io -o _xpidlgen/nsIFileSpec ../../../mozilla/xpcom/io/nsIFileSpec.idl
../../dist/./bin/xpidl -m header -w -I ../../dist/idl -I../../../mozilla/xpcom/io -o _xpidlgen/nsIBaseStream ../../../mozilla/xpcom/io/nsIBaseStream.idl
../../dist/./bin/xpidl -m header -w -I ../../dist/idl -I../../../mozilla/xpcom/io -o _xpidlgen/nsIInputStream ../../../mozilla/xpcom/io/nsIInputStream.idl
../../../mozilla/xpcom/io/nsIInputStream.idl:39: Error: methods in [scriptable] interfaces which are non-scriptable because they refer to native types (parameter "buf") must be marked [noscript]

nsBaseAppCore.cpp
nsProfileCore.cpp
D:\clob\mozilla\xpfe\AppCores\src\nsProfileCore.cpp(191) : error C2660: 'DeleteProfile' : function does not take 1 parameters
NMAKE : fatal error U1077: 'cl' : return code '0x2'
Stop.
NMAKE : fatal error U1077: '"C:\Program Files\Microsoft Visual Studio\VC98\Bin\NMAKE.EXE"' : return code '0x2'
Stop.
NMAKE : fatal error U1077: '"C:\Program Files\Microsoft Visual Studio\VC98\Bin\NMAKE.EXE"' : return code '0x2'
Stop.
NMAKE : fatal error U1077: '"C:\Program Files\Microsoft Visual Studio\VC98\Bin\NMAKE.EXE"' : return code '0x2'
Stop.
NMAKE : fatal error U1077: '"C:\Program Files\Microsoft Visual Studio\VC98\Bin\NMAKE.EXE"' : return code '0x2'
Stop.
NMAKE : fatal error U1077: '"C:\Program Files\Microsoft Visual Studio\VC98\Bin\NMAKE.EXE"' : return code '0x2'
Stop.











gmake[3]: Leaving directory `/builds/tinderbox/SeaMonkey/Linux_2.2.5_clobber/mozilla/xpfe/AppCores'
cd bootstrap; gmake install
gmake[3]: Entering directory `/builds/tinderbox/SeaMonkey/Linux_2.2.5_clobber/mozilla/xpfe/bootstrap'
c++ -o nsAppRunner.o -c   -fno-rtti -fno-exceptions -Wall -Wconversion -Wpointer-arith -Wbad-function-cast -Wcast-align -Woverloaded-virtual -Wsynth -Wshadow -pedantic -Wp,-MD,.deps/nsAppRunner.pp -include ../../config-defs.h -pthread -g  -fPIC  -DDEBUG -UNDEBUG -DDEBUG_cltbld -DTRACING -DOSTYPE=\"Linux2.2\" -DNECKO  -I../../dist/include -I../../dist/./include -I../../dist/include -I/builds/tinderbox/SeaMonkey/Linux_2.2.5_clobber/mozilla/include      -I/usr/X11R6/include  -DWIDGET_DLL=\"libwidget_gtk.so\" -DGFXWIN_DLL=\"libgfx_gtk.so\" -I/usr/X11R6/include -I/usr/lib/glib/include nsAppRunner.cpp
nsAppRunner.cpp:24: nsNeckoUtil2.h: No such file or directory
gmake[3]: *** [nsAppRunner.o] Error 1
gmake[3]: Leaving directory `/builds/tinderbox/SeaMonkey/Linux_2.2.5_clobber/mozilla/xpfe/bootstrap'
gmake[2]: *** [install] Error 2
gmake[2]: Leaving directory `/builds/tinderbox/SeaMonkey/Linux_2.2.5_clobber/mozilla/xpfe'
gmake[1]: *** [install] Error 2
gmake[1]: Leaving directory `/builds/tinderbox/SeaMonkey/Linux_2.2.5_clobber/mozilla'
gmake: *** [build] Error 2
/builds/tinderbox/SeaMonkey/Linux_2.2.5_clobber/./mozilla/dist/bin/apprunner doesn't exist, is zero-size, or not executable. 


../../../dist/include/prtypes.h:266: warning: ANSI C++ does not support `long long'
nsJSAppCoresManager.cpp: In function `int GetAppCoresManagerProperty(struct JSContext *, struct JSObject *, long int, long int *)':
nsJSAppCoresManager.cpp:61: warning: unused variable `int ok'
nsJSAppCoresManager.cpp: In function `int SetAppCoresManagerProperty(struct JSContext *, struct JSObject *, long int, long int *)':
nsJSAppCoresManager.cpp:96: warning: unused variable `int ok'
c++ -o nsJSProfileCore.o -c   -fno-rtti -fno-exceptions -Wall -Wconversion -Wpointer-arith -Wbad-function-cast -Wcast-align -Woverloaded-virtual -Wsynth -Wshadow -pedantic -Wp,-MD,.deps/nsJSProfileCore.pp -include ../../../config-defs.h -pthread -g  -fPIC  -DDEBUG -UNDEBUG -DDEBUG_cltbld -DTRACING -DOSTYPE=\"Linux2.2\" -DNECKO  -I../../../dist/include -I../../../dist/./include -I../../../dist/include -I/builds/tinderbox/SeaMonkey/Linux_2.2.5_depend/mozilla/include      -I/usr/X11R6/include  nsJSProfileCore.cpp
In file included from ../../../dist/include/jspubtd.h:24,
                 from ../../../dist/include/jsapi.h:26,
                 from nsJSProfileCore.cpp:20:
../../../dist/include/jstypes.h:284: warning: ANSI C++ does not support `long long'
../../../dist/include/jstypes.h:285: warning: ANSI C++ does not support `long long'
In file included from ../../../dist/include/nscore.h:29,
                 from ../../../dist/include/nsrootidl.h:7,
                 from ../../../dist/include/nsISupports.h:8,
                 from ../../../dist/include/nsJSUtils.h:29,
                 from nsJSProfileCore.cpp:21:
../../../dist/include/prtypes.h:265: warning: ANSI C++ does not support `long long'
../../../dist/include/prtypes.h:266: warning: ANSI C++ does not support `long long'
nsJSProfileCore.cpp: In function `int GetProfileCoreProperty(struct JSContext *, struct JSObject *, long int, long int *)':
nsJSProfileCore.cpp:62: warning: unused variable `int ok'
nsJSProfileCore.cpp: In function `int SetProfileCoreProperty(struct JSContext *, struct JSObject *, long int, long int *)':
nsJSProfileCore.cpp:97: warning: unused variable `int ok'
nsJSProfileCore.cpp: In function `int ProfileCoreDeleteProfile(struct JSContext *, struct JSObject *, unsigned int, long int *, long int *)':
nsJSProfileCore.cpp:287: no matching function for call to `nsIDOMProfileCore::DeleteProfile (nsAutoString &)'
../../../dist/include/nsIDOMProfileCore.h:41: candidates are: nsIDOMProfileCore::DeleteProfile(const nsString &, const nsString &)






cd src; gmake libs
gmake[4]: Entering directory `/export2/tinderbox/SunOS_5.6_depend/mozilla/xpfe/AppCores/src'
c++ -o nsProfileCore.o -c   -fno-rtti -fno-handle-exceptions -Wp,-MD,.deps/nsProfileCore.pp -include ../../../config-defs.h -g  -fPIC  -DDEBUG -UNDEBUG -DDEBUG_mcafee -DTRACING -DOSTYPE=\"SunOS5\" -DNECKO  -I../../../dist/include -I../../../dist/./include -I../../../dist/include -I/export2/tinderbox/SunOS_5.6_depend/mozilla/include  -I../../../dist/./public/jpeg -I../../../dist/./public/png -I../../../dist/./public/zlib  -I/usr/openwin/include  nsProfileCore.cpp
nsProfileCore.cpp:186: prototype for `unsigned int nsProfileCore::DeleteProfile(const class nsString &)' does not match any in class `nsProfileCore'
nsProfileCore.h:127: candidate is: unsigned int nsProfileCore::DeleteProfile(const class nsString &, const class nsString &)
nsProfileCore.cpp: In method `unsigned int nsProfileCore::DeleteProfile(const class nsString &)':
nsProfileCore.cpp:191: too few arguments for method `unsigned int nsIProfile::DeleteProfile(const char *, const char *)'
gmake[4]: *** [nsProfileCore.o] Error 1
gmake[4]: Leaving directory `/export2/tinderbox/SunOS_5.6_depend/mozilla/xpfe/AppCores/src'
gmake[3]: *** [libs] Error 2
gmake[3]: Leaving directory `/export2/tinderbox/SunOS_5.6_depend/mozilla/xpfe/AppCores'
gmake[2]: *** [libs] Error 2
gmake[2]: Leaving directory `/export2/tinderbox/SunOS_5.6_depend/mozilla/xpfe'
gmake[1]: *** [libs] Error 2
gmake[1]: Leaving directory `/export2/tinderbox/SunOS_5.6_depend/mozilla'
gmake: *** [build] Error 2
/export2/tinderbox/SunOS_5.6_depend/./mozilla/dist/bin/apprunner doesn't exist, is zero-size, or not executable. 
export binary missing, build FAILED





cd bootstrap; gmake install
gmake[3]: Entering directory `/export2/tinderbox/SunOS_5.6_depend/mozilla/xpfe/bootstrap'
c++ -o nsAppRunner.o -c   -fno-rtti -fno-handle-exceptions -Wp,-MD,.deps/nsAppRunner.pp -include ../../config-defs.h -g  -fPIC  -DDEBUG -UNDEBUG -DDEBUG_mcafee -DTRACING -DOSTYPE=\"SunOS5\" -DNECKO  -I../../dist/include -I../../dist/./include -I../../dist/include -I/export2/tinderbox/SunOS_5.6_depend/mozilla/include  -I../../dist/./public/jpeg -I../../dist/./public/png -I../../dist/./public/zlib  -I/usr/openwin/include  -DWIDGET_DLL=\"libwidget_gtk.so\" -DGFXWIN_DLL=\"libgfx_gtk.so\" -I/usr/openwin/include -I/usr/local/lib/glib/include -I/usr/local/include nsAppRunner.cpp
nsAppRunner.cpp:24: nsNeckoUtil2.h: No such file or directory
gmake[3]: *** [nsAppRunner.o] Error 1
gmake[3]: Leaving directory `/export2/tinderbox/SunOS_5.6_depend/mozilla/xpfe/bootstrap'
gmake[2]: *** [install] Error 2
gmake[2]: Leaving directory `/export2/tinderbox/SunOS_5.6_depend/mozilla/xpfe'
gmake[1]: *** [install] Error 2
gmake[1]: Leaving directory `/export2/tinderbox/SunOS_5.6_depend/mozilla'
gmake: *** [build] Error 2
/export2/tinderbox/SunOS_5.6_depend/./mozilla/dist/bin/apprunner doesn't exist, is zero-size, or not executable. 
export binary missing, build FAILED




../../dist/include/prtypes.h:265: warning: ANSI C++ does not support `long long'
../../dist/include/prtypes.h:266: warning: ANSI C++ does not support `long long'
nsPageMgr.cpp: In method `enum PRStatus nsPageMgr::InitPages(long unsigned int, long unsigned int)':
nsPageMgr.cpp:369: ANSI C++ forbids comparison between pointer and integer
nsPageMgr.cpp:369: warning: statement with no effect
nsPageMgr.cpp: In method `void nsPageMgr::FinalizePages()':
nsPageMgr.cpp:436: warning: implicit declaration of function `int close(...)'
nsPageMgr.cpp: In method `unsigned char (* nsPageMgr::NewCluster(long unsigned int))[4096]':
nsPageMgr.cpp:718: warning: unused variable `long unsigned int size'
nsPageMgr.cpp: In method `void nsPageMgr::DestroyCluster(unsigned char (*)[4096], long unsigned int)':
nsPageMgr.cpp:741: warning: unused variable `long unsigned int size'
gmake[3]: *** [nsPageMgr.o] Error 1
gmake[3]: Leaving directory `/builds/tinderbox/SeaMonkey/Linux_2.2.5_depend/mozilla/xpcom/ds'
gmake[2]: *** [libs] Error 2
gmake[2]: Leaving directory `/builds/tinderbox/SeaMonkey/Linux_2.2.5_depend/mozilla/xpcom'
gmake[1]: *** [libs] Error 2
gmake[1]: Leaving directory `/builds/tinderbox/SeaMonkey/Linux_2.2.5_depend/mozilla'
gmake: *** [build] Error 2



g++ -o nsIFileStream.o -c   -fno-rtti -fno-handle-exceptions -Wp,-MD,.deps/nsIFileStream.pp -include ../../config-defs.h -g  -fPIC  -DDEBUG -UNDEBUG -DDEBUG_bruce -DTRACING -DOSTYPE=\"SunOS5\" -DNECKO -D_IMPL_NS_COM -D_IMPL_NS_BASE  -I../../dist/include -I../../dist/./include -I../../dist/include -I../../../mozilla/include  -I../../dist/./public/jpeg -I../../dist/./public/png -I../../dist/./public/zlib  -I/usr/openwin/include  ../../../mozilla/xpcom/io/nsIFileStream.cpp
g++ -o nsIStringStream.o -c   -fno-rtti -fno-handle-exceptions -Wp,-MD,.deps/nsIStringStream.pp -include ../../config-defs.h -g  -fPIC  -DDEBUG -UNDEBUG -DDEBUG_bruce -DTRACING -DOSTYPE=\"SunOS5\" -DNECKO -D_IMPL_NS_COM -D_IMPL_NS_BASE  -I../../dist/include -I../../dist/./include -I../../dist/include -I../../../mozilla/include  -I../../dist/./public/jpeg -I../../dist/./public/png -I../../dist/./public/zlib  -I/usr/openwin/include  ../../../mozilla/xpcom/io/nsIStringStream.cpp
g++ -o nsPipe.o -c   -fno-rtti -fno-handle-exceptions -Wp,-MD,.deps/nsPipe.pp -include ../../config-defs.h -g  -fPIC  -DDEBUG -UNDEBUG -DDEBUG_bruce -DTRACING -DOSTYPE=\"SunOS5\" -DNECKO -D_IMPL_NS_COM -D_IMPL_NS_BASE  -I../../dist/include -I../../dist/./include -I../../dist/include -I../../../mozilla/include  -I../../dist/./public/jpeg -I../../dist/./public/png -I../../dist/./public/zlib  -I/usr/openwin/include  ../../../mozilla/xpcom/io/nsPipe.cpp
g++ -o nsSegmentedBuffer.o -c   -fno-rtti -fno-handle-exceptions -Wp,-MD,.deps/nsSegmentedBuffer.pp -include ../../config-defs.h -g  -fPIC  -DDEBUG -UNDEBUG -DDEBUG_bruce -DTRACING -DOSTYPE=\"SunOS5\" -DNECKO -D_IMPL_NS_COM -D_IMPL_NS_BASE  -I../../dist/include -I../../dist/./include -I../../dist/include -I../../../mozilla/include  -I../../dist/./public/jpeg -I../../dist/./public/png -I../../dist/./public/zlib  -I/usr/openwin/include  ../../../mozilla/xpcom/io/nsSegmentedBuffer.cpp
../../../mozilla/xpcom/io/nsSegmentedBuffer.cpp: In method `nsSegmentedBuffer::nsSegmentedBuffer(unsigned int, unsigned int, nsIAllocator *)':
../../../mozilla/xpcom/io/nsSegmentedBuffer.cpp:36: no method `nsAllocator::GetGlobalAllocator'
gmake[2]: *** [nsSegmentedBuffer.o] Error 1
gmake[2]: Leaving directory `/space/mozilla/SunOS_5.6_clobber/builddir/xpcom/io'
gmake[1]: *** [libs] Error 2
gmake[1]: Leaving directory `/space/mozilla/SunOS_5.6_clobber/builddir/xpcom'
gmake: *** [libs] Error 2
/space/mozilla/SunOS_5.6_clobber/./mozilla/../builddir/webshell/tests/viewer/viewer
export binary missing, build FAILED


