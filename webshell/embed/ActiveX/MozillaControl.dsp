# Microsoft Developer Studio Project File - Name="MozillaControl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=MozillaControl - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "MozillaControl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "MozillaControl.mak" CFG="MozillaControl - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "MozillaControl - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "MozillaControl - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "MozillaControl - Win32 Release"

# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f MozillaControl.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "MozillaControl.exe"
# PROP BASE Bsc_Name "MozillaControl.bsc"
# PROP BASE Target_Dir ""
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "NMAKE /f MozillaControl.mak"
# PROP Rebuild_Opt "/a"
# PROP Target_File "MozillaControl.exe"
# PROP Bsc_Name "MozillaControl.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "MozillaControl - Win32 Debug"

# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f MozillaControl.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "MozillaControl.exe"
# PROP BASE Bsc_Name "MozillaControl.bsc"
# PROP BASE Target_Dir ""
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "NMAKE /f makefile.win control_and_plugin"
# PROP Rebuild_Opt "/a"
# PROP Target_File "M:\moz\mozilla\dist\WIN32_D.OBJ\bin\npmozctl.dll"
# PROP Bsc_Name "MozillaControl.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "MozillaControl - Win32 Release"
# Name "MozillaControl - Win32 Debug"

!IF  "$(CFG)" == "MozillaControl - Win32 Release"

!ELSEIF  "$(CFG)" == "MozillaControl - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "*.cpp,*.c,*.idl,*.rc"
# Begin Source File

SOURCE=.\ActiveScriptSite.cpp
# End Source File
# Begin Source File

SOURCE=.\ActiveXPlugin.cpp
# End Source File
# Begin Source File

SOURCE=.\ActiveXPluginInstance.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlSite.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlSiteIPFrame.cpp
# End Source File
# Begin Source File

SOURCE=.\DropTarget.cpp
# End Source File
# Begin Source File

SOURCE=.\guids.cpp
# End Source File
# Begin Source File

SOURCE=.\IEHtmlDocument.cpp
# End Source File
# Begin Source File

SOURCE=.\IEHtmlElement.cpp
# End Source File
# Begin Source File

SOURCE=.\IEHtmlElementCollection.cpp
# End Source File
# Begin Source File

SOURCE=.\IEHtmlNode.cpp
# End Source File
# Begin Source File

SOURCE=.\ItemContainer.cpp
# End Source File
# Begin Source File

SOURCE=.\LegacyPlugin.cpp
# End Source File
# Begin Source File

SOURCE=.\MozillaBrowser.cpp
# End Source File
# Begin Source File

SOURCE=.\MozillaControl.cpp
# End Source File
# Begin Source File

SOURCE=.\MozillaControl.idl
# End Source File
# Begin Source File

SOURCE=.\MozillaControl.rc
# End Source File
# Begin Source File

SOURCE=.\npmozctl.def
# End Source File
# Begin Source File

SOURCE=.\npwin.cpp
# End Source File
# Begin Source File

SOURCE=.\nsSetupRegistry.cpp
# End Source File
# Begin Source File

SOURCE=.\PropertyBag.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# End Source File
# Begin Source File

SOURCE=.\WebShellContainer.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ActiveScriptSite.h
# End Source File
# Begin Source File

SOURCE=.\ActiveXPlugin.h
# End Source File
# Begin Source File

SOURCE=.\ActiveXPluginInstance.h
# End Source File
# Begin Source File

SOURCE=.\ActiveXTypes.h
# End Source File
# Begin Source File

SOURCE=.\BrowserDiagnostics.h
# End Source File
# Begin Source File

SOURCE=.\ControlSite.h
# End Source File
# Begin Source File

SOURCE=.\ControlSiteIPFrame.h
# End Source File
# Begin Source File

SOURCE=.\CPMozillaControl.h
# End Source File
# Begin Source File

SOURCE=.\DHTMLCmdIds.h
# End Source File
# Begin Source File

SOURCE=.\DropTarget.h
# End Source File
# Begin Source File

SOURCE=.\guids.h
# End Source File
# Begin Source File

SOURCE=.\IEHtmlDocument.h
# End Source File
# Begin Source File

SOURCE=.\IEHtmlElement.h
# End Source File
# Begin Source File

SOURCE=.\IEHtmlElementCollection.h
# End Source File
# Begin Source File

SOURCE=.\IEHtmlNode.h
# End Source File
# Begin Source File

