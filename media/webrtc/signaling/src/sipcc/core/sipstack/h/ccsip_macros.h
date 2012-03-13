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
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
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

#ifndef _CCSIP_MACROS_H_
#define _CCSIP_MACROS_H_


#define EVENT_ACTION_SM(x) \
        (gSIPHandlerTable[x])

#define UPDATE_FLAGS(x, y) \
        (x = (x == STATUS_SUCCESS) ? y : x)

#define GET_SIP_MESSAGE() sippmh_message_create()

#define REG_CHECK_EVENT_SANITY(x, y) \
        ((x - SIP_REG_STATE_BASE >= 0) && (x <= SIP_REG_STATE_END) && \
         (y - SIPSPI_REG_EV_BASE >=0)  && (y <= SIPSPI_REG_EV_END))

#define REG_EVENT_ACTION(x, y) \
        (gSIPRegSMTable[x - SIP_REG_STATE_BASE][y - SIPSPI_REG_EV_BASE])

#endif
