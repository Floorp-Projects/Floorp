rem common include files
rem --------------------

copy ..\..\..\dist\include\nsCOMPtr.h mozilla\include
copy ..\..\..\dist\include\nsCRT.h mozilla\include
copy ..\..\..\dist\include\nsCom.h mozilla\include
copy ..\..\..\dist\include\nsComponentManagerUtils.h mozilla\include
copy ..\..\..\dist\include\nsCppSharedAllocator.h mozilla\include
copy ..\..\..\dist\include\nsDebug.h mozilla\include
copy ..\..\..\dist\include\nsError.h mozilla\include
copy ..\..\..\dist\include\nsIAllocator.h mozilla\include
copy ..\..\..\dist\include\nsIAtom.h mozilla\include
copy ..\..\..\dist\include\nsIBaseStream.h mozilla\include
copy ..\..\..\dist\include\nsIComponentManager.h mozilla\include
copy ..\..\..\dist\include\nsID.h mozilla\include
copy ..\..\..\dist\include\nsIEnumerator.h mozilla\include
copy ..\..\..\dist\include\nsIFactory.h mozilla\include
copy ..\..\..\dist\include\nsIFileUtilities.h mozilla\include
copy ..\..\..\dist\include\nsIID.h mozilla\include
copy ..\..\..\dist\include\nsIInputStream.h mozilla\include
copy ..\..\..\dist\include\nsIOutputStream.h mozilla\include
copy ..\..\..\dist\include\nsIPlugin.h mozilla\include
copy ..\..\..\dist\include\nsIPluginInputStream.h mozilla\include
copy ..\..\..\dist\include\nsIPluginInputStream2.h mozilla\include
copy ..\..\..\dist\include\nsIPluginInstance.h mozilla\include
copy ..\..\..\dist\include\nsIPluginInstancePeer.h mozilla\include
copy ..\..\..\dist\include\nsIPluginManager.h mozilla\include
copy ..\..\..\dist\include\nsIPluginManager2.h mozilla\include
copy ..\..\..\dist\include\nsIPluginStreamInfo.h mozilla\include
copy ..\..\..\dist\include\nsIPluginStreamListener.h mozilla\include
copy ..\..\..\dist\include\nsIPluginTagInfo.h mozilla\include
copy ..\..\..\dist\include\nsIPluginTagInfo2.h mozilla\include
copy ..\..\..\dist\include\nsIServiceManager.h mozilla\include
copy ..\..\..\dist\include\nsISupports.h mozilla\include
copy ..\..\..\dist\include\nsISupportsUtils.h mozilla\include
copy ..\..\..\dist\include\nsIWindowlessPlugInstPeer.h mozilla\include
copy ..\..\..\dist\include\nsRepository.h mozilla\include
copy ..\..\..\dist\include\nsStr.h mozilla\include
copy ..\..\..\dist\include\nsString.h mozilla\include
copy ..\..\..\dist\include\nsString2.h mozilla\include
copy ..\..\..\dist\include\nsTraceRefcnt.h mozilla\include
copy ..\..\..\dist\include\nscore.h mozilla\include
copy ..\..\..\dist\include\nsplugin.h mozilla\include
copy ..\..\..\dist\include\nsplugindefs.h mozilla\include
copy ..\..\..\dist\include\nsrootidl.h mozilla\include

rem debug specific include files
rem ----------------------------

copy ..\..\..\dist\Win32_d.obj\include\plhash.h mozilla\debug\include
copy ..\..\..\dist\Win32_d.obj\include\plstr.h mozilla\debug\include
copy ..\..\..\dist\Win32_d.obj\include\pratom.h mozilla\debug\include
copy ..\..\..\dist\Win32_d.obj\include\prcpucfg.h mozilla\debug\include
copy ..\..\..\dist\Win32_d.obj\include\prlock.h mozilla\debug\include
copy ..\..\..\dist\Win32_d.obj\include\prlong.h mozilla\debug\include
copy ..\..\..\dist\Win32_d.obj\include\prtime.h mozilla\debug\include
copy ..\..\..\dist\Win32_d.obj\include\prtypes.h mozilla\debug\include

copy ..\..\..\dist\Win32_d.obj\include\obsolete\protypes.h mozilla\debug\include\obsolete

rem release specific include files
rem ------------------------------

copy ..\..\..\dist\Win32_o.obj\include\plhash.h mozilla\release\include
copy ..\..\..\dist\Win32_o.obj\include\plstr.h mozilla\release\include
copy ..\..\..\dist\Win32_o.obj\include\pratom.h mozilla\release\include
copy ..\..\..\dist\Win32_o.obj\include\prcpucfg.h mozilla\release\include
copy ..\..\..\dist\Win32_o.obj\include\prlock.h mozilla\release\include
copy ..\..\..\dist\Win32_o.obj\include\prlong.h mozilla\release\include
copy ..\..\..\dist\Win32_o.obj\include\prtime.h mozilla\release\include
copy ..\..\..\dist\Win32_o.obj\include\prtypes.h mozilla\release\include

copy ..\..\..\dist\Win32_o.obj\include\obsolete\protypes.h mozilla\release\include\obsolete

rem debug specific libs
rem -------------------

copy ..\..\..\dist\Win32_d.obj\lib\xpcom.lib mozilla\debug\lib

rem release specific libs
rem ---------------------

copy ..\..\..\dist\Win32_o.obj\lib\xpcom.lib mozilla\release\lib
