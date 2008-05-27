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
 * The Original Code is InnoTek Plugin Wrapper code.
 *
 * The Initial Developer of the Original Code is
 * InnoTek Systemberatung GmbH.
 * Portions created by the Initial Developer are Copyright (C) 2003-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   InnoTek Systemberatung GmbH / Knut St. Osmundsen
 *   Peter Weilbacher <mozilla@weilbacher.org>
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

/*
 *  Generate moz_IDs_Generated.h from moz_IDs_Input.lst
 */

/*
 * Read the input
 */
aIDs.0 = 0;
parse arg sIn sDummy
if (sIn = '') then
    sIn = 'moz_IDs_Input.lst';
do while lines(sIn)
    sLine = strip(linein(sIn));
    if (sLine <> '' & left(sLine,1) <> '#' & left(sLine,1) <> ';') then
    do
        i = aIDs.0 + 1;
        aIDs.0 = i;
        parse var sLine aIDs.i.sClass aIDs.i.sDefine aIDs.i.sConst
        if (aIDs.i.sDefine = '') then
        do
            if (left(aIDs.i.sClass, 2) == 'ns') then
                aIDs.i.sDefine = 'NS_'||substr(aIDs.i.sClass, 3)||'_IID';
            else
                aIDs.i.sDefine = 'NS_'||aIDs.i.sClass||'_IID';
            aIDs.i.sDefine = translate(aIDs.i.sDefine);
        end
        if (aIDs.i.sConst = '') then
        do
            if (left(aIDs.i.sClass, 3) == 'nsI') then
                aIDs.i.sConst = 'k'||substr(aIDs.i.sClass, 4)||'IID';
            else if (left(aIDs.i.sClass, 2) == 'ns') then
                aIDs.i.sConst = 'k'||substr(aIDs.i.sClass, 3)||'IID';
            else
                aIDs.i.sConst = 'k'||aIDs.i.sClass||'IID';
        end
    end
end
call stream sIn, 'c', 'close';


/*
 * Make output.
 */
say '/* ***** BEGIN LICENSE BLOCK *****'
say ' * Version: MPL 1.1/GPL 2.0/LGPL 2.1'
say ' *'
say ' * The contents of this file are subject to the Mozilla Public License Version'
say ' * 1.1 (the "License"); you may not use this file except in compliance with'
say ' * the License. You may obtain a copy of the License at'
say ' * http://www.mozilla.org/MPL/'
say ' *'
say ' * Software distributed under the License is distributed on an "AS IS" basis,'
say ' * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License'
say ' * for the specific language governing rights and limitations under the'
say ' * License.'
say ' *'
say ' * The Original Code is InnoTek Plugin Wrapper code.'
say ' *'
say ' * The Initial Developer of the Original Code is'
say ' * InnoTek Systemberatung GmbH.'
say ' * Portions created by the Initial Developer are Copyright (C) 2003-2005'
say ' * the Initial Developer. All Rights Reserved.'
say ' *'
say ' * Contributor(s):'
say ' *   InnoTek Systemberatung GmbH / Knut St. Osmundsen'
say ' *   Peter Weilbacher <mozilla@weilbacher.org>'
say ' *'
say ' * Alternatively, the contents of this file may be used under the terms of'
say ' * either the GNU General Public License Version 2 or later (the "GPL"), or'
say ' * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),'
say ' * in which case the provisions of the GPL or the LGPL are applicable instead'
say ' * of those above. If you wish to allow use of your version of this file only'
say ' * under the terms of either the GPL or the LGPL, and not to allow others to'
say ' * use your version of this file under the terms of the MPL, indicate your'
say ' * decision by deleting the provisions above and replace them with the notice'
say ' * and other provisions required by the GPL or the LGPL. If you do not delete'
say ' * the provisions above, a recipient may use your version of this file under'
say ' * the terms of any one of the MPL, the GPL or the LGPL.'
say ' *'
say ' * ***** END LICENSE BLOCK ***** */'
say ''
say '/*'
say ' * ID constants.'
say ' */'
say ''
say '/*******************************************************************************'
say '*   Defined Constants And Macros                                               *'
say '*******************************************************************************/'
say '#ifndef NP_DEF_ID'
say '#define NP_DEF_ID(_name, _iidspec) extern const nsIID _name'
say '#endif'
say ''
say ''
say '/*******************************************************************************'
say '*   Global Variables                                                           *'
say '*******************************************************************************/'
say '/** @name Component and Interface IDs Constants.'
say ' * @{'
say ' */'
do i = 1 to aIDs.0
    say '#ifdef '||aIDs.i.sDefine
    say 'NP_DEF_ID('||aIDs.i.sConst', '||aIDs.i.sDefine||');'
    say '#endif'
end
say ''
say ''
say ''
say '#ifdef NP_INCL_LOOKUP'
say '/**'
say ' * Lookup list for IIDs and CIDs to get an understandable name.'
say ' */'
say 'static struct nsIDNameLookupEntry'
say '{'
say '    const nsID *    pID;'
say '    const char *    pszName;'
say '}   aIDNameLookup[] ='
say '{'
do i = 1 to aIDs.0
    say '#ifdef '||aIDs.i.sDefine
    say '    { &'||aIDs.i.sConst', "'||aIDs.i.sDefine||'" },'
    say '#endif'
end
say '};'
say ''
say ''
say ''
say '/**'
say ' * Lookup list for IIDs and CIDs to get an understandable name.'
say ' */'
say 'static struct nsLookupStrIDEntry'
say '{'
say '    const nsID *    pID;'
say '    const char *    pszStrID;'
say '}   aIDStrIDLookup[] ='
say '{'
do i = 1 to aIDs.0
    say '#ifdef '||aIDs.i.sDefine||'_STR'
    say '    { &'||aIDs.i.sConst', '||aIDs.i.sDefine||'_STR },'
    say '#endif'
end
say '};'
say '#endif'
say ''

exit(0);
