/* vim:set ts=2 sw=2 sts=2 cin et: */
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
 * The Original Code is the Mozilla icon channel for Qt.
 *
 * The Initial Developer of the Original Code is Nokia
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Oleg Romashin <romaxa@gmail.com>
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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");


function GtkQtIconsConverter() { };
GtkQtIconsConverter.prototype = {
  classDescription: "Gtk Qt stock icons converter",
  classID:          Components.ID("{c0783c34-a831-40c6-8c03-98c9f74cca45}"),
  contractID:       "@mozilla.org/gtkqticonsconverter;1",
  QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsIGtkQtIconsConverter]),
  convert: function(icon) { return this._gtk_qt_icons_table[icon]; },
  _gtk_qt_icons_table: {
    'about':
    0,
    'add':
    0,
    'apply':
    44, /* QStyle::SP_DialogApplyButton */
    'cancel':
    39, /* QStyle::SP_DialogCancelButton */
    'clear':
    45, /* QStyle::SP_DialogResetButton  */
    'color-picker':
    0,
    'copy':
    0,
    'close':
    43, /* QStyle::SP_DialogCloseButton */
    'cut':
    0,
    'delete':
    0,
    'dialog-error':
    0,
    'dialog-info':
    0,
    'dialog-question':
    12, /* QStyle::SP_MessageBoxQuestion */
    'dialog-warning':
    10, /* QStyle::SP_MessageBoxWarning */
    'directory':
    37, /* QStyle::SP_DirIcon */
    'file':
    24, /* QStyle::SP_FileIcon */
    'find':
    0,
    'go-back-ltr':
    53, /* QStyle::SP_ArrowBack */
    'go-back-rtl':
    53, /* QStyle::SP_ArrowBack */
    'go-back':
    53, /* QStyle::SP_ArrowBack */
    'go-forward-ltr':
    54, /* QStyle::SP_ArrowForward */
    'go-forward-rtl':
    54, /* QStyle::SP_ArrowForward */
    'go-forward':
    54, /* QStyle::SP_ArrowForward */
    'go-up':
    49, /* QStyle::SP_ArrowUp */
    'goto-first':
    0,
    'goto-last':
    0,
    'help':
    7, /* QStyle::SP_TitleBarContextHelpButton */
    'home':
    55, /* QStyle::SP_DirHomeIcon */
    'info':
    9, /* QStyle::SP_MessageBoxInformation */
    'jump-to':
    0,
    'media-pause':
    0,
    'media-play':
    0,
    'network':
    20, /* QStyle::SP_DriveNetIcon */
    'no':
    48, /* QStyle::SP_DialogNoButton */
    'ok':
    38, /* QStyle::SP_DialogOkButton */
    'open':
    21, /* QStyle::SP_DirOpenIcon */
    'orientation-landscape':
    0,
    'orientation-portrait':
    0,
    'paste':
    0,
    'preferences':
    34, /* QStyle::SP_FileDialogContentsView */
    'print-preview':
    0,
    'print':
    0,
    'properties':
    0,
    'quit':
    0,
    'redo':
    0,
    'refresh':
    58, /* QStyle::SP_BrowserReload */
    'remove':
    0,
    'revert-to-saved':
    0,
    'save-as':
    42, /* QStyle::SP_DialogSaveButton */
    'save':
    42, /* QStyle::SP_DialogSaveButton */
    'select-all':
    0,
    'select-font':
    0,
    'stop':
    59, /* QStyle::SP_BrowserStop */
    'undelete':
    0,
    'undo':
    0,
    'yes':
    47, /* QStyle::SP_DialogYesButton */
    'zoom-100':
    0,
    'zoom-in':
    0,
    'zoom-out':
    0
  },
}
var components = [GtkQtIconsConverter];
function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule(components);
}

