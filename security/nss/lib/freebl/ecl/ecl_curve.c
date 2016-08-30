/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ecl.h"
#include "ecl-curve.h"
#include "ecl-priv.h"
#include <stdlib.h>
#include <string.h>

#define CHECK(func)       \
    if ((func) == NULL) { \
        res = 0;          \
        goto CLEANUP;     \
    }

/* Duplicates an ECCurveParams */
ECCurveParams *
ECCurveParams_dup(const ECCurveParams *params)
{
    int res = 1;
    ECCurveParams *ret = NULL;

    CHECK(ret = (ECCurveParams *)calloc(1, sizeof(ECCurveParams)));
    if (params->text != NULL) {
        CHECK(ret->text = strdup(params->text));
    }
    ret->field = params->field;
    ret->size = params->size;
    if (params->irr != NULL) {
        CHECK(ret->irr = strdup(params->irr));
    }
    if (params->curvea != NULL) {
        CHECK(ret->curvea = strdup(params->curvea));
    }
    if (params->curveb != NULL) {
        CHECK(ret->curveb = strdup(params->curveb));
    }
    if (params->genx != NULL) {
        CHECK(ret->genx = strdup(params->genx));
    }
    if (params->geny != NULL) {
        CHECK(ret->geny = strdup(params->geny));
    }
    if (params->order != NULL) {
        CHECK(ret->order = strdup(params->order));
    }
    ret->cofactor = params->cofactor;

CLEANUP:
    if (res != 1) {
        EC_FreeCurveParams(ret);
        return NULL;
    }
    return ret;
}

#undef CHECK

/* Construct ECCurveParams from an ECCurveName */
ECCurveParams *
EC_GetNamedCurveParams(const ECCurveName name)
{
    if ((name <= ECCurve_noName) || (ECCurve_pastLastCurve <= name) ||
        (ecCurve_map[name] == NULL)) {
        return NULL;
    } else {
        return ECCurveParams_dup(ecCurve_map[name]);
    }
}

/* Free the memory allocated (if any) to an ECCurveParams object. */
void
EC_FreeCurveParams(ECCurveParams *params)
{
    if (params == NULL)
        return;
    if (params->text != NULL)
        free(params->text);
    if (params->irr != NULL)
        free(params->irr);
    if (params->curvea != NULL)
        free(params->curvea);
    if (params->curveb != NULL)
        free(params->curveb);
    if (params->genx != NULL)
        free(params->genx);
    if (params->geny != NULL)
        free(params->geny);
    if (params->order != NULL)
        free(params->order);
    free(params);
}
