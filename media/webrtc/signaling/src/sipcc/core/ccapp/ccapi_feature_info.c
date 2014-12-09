/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
   CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   info = (cc_feature_info_t *) feature;
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->button);
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
   CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   info = (cc_feature_info_t *) feature;
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->feature_id);
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
   CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   info = (cc_feature_info_t *) feature;
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->name);
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
   CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   info = (cc_feature_info_t *) feature;
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->speedDialNumber);
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
   CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   info = (cc_feature_info_t *) feature;
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->contact);
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
   CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   info = (cc_feature_info_t *) feature;
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->retrievalPrefix);
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
   CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->blf_state);
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
   CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   info = (cc_feature_info_t *) feature;
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->featureOptionMask);
       return info->featureOptionMask;
   }
   return -1;
}