SOURCE=.\IOleCommandTargetImpl.h
# End Source File
# Begin Source File

SOURCE=.\ItemContainer.h
# End Source File
# Begin Source File

SOURCE=.\MozillaBrowser.h
# End Source File
# Begin Source File

SOURCE=.\MozillaControl.h
# End Source File
# Begin Source File

SOURCE=.\PropertyBag.h
# End Source File
# Begin Source File

SOURCE=.\PropertyList.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\WebShellContainer.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\MozillaBrowser.ico
# End Source File
# Begin Source File

SOURCE=.\MozillaBrowser.rgs
# End Source File
# End Group
# Begin Group "Mozilla Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\dist\include\abouturl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\admin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\base64.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\bool.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\cdefs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\cert.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\CNavDTD.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\cnetinit.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\comi18n.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\cookies.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\COtherDTD.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\CRtfDTD.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\cvactive.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\cvchunk.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\cvcolor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\cvdisk.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\cvjscfg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\cvmime.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\cvpics.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\cvplugin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\cvsimple.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\cvunzip.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\dllcompat.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\domstubs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\dummy_nc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\fileurl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\ftpurl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\gophurl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\httpauth.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\httpurl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\if_struct.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\il.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\il_icons.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\il_strm.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\il_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\il_util.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\il_utilp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\ilIImageRenderer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\ilINetContext.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\ilINetReader.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\ilISystemServices.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\ilIURL.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\interpreter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\java_lang_String.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\javaString.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\javaThreads.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jconfig.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jdk_java_lang_String.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jerror.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jinclude.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jmc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jmorecfg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jni.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jni_md.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jpegint.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jpeglib.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jpermission.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jri.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jri_md.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jriext.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jritypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsapi.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsarena.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsarray.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsatom.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsbit.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsbool.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsclist.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jscntxt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jscompat.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsconfig.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jscpucfg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsdate.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsdbgapi.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsemit.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsfun.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsgc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jshash.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsinterp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsjava.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jslock.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jslong.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsmath.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsnum.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsobj.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsopcode.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsosdep.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsotypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsparse.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsprf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsprvtd.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jspubtd.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsregexp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsscan.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsscope.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsscript.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsstddef.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsstr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jstypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsurl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsutil.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jsxdrapi.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jvmmgr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\jwinfig.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\libimg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\MailNewsTypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mcom_db.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mdb.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mimecont.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mimecth.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mimehdrs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mimei.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mimeleaf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mimemsig.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mimemult.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mimeobj.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mimepbuf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mimerosetta.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mimetext.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mkaccess.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mkautocf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mkformat.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mkfsort.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mkgeturl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mkhelp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mkldap.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mkpadpac.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mkparse.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mkprefs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mkselect.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mksort.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mkstream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mktcp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mktrace.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\mkutils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\modlmime.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\modmime.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\modmimee.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\MsgCompGlue.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\msgCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\ncompat.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\net_xp_file.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\netcache.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\netscape_javascript_JSObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\netstream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\netutils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\ni_pixmp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nntpCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsAbBaseCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsAbCard.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsAbDirectory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsAgg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsAppCoresCIDs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsAppShellCIDs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsAutoLock.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsBrowserCIDs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsBTree.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCaps.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCapsEnums.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCapsPublicEnums.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCardDataSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCCapsManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCCapsManagerFactory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCCertPrincipal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCCodebasePrincipal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCCodeSourcePrincipal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCollationCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsColor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsColorNameIDs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsColorNames.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCom.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsComposeAppCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsComposer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCOMPtr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCoord.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nscore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCRT.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCSSAtoms.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCSSPropIDs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCSSProps.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsCSSValue.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsDateTimeFormatCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsDBFolderInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsDebug.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsDeque.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsDeviceContext.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsDirectoryDataSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsDirPrefs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsDOMCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsDOMCSSDeclaration.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsDOMEvent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsDOMEventsIIDs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsEditorCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsEmitterUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsEnumeratorUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsError.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsEscape.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsEventListenerManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsEventStateManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsExpatDTD.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsFileLocations.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsFileSpec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsFileSpecImpl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsFileSpecStreaming.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsFileStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsFont.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsFrameList.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsFrameTraversal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsGfxCIID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsGUIEvent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsHankakuToZenkakuCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsHashtable.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsHTMLContentSinkStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsHTMLEntities.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nshtmlpars.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsHTMLParts.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsHTMLTags.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsHTMLTokens.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsHTMLToTXTSinkStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsHTMLValue.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIAbBase.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIAbCard.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIAbDirectory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIAbListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIAllocator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIAppShell.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIAppShellComponent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIAppShellComponentImpl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIAppShellService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIArena.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIAtom.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIBaseStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIBinarySearchIterator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIBlender.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIBlockingNotification.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIBookmarkDataSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIBreakState.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIBrowserWindow.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIBrowsingProfile.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIBuffer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIBufferInputStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIBufferOutputStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIButton.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIByteBuffer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICapsManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICaret.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICaseConversion.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICertPrincipal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICharRepresentable.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICharsetAlias.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICharsetConverterInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICharsetConverterManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICheckButton.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIChromeRegistry.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICiter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIClipboard.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIClipboardCommands.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIClipboardOwner.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIClipView.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICmdLineService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICodebasePrincipal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICodeSourcePrincipal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICollation.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICollection.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIComboBox.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIComboboxControlFrame.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIComponentManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIComposer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIConnectionInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIContent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIContentConnector.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIContentIterator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIContentSink.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIContentViewer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIContentViewerContainer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIContextLoader.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIContextMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICookieStorage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICopyMessageListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICSSDeclaration.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICSSLoader.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICSSParser.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICSSStyleRule.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsICSSStyleSheet.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDateTimeFormat.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDBChangeListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDBFolderInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDeviceContext.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDeviceContextSpec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDeviceContextSpecFactory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDiskDocument.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDocStreamLoaderFactory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDocument.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDocumentContainer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDocumentEncoder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDocumentLoader.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDocumentLoaderObserver.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDocumentObserver.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDocumentViewer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMAppCoresManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMAttr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMBarProp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMBaseAppCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMBrowserAppCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMCDATASection.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMCharacterData.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMComment.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMComposeAppCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMCompositionListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMCookieCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMCSS2Properties.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMCSSFontFaceRule.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMCSSImportRule.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMCSSMediaRule.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMCSSPageRule.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMCSSRule.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMCSSStyleDeclaration.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMCSSStyleRule.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMCSSStyleRuleCollection.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMCSSStyleSheet.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMCSSUnknownRule.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMDocument.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMDocumentFragment.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMDocumentType.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMDOMImplementation.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMDOMPropsCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMDragListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMEditorAppCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMElementObserver.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMEntity.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMEntityReference.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMEvent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMEventCapturer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMEventListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMEventReceiver.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMEventTarget.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMFocusListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMFormListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHistory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLAnchorElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLAppletElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLAreaElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLBaseElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLBaseFontElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLBlockquoteElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLBodyElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLBRElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLButtonElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLCollection.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLDirectoryElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLDivElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLDListElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLDocument.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLFieldSetElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLFontElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLFormElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLFrameElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLFrameSetElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLHeadElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLHeadingElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLHRElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLHtmlElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLIFrameElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLImageElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLInputElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLIsIndexElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLLabelElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLLayerElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLLegendElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLLIElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLLinkElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLMapElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLMenuElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLMetaElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLModElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLObjectElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLOListElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLOptGroupElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLOptionElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLParagraphElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLParamElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLPreElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLQuoteElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLScriptElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLSelectElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLStyleElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLTableCaptionElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLTableCellElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLTableColElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLTableElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLTableRowElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLTableSectionElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLTextAreaElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLTitleElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMHTMLUListElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMImage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMInstallTriggerGlobal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMInstallVersion.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMKeyListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMLoadListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMLocation.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMMimeType.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMMimeTypeArray.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMMouseListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMMouseMotionListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMNamedNodeMap.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMNativeObjectRegistry.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMNavigator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMNode.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMNodeList.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMNodeObserver.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMNSDocument.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMNSHTMLButtonElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMNSHTMLDocument.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMNSHTMLFormElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMNSRange.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMNSUIEvent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMOption.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMPaintListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMPlugin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMPluginArray.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMPrefsCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMProcessingInstruction.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMProfileCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMRange.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMRDFCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMRenderingContext.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMScreen.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMScriptObjectFactory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMSelection.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMSelectionListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMSignonCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMStyleSheet.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMStyleSheetCollection.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMText.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMTextListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMToolbarCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMToolkitCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMUIEvent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMWalletCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMWindow.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMWindowCollection.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMXPConnectFactory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMXULDocument.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMXULElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMXULFocusTracker.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDOMXULTreeElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDragService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDragSession.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDrawingSurface.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDrawingSurfaceWin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDTD.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIDTDDebug.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIEditActionListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIEditGuiManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIEditor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIEditRules.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIElementObserver.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIEnumerator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIEventHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIEventListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIEventListenerManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIEventQueue.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIEventQueueService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIEventStateManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIExpatTokenizer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFactory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFileChooser.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFileListTransferable.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFileLocator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFileSpec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFileStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFileUtilities.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFileWidget.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFindComponent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFocusableContent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFocusTracker.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFolder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFolderListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFontMetrics.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFontNameIterator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFontRetrieverService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFontSizeIterator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIForm.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFormatConverter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFormControl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFormControlFrame.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFormManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFrame.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFrameImageLoader.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFrameReflow.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFrameSelection.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIFrameUtil.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIGenericCommandSet.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIGenericFactory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIGlobalHistory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIGuiManagerFactory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIHTMLContent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIHTMLContentContainer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIHTMLContentSink.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIHTMLCSSStyleSheet.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIHTMLDocument.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIHTMLEditor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIHTMLElementFactory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIHTMLFragmentContentSink.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIHTMLStyleSheet.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIHttpURL.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImageGroup.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImageManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImageObserver.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImageRequest.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImapExtensionSink.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImapFlagAndUidState.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIIMAPHostSessionList.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImapIncomingServer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImapLog.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImapMailFolderSink.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImapMessageSink.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImapMiscellaneousSink.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImapProtocol.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImapService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImapUrl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIImgDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIInputStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIInterfaceInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIInterfaceInfoManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIJAR.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIJRILiveConnectPlugin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIJRILiveConnectPlugInstPeer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIJRIPlugin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIJSNativeInitializer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIJSScriptObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIJVMConsole.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIJVMManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIJVMPlugin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIJVMPluginInstance.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIJVMPluginTagInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIJVMPrefsWindow.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIJVMWindow.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILabel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILayoutDebugger.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILinearIterator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILineBreaker.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILineBreakerFactory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILineIterator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILinkHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIListBox.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIListControlFrame.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIListWidget.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILiveconnect.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILiveConnectManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILiveConnectPlugin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILiveConnectPlugInstPeer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILoadAttribs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILocale.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILocaleFactory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILocalStore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILoggingSink.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsILookAndFeel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMailboxService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMailboxUrl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMalloc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsImapCore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsImapFlagAndUidState.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMAPGenericParser.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMAPHostSessionList.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsImapIncomingServer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsImapMailDatabase.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsImapMailFolder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsImapMessage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMAPNamespace.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMappingCache.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsImapProtocol.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsImapProxyEvent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsImapSearchResults.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsImapServerResponseParser.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsImapService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsImapUrl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMenuBar.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMenuItem.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMenuListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMessage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMessageView.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMessenger.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMetaCharsetService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsImgDCallbk.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsImgDecCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsImgDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMimeContentTypeHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMimeConverter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMimeEmitter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMimeObjectClassAccess.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMimeURLUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMouseListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgAccount.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgAccountManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgBiffManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgCompFields.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgCompose.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgDatabase.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgFolder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgGroupRecord.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgHdr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgHeaderParser.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgHost.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgIdentity.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgIncomingServer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgMailNewsUrl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgMailSession.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgMessageService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgOfflineNewsState.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgParseMailMsgState.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgSend.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgSendLater.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgSignature.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgThread.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIMsgVCard.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINameSpace.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINameSpaceManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINetlibURL.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINetOStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINetPlugin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINetPluginInstance.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINetService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINetSupport.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINetSupportDialogService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINNTPArticleList.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINNTPCategory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINNTPCategoryContainer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINNTPHost.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINntpIncomingServer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINNTPNewsgroup.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINNTPNewsgroupList.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINNTPNewsgroupPost.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINNTPProtocol.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINntpService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINntpUrl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsINSEvent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsInt64.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIObserver.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIObserverList.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIObserverService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIOrderIdFormater.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIOutputStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPageManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPageSequenceFrame.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIParser.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIParserFilter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIParserNode.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPICS.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPlatformCharset.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPlugin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginHost.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginInputStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginInputStream2.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginInstance.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginInstanceOwner.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginInstancePeer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginInstancePeer2.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginManager2.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginStreamInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginStreamListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginStreamPeer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginStreamPeer2.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginTagInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPluginTagInfo2.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPop3IncomingServer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPop3Service.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPop3Sink.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPop3URL.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPopUpMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPostToServer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPref.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPrefWindow.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPresContext.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPresShell.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPrincipal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPrivateDOMEvent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIProfile.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIProperties.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIProtocol.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIProtocolURLFactory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIPtr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRadioButton.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFCompositeDataSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFContainer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFContainerUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFContentModelBuilder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFContentSink.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFDataSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFDocument.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFFileSystem.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFFind.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFFTP.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFLiteral.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFNode.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFObserver.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFPurgeableDataSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFResource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFXMLDataSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRDFXMLSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIReflowCommand.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRefreshUrl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRegion.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRegistry.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRelatedLinks.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRelatedLinksHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRenderingContext.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIRenderingContextWin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISample.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIScriptContext.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIScriptContextOwner.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIScriptEventListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIScriptExternalNameSet.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIScriptGlobalObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIScriptNameSetRegistry.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIScriptNameSpaceManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIScriptObjectOwner.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIScriptSecurityManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIScrollableView.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIScrollbar.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISecureEnv.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISecureLiveconnect.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISecurityContext.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISeekablePluginStreamPeer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISelectElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIServiceManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIServiceProvider.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISimpleEnumerator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISizeOfHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISmtpService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISmtpUrl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISoftwareUpdate.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISpaceManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISpellChecker.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIStreamConverter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIStreamListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIStreamLoadableDocument.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIStreamObserver.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIStreamTransfer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIString.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIStringBundle.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIStringStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIStyleContext.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIStyledContent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIStyleFrameConstruction.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIStyleRule.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIStyleSet.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIStyleSheet.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIStyleSheetLinkingElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISupports.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISupportsArray.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISupportsUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISymantecDebugger.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsISymantecDebugManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITableCellLayout.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITabWidget.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITextAreaWidget.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITextContent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITextEditor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITextService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITextServicesDocument.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITextTransform.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITextWidget.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIThread.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIThreadManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIThrobber.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITimer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITimerCallback.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITokenizer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIToolkit.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITooltipWidget.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITransaction.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITransactionListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITransactionManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITransferable.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsITransport.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIUGenCategory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIUGenDetailCategory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIUnicharBuffer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIUnicharInputStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIUnicharStreamLoader.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIUnicodeDecodeHelper.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIUnicodeDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIUnicodeDecodeUtil.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIUnicodeEncodeHelper.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIUnicodeEncoder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIUnknownContentTypeHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIURL.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIURLGroup.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIUrlListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIUrlListenerManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIVariant.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIView.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIViewManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIViewObserver.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIWalletEditor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIWalletService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIWebShell.h
# End Source File
# Begin Source File

