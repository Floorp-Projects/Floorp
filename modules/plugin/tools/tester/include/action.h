/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __ACTION_H__
#define __ACTION_H__

typedef enum
{
  action_invalid = 0,

  action_np_initialize,
  action_np_shutdown,
  action_np_get_mime_description,

  action_npn_version = 101,
  action_npn_get_url_notify,
  action_npn_get_url,
  action_npn_post_url_notify,
  action_npn_post_url,
  action_npn_request_read,
  action_npn_new_stream,
  action_npn_write,
  action_npn_destroy_stream,
  action_npn_status,
  action_npn_user_agent,
  action_npn_mem_alloc,
  action_npn_mem_free,
  action_npn_mem_flush,
  action_npn_reload_plugins,
  action_npn_get_java_env,
  action_npn_get_java_peer,
  action_npn_get_value,
  action_npn_set_value,
  action_npn_invalidate_rect,
  action_npn_invalidate_region,
  action_npn_force_redraw,

  action_npp_new = 201,
  action_npp_destroy,
  action_npp_set_window,
  action_npp_new_stream,
  action_npp_destroy_stream,
  action_npp_stream_as_file,
  action_npp_write_ready,
  action_npp_write,
  action_npp_print,
  action_npp_handle_event,
  action_npp_url_notify,
  action_npp_get_java_class,
  action_npp_get_value,
  action_npp_set_value
} NPAPI_Action;

#endif // __ACTION_H__
