/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "string_lib.h"
#include "sessionConstants.h"
#include "sessionTypes.h"

/* CallControl Provider Management Interfaces */
void scSessionProviderCmd(sessionProvider_cmd_t *data);

/* CallControl Provider Management Updates */
void scSessionProviderState(unsigned int state, scProvider_state_t *data);

/* Session mgmt */
session_id_t scCreateSession(session_create_param_t *param);
void scCloseSession(session_id_t sess_id);
void scInvokeFeature(session_feature_t *featData);

/* Session Updates */
void scSessionUpdate(session_update_t *session);
void scFeatureUpdate(feature_update_t *data);


