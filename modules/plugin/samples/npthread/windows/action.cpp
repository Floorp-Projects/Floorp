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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "action.h"

char * FormatAction(int aAction)
{
  static char string[64] = {'\0'};
  switch (aAction) {
    case action_invalid:
      strcpy(string, "invalid action");
      break;
    case action_npn_version:
      strcpy(string, "npn_version");
      break;
    case action_npn_get_url_notify:
      strcpy(string, "npn_get_url_notify");
      break;
    case action_npn_get_url:
      strcpy(string, "npn_get_url");
      break;
    case action_npn_post_url_notify:
      strcpy(string, "npn_post_url_notify");
      break;
    case action_npn_post_url:
      strcpy(string, "npn_post_url");
      break;
    case action_npn_request_read:
      strcpy(string, "npn_request_read");
      break;
    case action_npn_new_stream:
      strcpy(string, "npn_new_stream");
      break;
    case action_npn_write:
      strcpy(string, "npn_write");
      break;
    case action_npn_destroy_stream:
      strcpy(string, "npn_destroy_stream");
      break;
    case action_npn_status:
      strcpy(string, "npn_status");
      break;
    case action_npn_user_agent:
      strcpy(string, "npn_user_agent");
      break;
    case action_npn_mem_alloc:
      strcpy(string, "npn_mem_alloc");
      break;
    case action_npn_mem_free:
      strcpy(string, "npn_mem_free");
      break;
    case action_npn_mem_flush:
      strcpy(string, "npn_mem_flush");
      break;
    case action_npn_reload_plugins:
      strcpy(string, "npn_reload_plugins");
      break;
    case action_npn_get_java_env:
      strcpy(string, "npn_get_java_env");
      break;
    case action_npn_get_java_peer:
      strcpy(string, "npn_get_java_peer");
      break;
    case action_npn_get_value:
      strcpy(string, "npn_get_value");
      break;
    case action_npn_set_value:
      strcpy(string, "npn_set_value");
      break;
    case action_npn_invalidate_rect:
      strcpy(string, "npn_invalidate_rect");
      break;
    case action_npn_invalidate_region:
      strcpy(string, "npn_invalidate_region");
      break;
    case action_npn_force_redraw:
      strcpy(string, "npn_force_redraw");
      break;
 
    case action_npp_new:
      strcpy(string, "npp_new");
      break;
    case action_npp_destroy:
      strcpy(string, "npp_destroy");
      break;
    case action_npp_set_window:
      strcpy(string, "npp_set_window");
      break;
    case action_npp_new_stream:
      strcpy(string, "npp_new_stream");
      break;
    case action_npp_destroy_stream:
      strcpy(string, "npp_destroy_stream");
      break;
    case action_npp_stream_as_file:
      strcpy(string, "npp_stream_as_file");
      break;
    case action_npp_write_ready:
      strcpy(string, "npp_write_ready");
      break;
    case action_npp_write:
      strcpy(string, "npp_write");
      break;
    case action_npp_print:
      strcpy(string, "npp_print");
      break;
    case action_npp_handle_event:
      strcpy(string, "npp_handle_event");
      break;
    case action_npp_url_notify:
      strcpy(string, "npp_url_notify");
      break;
    case action_npp_get_java_class:
      strcpy(string, "npp_get_java_class");
      break;
    case action_npp_get_value:
      strcpy(string, "npp_get_value");
      break;
    case action_npp_set_value:
      strcpy(string, "npp_set_value");
      break;
    default:
      strcpy(string, "Unknown action!");
      break;
  }
  return string;
} 
