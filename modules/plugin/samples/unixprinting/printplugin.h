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
 * The Initial Developer of the Original Code is
 * Dantifer Dang <dantifer.dang@sun.com>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <roland.mainz@nrubsig.org>
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


#ifndef UNIXPRINTING_SAMPLEPLUGIN_H
#define UNIXPRINTING_SAMPLEPLUGIN_H 1

#define MIME_TYPES_HANDLED "application/x-print-unix-nsplugin:.pnt:Demo Print Plugin for Unix/Linux"
#define PLUGIN_NAME         "Demo Print Plugin for unix/linux"
#define PLUGIN_DESCRIPTION   "The demo print plugin for unix."

typedef struct _PluginInstance
{
    uint16       mode;
#ifdef MOZ_X11
    Window       window;
    Display     *display;
#endif /* MOZ_X11 */
    uint32       x,
                 y;
    uint32       width,
                 height;
    NPMIMEType   type;
    char        *message;

    NPP          instance;
    char        *pluginsPrintMessage;
    NPBool       pluginsHidden;
#ifdef MOZ_X11
    Visual      *visual;
    Colormap     colormap;
#endif /* MOZ_X11 */
    unsigned int depth;

    NPBool       exists;  /* Does the widget already exist? */
} PluginInstance;


/* Extern functions */
extern NPMIMEType dupMimeType(NPMIMEType type);
extern void printScreenMessage(PluginInstance *This);
extern void printEPSMessage(PluginInstance *This, FILE *output, NPWindow window);

#endif /* !UNIXPRINTING_SAMPLEPLUGIN_H */

