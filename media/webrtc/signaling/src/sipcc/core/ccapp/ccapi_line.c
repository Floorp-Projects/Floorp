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

#include "cpr_stdlib.h"
#include "string_lib.h"
#include "cc_call_feature.h"
#include "ccapi_snapshot.h"
#include "ccapi_line.h"

extern cc_line_info_t lineInfo[MAX_CONFIG_LINES+1];

/**
 * Get reference handle for the line
 * @return cc_call_handle_t - handle of the call created
 */
cc_lineinfo_ref_t CCAPI_Line_getLineInfo(cc_uint32_t lineID)
{
  cc_line_info_t *line_info = NULL;
  int i; 

  for (i=1;i<=MAX_CONFIG_LINES;i++) {
      if ( (cc_uint32_t)lineInfo[i].button == lineID ) {
          line_info = (cc_line_info_t*)cpr_malloc(sizeof(cc_line_info_t));
   
          if (line_info) {
              *line_info = lineInfo[i];
              line_info->ref_count = 1;
              line_info->name = strlib_copy(lineInfo[i].name);
              line_info->dn = strlib_copy(lineInfo[i].dn);
              line_info->cfwd_dest = strlib_copy(lineInfo[i].cfwd_dest);
              line_info->externalNumber = 
                  strlib_copy(lineInfo[i].externalNumber);
          }
      }
  }
  return line_info;
}

/**
 * Create a call on the line
 * @param [in] line - lineID
 * @return cc_call_handle_t - handle of the call created
 */
cc_call_handle_t CCAPI_Line_CreateCall(cc_lineid_t line)
{
 // do we need to check the line on which this gets created?
  return CC_createCall(line);
}

/**
 * Reatin the lineInfo snapshot
 * @param cc_callinfo_ref_t - refrence to the block to be retained
 * @return void
 */
void CCAPI_Line_retainLineInfo(cc_lineinfo_ref_t ref){
    cc_line_info_t *line_info = ref;

    line_info->ref_count++;
}
/**
 * Free the lineInfo snapshot
 * @param cc_callinfo_ref_t - refrence to the block to be freed
 * @return void
 */
void CCAPI_Line_releaseLineInfo(cc_lineinfo_ref_t ref){
    cc_line_info_t *line_info = ref;

    if (line_info) {
	line_info->ref_count--;
	if ( line_info->ref_count == 0) {
            strlib_free(line_info->name);
            strlib_free(line_info->dn);
            strlib_free(line_info->cfwd_dest);
            strlib_free(line_info->externalNumber);
            cpr_free(line_info);
	}
    }
}


