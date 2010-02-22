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
 * The Original Code is mozilla pluginfinder style sheet registration code.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *  Justin Dolske <dolske@mozilla.com>
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

function pluginBindings() { }

pluginBindings.prototype = {
    // This isn't a real component, we're just using categories to
    // automatically add our stylesheets during module registration.
    classDescription: "plugin bindings",
    classID:          Components.ID("12663f3a-a311-4606-83eb-b6b9108dcc36"),
    contractID:       "@mozilla.org/plugin-bindings;1",
    QueryInterface: XPCOMUtils.generateQI([]),

    _xpcom_categories: [{ category: "agent-style-sheets",
                          entry:    "pluginfinder xbl binding",
                          value:    "chrome://mozapps/content/plugins/pluginFinderBinding.css"},
                        { category: "agent-style-sheets",
                          entry:    "pluginproblem xbl binding",
                          value:    "chrome://mozapps/content/plugins/pluginProblemBinding.css"}]
};

var NSGetModule = XPCOMUtils.generateNSGetModule([pluginBindings]);
