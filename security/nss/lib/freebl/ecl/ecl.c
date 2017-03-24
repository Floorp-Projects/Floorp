/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "../stubs.h"
#endif

#include "mpi.h"
#include "mplogic.h"
#include "ecl.h"
#include "ecl-priv.h"
#include "ecp.h"
#include "ecl-curve.h"
#include <stdlib.h>
#include <string.h>

/* Allocate memory for a new ECGroup object. */
ECGroup *
ECGroup_new()
{
    mp_err res = MP_OKAY;
    ECGroup *group;
    group = (ECGroup *)malloc(sizeof(ECGroup));
    if (group == NULL)
        return NULL;
    group->constructed = MP_YES;
    group->meth = NULL;
    group->text = NULL;
    MP_DIGITS(&group->curvea) = 0;
    MP_DIGITS(&group->curveb) = 0;
    MP_DIGITS(&group->genx) = 0;
    MP_DIGITS(&group->geny) = 0;
    MP_DIGITS(&group->order) = 0;
    group->base_point_mul = NULL;
    group->points_mul = NULL;
    group->validate_point = NULL;
    group->extra1 = NULL;
    group->extra2 = NULL;
    group->extra_free = NULL;
    MP_CHECKOK(mp_init(&group->curvea));
    MP_CHECKOK(mp_init(&group->curveb));
    MP_CHECKOK(mp_init(&group->genx));
    MP_CHECKOK(mp_init(&group->geny));
    MP_CHECKOK(mp_init(&group->order));

CLEANUP:
    if (res != MP_OKAY) {
        ECGroup_free(group);
        return NULL;
    }
    return group;
}

/* Construct a generic ECGroup for elliptic curves over prime fields. */
ECGroup *
ECGroup_consGFp(const mp_int *irr, const mp_int *curvea,
                const mp_int *curveb, const mp_int *genx,
                const mp_int *geny, const mp_int *order, int cofactor)
{
    mp_err res = MP_OKAY;
    ECGroup *group = NULL;

    group = ECGroup_new();
    if (group == NULL)
        return NULL;

    group->meth = GFMethod_consGFp(irr);
    if (group->meth == NULL) {
        res = MP_MEM;
        goto CLEANUP;
    }
    MP_CHECKOK(mp_copy(curvea, &group->curvea));
    MP_CHECKOK(mp_copy(curveb, &group->curveb));
    MP_CHECKOK(mp_copy(genx, &group->genx));
    MP_CHECKOK(mp_copy(geny, &group->geny));
    MP_CHECKOK(mp_copy(order, &group->order));
    group->cofactor = cofactor;
    group->point_add = &ec_GFp_pt_add_aff;
    group->point_sub = &ec_GFp_pt_sub_aff;
    group->point_dbl = &ec_GFp_pt_dbl_aff;
    group->point_mul = &ec_GFp_pt_mul_jm_wNAF;
    group->base_point_mul = NULL;
    group->points_mul = &ec_GFp_pts_mul_jac;
    group->validate_point = &ec_GFp_validate_point;

CLEANUP:
    if (res != MP_OKAY) {
        ECGroup_free(group);
        return NULL;
    }
    return group;
}

/* Construct a generic ECGroup for elliptic curves over prime fields with
 * field arithmetic implemented in Montgomery coordinates. */
ECGroup *
ECGroup_consGFp_mont(const mp_int *irr, const mp_int *curvea,
                     const mp_int *curveb, const mp_int *genx,
                     const mp_int *geny, const mp_int *order, int cofactor)
{
    mp_err res = MP_OKAY;
    ECGroup *group = NULL;

    group = ECGroup_new();
    if (group == NULL)
        return NULL;

    group->meth = GFMethod_consGFp_mont(irr);
    if (group->meth == NULL) {
        res = MP_MEM;
        goto CLEANUP;
    }
    MP_CHECKOK(group->meth->field_enc(curvea, &group->curvea, group->meth));
    MP_CHECKOK(group->meth->field_enc(curveb, &group->curveb, group->meth));
    MP_CHECKOK(group->meth->field_enc(genx, &group->genx, group->meth));
    MP_CHECKOK(group->meth->field_enc(geny, &group->geny, group->meth));
    MP_CHECKOK(mp_copy(order, &group->order));
    group->cofactor = cofactor;
    group->point_add = &ec_GFp_pt_add_aff;
    group->point_sub = &ec_GFp_pt_sub_aff;
    group->point_dbl = &ec_GFp_pt_dbl_aff;
    group->point_mul = &ec_GFp_pt_mul_jm_wNAF;
    group->base_point_mul = NULL;
    group->points_mul = &ec_GFp_pts_mul_jac;
    group->validate_point = &ec_GFp_validate_point;

CLEANUP:
    if (res != MP_OKAY) {
        ECGroup_free(group);
        return NULL;
    }
    return group;
}

