/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 	 Christopher Seawood <cls@seawood.org>
 *   Chris Waterson <waterson@netscape.com>
 *   Benjamin Smedberg <bsmedberg@covad.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define XPCOM_TRANSLATE_NSGM_ENTRY_POINT 1

#include "nsIGenericFactory.h"
#include "nsXPCOM.h"
#include "nsStaticComponents.h"
#include "nsMemory.h"

#define NSGETMODULE(_name) _name##_NSGetModule

#ifdef MOZ_MATHML
#define MATHML_MODULES MODULE(nsUCvMathModule)
#else
#define MATHML_MODULES
#endif

#ifdef XP_WIN
#define INTL_COMPAT_MODULES MODULE(I18nCompatibility)
#else
#define INTL_COMPAT_MODULES
#endif

#ifdef NECKO2
#define NECKO2_MODULES MODULE(necko_secondary_protocols)
#else
#define NECKO2_MODULES
#endif

#ifdef MOZ_IPCD
#define IPC_MODULE MODULE(ipcdclient)
#else
#define IPC_MODULE
#endif

#ifdef MOZ_ENABLE_POSTSCRIPT
#define POSTSCRIPT_MODULES MODULE(nsGfxPSModule)
#else
#define POSTSCRIPT_MODULES
#endif

#ifdef XP_WIN
#  define GFX_MODULES MODULE(nsGfxModule)
#  define WIDGET_MODULES MODULE(nsWidgetModule)
#elif defined(XP_MACOSX)
#  define GFX_MODULES MODULE(nsGfxMacModule)
#  define WIDGET_MODULES MODULE(nsWidgetMacModule)
#elif defined(XP_BEOS)
#  define GFX_MODULES MODULE(nsGfxBeOSModule)
#  define WIDGET_MODULES MODULE(nsWidgetBeOSModule)
#elif defined(XP_OS2)
#  define GFX_MODULES MODULE(nsGfxOS2Module)
#  define WIDGET_MODULES MODULE(nsWidgetOS2Module)
#endif

#ifdef MOZ_ENABLE_CAIRO_GFX
#define GFX_MODULES MODULE(nsGfxModule)
#else
#  if defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_GTK2)
#  define GFX_MODULES MODULE(nsGfxGTKModule)
#  elif defined(MOZ_WIDGET_QT)
#  define GFX_MODULES MODULE(nsGfxQtModule)
#  elif defined(MOZ_WIDGET_XLIB)
#  define GFX_MODULES MODULE(nsGfxXlibModule)
#  elif defined(MOZ_WIDGET_PHOTON)
#  define GFX_MODULES MODULE(nsGfxPhModule)
#  endif
#endif

#ifdef ICON_DECODER
#define ICON_MODULE MODULE(nsIconDecoderModule)
#else
#define ICON_MODULE
#endif

#ifdef MOZ_WIDGET_GTK
#define WIDGET_MODULES MODULE(nsWidgetGTKModule)
#endif
#ifdef MOZ_WIDGET_GTK2
#define WIDGET_MODULES MODULE(nsWidgetGtk2Module)
#endif
#ifdef MOZ_WIDGET_XLIB
#define WIDGET_MODULES MODULE(nsWidgetXLIBModule)
#endif
#ifdef MOZ_WIDGET_PHOTON
#define WIDGET_MODULES MODULE(nsWidgetPhModule)
#endif
#ifdef MOZ_WIDGET_QT
#define WIDGET_MODULES MODULE(nsWidgetQtModule)
#endif

#ifdef MOZ_ENABLE_XPRINT
#define XPRINT_MODULES MODULE(nsGfxXprintModule)
#else
#define XPRINT_MODULES
#endif

#ifdef OJI
#define OJI_MODULES MODULE(nsCJVMManagerModule)
#else
#define OJI_MODULES
#endif

#ifdef ACCESSIBILITY
#define ACCESS_MODULES MODULE(nsAccessibilityModule)
#else
#define ACCESS_MODULES
#endif

#ifdef MOZ_ENABLE_XREMOTE
#define XREMOTE_MODULES MODULE(RemoteServiceModule)
#else
#define XREMOTE_MODULES
#endif

#define XUL_MODULES                          \
    MODULE(xpcomObsoleteModule)              \
    MODULE(xpconnect)                        \
    MATHML_MODULES                           \
    MODULE(nsUConvModule)                    \
    MODULE(nsI18nModule)                     \
    INTL_COMPAT_MODULES                      \
    MODULE(necko_core_and_primary_protocols) \
    NECKO2_MODULES                           \
    IPC_MODULE                               \
    MODULE(nsJarModule)                      \
    MODULE(nsPrefModule)                     \
    MODULE(nsSecurityManagerModule)          \
    MODULE(nsRDFModule)                      \
    MODULE(nsParserModule)                   \
    POSTSCRIPT_MODULES                       \
    GFX_MODULES                              \
    WIDGET_MODULES                           \
    MODULE(nsImageLib2Module)                \
    ICON_MODULE                              \
    MODULE(nsPluginModule)                   \
    MODULE(nsLayoutModule)                   \
    MODULE(docshell_provider)                \
    MODULE(embedcomponents)                  \
    MODULE(Browser_Embedding_Module)         \
    MODULE(nsEditorModule)                   \
    OJI_MODULES                              \
    ACCESS_MODULES                           \
    MODULE(appshell)                         \
    MODULE(nsTransactionManagerModule)       \
    MODULE(nsComposerModule)                 \
    MODULE(nsChromeModule)                   \
    MODULE(nsMorkModule)                     \
    MODULE(nsFindComponent)                  \
    MODULE(application)                      \
    MODULE(Apprunner)                        \
    MODULE(CommandLineModule)                \
    MODULE(nsToolkitCompsModule)             \
    XREMOTE_MODULES                          \
    MODULE(nsSoftwareUpdate)                 \
    MODULE(JavaScript_Debugger)              \
    MODULE(PKI)                              \
    MODULE(BOOT)                             \
    MODULE(NSS)                              \
    /* end of list */

#define MODULE(_name) \
NSGETMODULE_ENTRY_POINT(_name) (nsIComponentManager*, nsIFile*, nsIModule**);

XUL_MODULES

#undef MODULE

#define MODULE(_name) { #_name, NSGETMODULE(_name) },

/**
 * The nsStaticModuleInfo
 */
static nsStaticModuleInfo const gStaticModuleInfo[] = {
	XUL_MODULES
};

nsStaticModuleInfo const *const kPStaticModules = gStaticModuleInfo;
PRUint32 const kStaticModuleCount = NS_ARRAY_LENGTH(gStaticModuleInfo);
