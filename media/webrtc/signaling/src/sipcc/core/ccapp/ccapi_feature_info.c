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

#include "ccapi_snapshot.h"
#include "sessionHash.h"
#include "CCProvider.h"
#include "phone_debug.h"

/**
 * Get the physical button number on which this feature is configured
 * @param feature - feature reference handle
 * @return cc_int32_t - button assigned to the feature
 */
cc_int32_t CCAPI_featureInfo_getButton(cc_featureinfo_ref_t feature)
{
   static const char *fname="CCAPI_featureInfo_getButton";
   cc_feature_info_t  *info;
   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   info = (cc_feature_info_t *) feature;
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->button);
       return info->button;
   }
   return -1;
}

/**
 * Get the featureID
 * @param feature - feature reference handle
 * @return cc_int32_t - type of to the feature
 */
cc_int32_t CCAPI_featureInfo_getFeatureID(cc_featureinfo_ref_t feature)
{
   static const char *fname="CCAPI_featureInfo_getFeatureID";
   cc_feature_info_t  *info;
   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   info = (cc_feature_info_t *) feature;
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->feature_id);
       return info->feature_id;
   }
   return -1;
}

/**
 * Get the feature Label Name
 * @param feature - feature reference handle
 * @return cc_string_t - name of the feature created
 */
cc_string_t CCAPI_featureInfo_getDisplayName(cc_featureinfo_ref_t feature) {
   static const char *fname="CCAPI_featureInfo_getDisplayName";
   cc_feature_info_t  *info;
   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   info = (cc_feature_info_t *) feature;
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->name);
       return ccsnap_get_line_label(info->button);
   }
   return NULL;
}

/**
 * Get the speeddial Number
 * @param feature - feature reference handle
 * @return cc_string_t - speeddial number of the feature created
 */
cc_string_t CCAPI_featureInfo_getSpeedDialNumber(cc_featureinfo_ref_t feature) {
   static const char *fname="CCAPI_featureInfo_getSpeedDialNumber";
   cc_feature_info_t  *info;
   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   info = (cc_feature_info_t *) feature;
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->speedDialNumber);
       return info->speedDialNumber;
   }
   return NULL;
}

/**
 * Get the contact
 * @param feature - feature reference handle
 * @return cc_string_t - contact of the feature created
 */
cc_string_t CCAPI_featureInfo_getContact(cc_featureinfo_ref_t feature) {
   static const char *fname="CCAPI_featureInfo_getContact";
   cc_feature_info_t  *info;
   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   info = (cc_feature_info_t *) feature;
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->contact);
       return info->contact;
   }
   return NULL;
}

/**
 * Get the retrieval prefix
 * @param feature - feature reference handle
 * @return cc_string_t - retrieval prefix of the feature created
 */
cc_string_t CCAPI_featureInfo_getRetrievalPrefix(cc_featureinfo_ref_t feature) {
   static const char *fname="CCAPI_featureInfo_getRetrievalPrefix";
   cc_feature_info_t  *info;
   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   info = (cc_feature_info_t *) feature;
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->retrievalPrefix);
       return info->retrievalPrefix;
   }
   return NULL;
}

/**
 * Get BLF state
 * @param feature - feature reference handle
 * @return cc_string_t - handle of the feature created
 */
cc_blf_state_t CCAPI_featureInfo_getBLFState(cc_featureinfo_ref_t feature) {
   static const char *fname="CCAPI_featureInfo_getBLFState";
   cc_feature_info_t  *info = (cc_feature_info_t  *)feature;
   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->blf_state);
       return info->blf_state;
   }
   return CC_SIP_BLF_UNKNOWN;
}

/**
 * Get the feature option mask
 * @param feature - feature reference handle
 * @return cc_int32_t - feature option mask for the feature
 */
cc_int32_t CCAPI_featureInfo_getFeatureOptionMask(cc_featureinfo_ref_t feature)
{
   static const char *fname="CCAPI_featureInfo_getFeatureOptionMask";
   cc_feature_info_t  *info;
   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   info = (cc_feature_info_t *) feature;
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->featureOptionMask);
       return info->featureOptionMask;
   }
   return -1;
}
