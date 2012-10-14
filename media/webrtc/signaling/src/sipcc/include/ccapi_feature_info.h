/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCAPIAPI_FEATURE_INFO_H_
#define _CCAPIAPI_FEATURE_INFO_H_

#include "ccapi_types.h"


/**
 * Get the physical button number on which this feature is configured
 * @param feature - feature reference handle
 * @return cc_int32_t - button assgn to the feature
 */
cc_int32_t CCAPI_featureInfo_getButton(cc_featureinfo_ref_t feature);

/**
 * Get the featureID
 * @param feature - feature reference handle
 * @return cc_int32_t - button assgn to the feature
 */
cc_int32_t CCAPI_featureInfo_getFeatureID(cc_featureinfo_ref_t feature);
/**
 * Get the feature Name
 * @param feature - feature reference handle
 * @return cc_string_t - handle of the feature created
 */
cc_string_t CCAPI_featureInfo_getDisplayName(cc_featureinfo_ref_t feature);

/**
 * Get the speeddial Number
 * @param feature - feature reference handle
 * @return cc_string_t - handle of the feature created
 */
cc_string_t CCAPI_featureInfo_getSpeedDialNumber(cc_featureinfo_ref_t feature);

/**
 * Get the contact
 * @param feature - feature reference handle
 * @return cc_string_t - handle of the feature created
 */
cc_string_t CCAPI_featureInfo_getContact(cc_featureinfo_ref_t feature);

/**
 * Get the retrieval prefix
 * @param feature - feature reference handle
 * @return cc_string_t - handle of the feature created
 */
cc_string_t CCAPI_featureInfo_getRetrievalPrefix(cc_featureinfo_ref_t feature);

/**
 * Get BLF state
 * @param feature - feature reference handle
 * @return cc_string_t - handle of the feature created
 */
cc_blf_state_t CCAPI_featureInfo_getBLFState(cc_featureinfo_ref_t feature);

/**
 * Get the feature option mask
 * @param feature - feature reference handle
 * @return cc_int32_t - button assgn to the feature
 */
cc_int32_t CCAPI_featureInfo_getFeatureOptionMask(cc_featureinfo_ref_t feature);


#endif /* _CCAPIAPI_FEATURE_INFO_H_ */