/* Construct an ECGroup. */
ECGroup *
construct_ecgroup(const ECCurveName name, mp_int irr, mp_int curvea,
                  mp_int curveb, mp_int genx, mp_int geny, mp_int order,
                  int cofactor, ECField field, const char *text)
{
    int bits;
    ECGroup *group = NULL;
    mp_err res = MP_OKAY;

    /* determine number of bits */
    bits = mpl_significant_bits(&irr) - 1;
    if (bits < MP_OKAY) {
        res = bits;
        goto CLEANUP;
    }

    /* determine which optimizations (if any) to use */
    if (field == ECField_GFp) {
        switch (name) {
            case ECCurve_SECG_PRIME_256R1:
                group =
                    ECGroup_consGFp(&irr, &curvea, &curveb, &genx, &geny,
                                    &order, cofactor);
                if (group == NULL) {
                    res = MP_UNDEF;
                    goto CLEANUP;
                }
                MP_CHECKOK(ec_group_set_gfp256(group, name));
                MP_CHECKOK(ec_group_set_gfp256_32(group, name));
                break;
            case ECCurve_SECG_PRIME_521R1:
                group =
                    ECGroup_consGFp(&irr, &curvea, &curveb, &genx, &geny,
                                    &order, cofactor);
                if (group == NULL) {
                    res = MP_UNDEF;
                    goto CLEANUP;
                }
                MP_CHECKOK(ec_group_set_gfp521(group, name));
                break;
            default:
                /* use generic arithmetic */
                group =
                    ECGroup_consGFp_mont(&irr, &curvea, &curveb, &genx, &geny,
                                         &order, cofactor);
                if (group == NULL) {
                    res = MP_UNDEF;
                    goto CLEANUP;
                }
        }
    } else {
        res = MP_UNDEF;
        goto CLEANUP;
    }

    /* set name, if any */
    if ((group != NULL) && (text != NULL)) {
        group->text = strdup(text);
        if (group->text == NULL) {
            res = MP_MEM;
        }
    }

CLEANUP:
    if (group && res != MP_OKAY) {
        ECGroup_free(group);
        return NULL;
    }
    return group;
}

/* Construct ECGroup from parameters and name, if any. */
ECGroup *
ecgroup_fromName(const ECCurveName name,
                 const ECCurveBytes *params)
{
    mp_int irr, curvea, curveb, genx, geny, order;
    ECGroup *group = NULL;
    mp_err res = MP_OKAY;

    /* initialize values */
    MP_DIGITS(&irr) = 0;
    MP_DIGITS(&curvea) = 0;
    MP_DIGITS(&curveb) = 0;
    MP_DIGITS(&genx) = 0;
    MP_DIGITS(&geny) = 0;
    MP_DIGITS(&order) = 0;
    MP_CHECKOK(mp_init(&irr));
    MP_CHECKOK(mp_init(&curvea));
    MP_CHECKOK(mp_init(&curveb));
    MP_CHECKOK(mp_init(&genx));
    MP_CHECKOK(mp_init(&geny));
    MP_CHECKOK(mp_init(&order));
    MP_CHECKOK(mp_read_unsigned_octets(&irr, params->irr, params->scalarSize));
    MP_CHECKOK(mp_read_unsigned_octets(&curvea, params->curvea, params->scalarSize));
    MP_CHECKOK(mp_read_unsigned_octets(&curveb, params->curveb, params->scalarSize));
    MP_CHECKOK(mp_read_unsigned_octets(&genx, params->genx, params->scalarSize));
    MP_CHECKOK(mp_read_unsigned_octets(&geny, params->geny, params->scalarSize));
    MP_CHECKOK(mp_read_unsigned_octets(&order, params->order, params->scalarSize));

    group = construct_ecgroup(name, irr, curvea, curveb, genx, geny, order,
                              params->cofactor, params->field, params->text);

CLEANUP:
    mp_clear(&irr);
    mp_clear(&curvea);
    mp_clear(&curveb);
    mp_clear(&genx);
    mp_clear(&geny);
    mp_clear(&order);
    if (group && res != MP_OKAY) {
        ECGroup_free(group);
        return NULL;
    }
    return group;
}

/* Construct ECCurveBytes from an ECCurveName */
const ECCurveBytes *
ec_GetNamedCurveParams(const ECCurveName name)
{
    if ((name <= ECCurve_noName) || (ECCurve_pastLastCurve <= name) ||
        (ecCurve_map[name] == NULL)) {
        return NULL;
    } else {
        return ecCurve_map[name];
    }
}

/* Construct ECGroup from named parameters. */
ECGroup *
ECGroup_fromName(const ECCurveName name)
{
    const ECCurveBytes *params = NULL;

    /* This doesn't work with Curve25519 but it's not necessary to. */
    PORT_Assert(name != ECCurve25519);

    params = ec_GetNamedCurveParams(name);
    if (params == NULL) {
        return NULL;
    }

    /* construct actual group */
    return ecgroup_fromName(name, params);
}

/* Validates an EC public key as described in Section 5.2.2 of X9.62. */
mp_err
ECPoint_validate(const ECGroup *group, const mp_int *px, const mp_int *py)
{
    /* 1: Verify that publicValue is not the point at infinity */
    /* 2: Verify that the coordinates of publicValue are elements
     *    of the field.
     */
    /* 3: Verify that publicValue is on the curve. */
    /* 4: Verify that the order of the curve times the publicValue
     *    is the point at infinity.
     */
    return group->validate_point(px, py, group);
}

/* Free the memory allocated (if any) to an ECGroup object. */
void
ECGroup_free(ECGroup *group)
{
    if (group == NULL)
        return;
    GFMethod_free(group->meth);
    if (group->constructed == MP_NO)
        return;
    mp_clear(&group->curvea);
    mp_clear(&group->curveb);
    mp_clear(&group->genx);
    mp_clear(&group->geny);
    mp_clear(&group->order);
    if (group->text != NULL)
        free(group->text);
    if (group->extra_free != NULL)
        group->extra_free(group);
    free(group);
}