SOURCE=..\..\public\nsIWebShell.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIWebShellServices.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIWebShellWindow.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIWidget.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIWidgetController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIWin32Locale.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIWindowlessPlugInstPeer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIWindowListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIWindowMediator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIWordBreaker.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIWordBreakerFactory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXMLContent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXMLContentSink.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXMLDocument.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXPBaseWindow.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXPConnect.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXPCScriptable.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXPCSecurityManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXPInstallProgress.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXULChildDocument.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXULContentSink.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXULDocumentInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXULFocusTracker.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXULParentDocument.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXULPopupListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXULSortService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIXULWindowCallbacks.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsIZip.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsJARIIDs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsJSUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsjvm.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsJVMManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsJVMPluginTagInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nslayout.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsLayoutAtoms.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsLayoutCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsLoadZig.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsLocaleCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsLocalFolderSummarySpec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsLocalMailFolder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsLocalMessage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsLWBrkCIID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMailboxProtocol.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMailboxService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMailboxUrl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMailDatabase.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMailHeaders.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMargin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMessage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMessageViewDataSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMessenger.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMessengerBootstrap.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMetaCharsetCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMimeConverter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMimeObjectClassAccess.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMimeRebuffer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgAccount.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgAccountDataSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgAccountManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgAccountManagerDS.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgBaseCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgBiffManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgCompCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgCompFieldsFact.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgComposeFact.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgCompUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgCreate.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgDatabase.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgDBCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgDBFolder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgFolder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgFolderDataSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgFolderFlags.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgGroupRecord.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgHdr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgHeaderMasks.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgIdentity.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgIdentityDataSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgIncomingServer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgKeyArray.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgKeySet.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgLineBuffer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgLocalCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgLocalFolderHdrs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgMailSession.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgMD5.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgMessageDataSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgMessageFlags.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgNewsCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgProtocol.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgRDFDataSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgSendFact.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgSendLater.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgSendLaterFact.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgServerDataSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgThread.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsMsgZapIt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsNewsDatabase.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsNewsFolder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsNewsMessage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsNewsSummarySpec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsNewsUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsNNTPArticleList.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsNNTPCategoryContainer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsNNTPHost.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsNntpIncomingServer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsNNTPNewsgroup.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsNNTPNewsgroupList.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsNNTPNewsgroupPost.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsNNTPProtocol.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsNntpService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsNntpUrl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsParseMailbox.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsParser.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsParserCIID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsParserError.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nspics.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsPICSElementObserver.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsplugin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsplugindefs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsPluginsCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsPoint.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsPop3IncomingServer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsPop3Protocol.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsPop3Service.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsPop3Sink.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsPop3URL.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nspr_md.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsPrincipal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsPrivilege.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsPrivilegeManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsPrivilegeTable.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsProxyEvent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsProxyObjectManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsQuickSort.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsRBTree.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsRDFCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsRDFInterfaces.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsRDFResource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsRect.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\NSReg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsRepository.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsres.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsrootidl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsSize.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsSmtpProtocol.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsSmtpService.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsSmtpUrl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsSocketTransport.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsSoftwareUpdateIIDs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsSpecialSystemDirectory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsStr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsString.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsString2.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsStringUtil.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsStyleChangeList.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsStyleConsts.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsStyleCoord.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsStyleStruct.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsStyleUtil.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsSystemPrivilegeTable.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsTarget.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsTextFragment.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsTextServicesCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsTime.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsToken.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsTraceRefcnt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsTransactionManagerCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsTransform2D.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsUCvCnCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsUCVJA2CID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsUCVJACID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsUCvKOCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsUCvLatinCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsUCvTW2CID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsUCvTWCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsui.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsUInt32Array.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsUnicharUtilCIID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsUnitConversion.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsUrlListenerManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsUserDialogHelper.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsUserTarget.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsValidDTD.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsVector.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsViewsCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsVoidArray.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsweb.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsWellFormedDTD.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsWidgetsCID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsWidgetSupport.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsXIFDTD.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsXPComFactory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsXPIDLString.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsZig.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\nsZip.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\oobj.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\plvector.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\png.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\pngconf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\preenc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\prefapi.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\prefldap.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\profileSwitch.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\ProxyJNI.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\rdf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\remoturl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\rosetta.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\rosetta_mailnews.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\seccomon.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\secerr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\sechash.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\secnav.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\secprefs.rc
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\secrng.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\secstubn.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\secstubs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\secstubt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\sockstub.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\ssl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\sslerr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\sysmacros_md.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\tree.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\typedefs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\typedefs_md.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\uconvutil.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\VerReg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\winfile.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\xmlparse.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\xp_obs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\xpccomponents.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\xpcjsid.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\xpclog.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\xpctest.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\xpt_struct.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\xpt_xdr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\xptcall.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\xptinfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\xulstubs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\zconf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\zig.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\zip.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\zipfile.h
# End Source File
# Begin Source File

SOURCE=..\..\..\dist\include\zlib.h
# End Source File
# End Group
# Begin Group "Make Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\config\config.mak
# End Source File
# Begin Source File

SOURCE=..\..\..\config\config.mk
# End Source File
# Begin Source File

SOURCE=..\..\..\config\rules.mak
# End Source File
# End Group
# Begin Source File

SOURCE=.\makefile.win
# End Source File
# Begin Source File

SOURCE=.\mkctldef.bat
# End Source File
# End Target
# End Project
