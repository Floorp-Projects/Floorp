/*
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
 * The Original Code is the Elliptic Curve Cryptography library.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems, Inc. are Copyright (C) 2003
 * Sun Microsystems, Inc. All Rights Reserved.
 *
 * Contributor(s):
 *	Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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
 */

#ifdef NSS_ENABLE_ECC

#include "blapi.h"
#include "secoid.h"
#include "secitem.h"
#include "secerr.h"
#include "ec.h"

#define CHECK_OK(func) if (func == NULL) goto cleanup;

/* Initializes a SECItem from a hexadecimal string */
static SECItem *
hexString2SECItem(PRArenaPool *arena, SECItem *item, const char *str)
{
    int i = 0;
    int byteval = 0;
    int tmp = PORT_Strlen(str);

    if ((tmp % 2) != 0) return NULL;
    
    item->data = (unsigned char *) PORT_ArenaZAlloc(arena, tmp/2);
    if (item->data == NULL) return NULL;
    item->len = tmp/2;

    while (str[i]) {
        if ((str[i] >= '0') && (str[i] <= '9'))
	    tmp = str[i] - '0';
	else if ((str[i] >= 'a') && (str[i] <= 'f'))
	    tmp = str[i] - 'a' + 10;
	else if ((str[i] >= 'A') && (str[i] <= 'F'))
	    tmp = str[i] - 'A' + 10;
	else
	    return NULL;

	byteval = byteval * 16 + tmp;
	if ((i % 2) != 0) {
	    item->data[i/2] = byteval;
	    byteval = 0;
	}
	i++;
    }

    return item;
}

SECStatus
EC_FillParams(PRArenaPool *arena, const SECItem *encodedParams, 
    ECParams *params)
{
    SECOidTag tag;
    SECItem oid = { siBuffer, NULL, 0};

#if EC_DEBUG
    int i;

    printf("Encoded params in EC_DecodeParams: ");
    for (i = 0; i < encodedParams->len; i++) {
	    printf("%02x:", encodedParams->data[i]);
    }
    printf("\n");
#endif

    if ((encodedParams->len != ANSI_X962_CURVE_OID_TOTAL_LEN) &&
	(encodedParams->len != SECG_CURVE_OID_TOTAL_LEN)) {
	    PORT_SetError(SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
	    return SECFailure;
    };

    oid.len = encodedParams->len - 2;
    oid.data = encodedParams->data + 2;
    if ((encodedParams->data[0] != SEC_ASN1_OBJECT_ID) ||
	((tag = SECOID_FindOIDTag(&oid)) == SEC_OID_UNKNOWN)) { 
	    PORT_SetError(SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
	    return SECFailure;
    }

    params->arena = arena;
    params->cofactor = 0;
    params->type = ec_params_named;

    switch (tag) {
    case SEC_OID_ANSIX962_EC_PRIME192V1:
	/* Populate params for prime192v1 aka secp192r1 
	 * (the NIST P-192 curve)
	 */
	params->fieldID.size = 192;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE" \
	    "FFFFFFFFFFFFFFFF"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE" \
	    "FFFFFFFFFFFFFFFC"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "64210519E59C80E70FA7E9AB72243049" \
	    "FEB8DEECC146B9B1"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "188DA80EB03090F67CBF20EB43A18800" \
	    "F4FF0AFD82FF1012" \
	    "07192B95FFC8DA78631011ED6B24CDD5" \
	    "73F977A11E794811"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "FFFFFFFFFFFFFFFFFFFFFFFF99DEF836" \
	    "146BC9B1B4D22831"));
	params->cofactor = 1;
	break;

    case SEC_OID_ANSIX962_EC_PRIME192V2:
	/* Populate params for prime192v2 */
	params->fieldID.size = 192;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE" \
	    "FFFFFFFFFFFFFFFF"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE" \
	    "FFFFFFFFFFFFFFFC"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "CC22D6DFB95C6B25E49C0D6364A4E598" \
	    "0C393AA21668D953"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "EEA2BAE7E1497842F2DE7769CFE9C989" \
	    "C072AD696F48034A" \
	    "6574D11D69B6EC7A672BB82A083DF2F2" \
	    "B0847DE970B2DE15"));
	
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "FFFFFFFFFFFFFFFFFFFFFFFE5FB1A724" \
	    "DC80418648D8DD31"));
	params->cofactor = 1;
	break;

    case SEC_OID_ANSIX962_EC_PRIME192V3:
	/* Populate params for prime192v3 */
	params->fieldID.size = 192;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE" \
	    "FFFFFFFFFFFFFFFF"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE" \
	    "FFFFFFFFFFFFFFFC"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "22123DC2395A05CAA7423DAECCC94760" \
	    "A7D462256BD56916"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "7D29778100C65A1DA1783716588DCE2B" \
	    "8B4AEE8E228F1896" \
	    "38A90F22637337334B49DCB66A6DC8F9" \
	    "978ACA7648A943B0"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "FFFFFFFFFFFFFFFFFFFFFFFF7A62D031" \
	    "C83F4294F640EC13"));
	params->cofactor = 1;
	break;
	
    case SEC_OID_ANSIX962_EC_PRIME239V1:
	/* Populate params for prime239v1 */
	params->fieldID.size = 239;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF" \
	    "FFFF8000000000007FFFFFFFFFFF"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF" \
	    "FFFF8000000000007FFFFFFFFFFC"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "6B016C3BDCF18941D0D654921475CA71" \
	    "A9DB2FB27D1D37796185C2942C0A"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "0FFA963CDCA8816CCC33B8642BEDF905" \
	    "C3D358573D3F27FBBD3B3CB9AAAF" \
	    "7DEBE8E4E90A5DAE6E4054CA530BA046" \
	    "54B36818CE226B39FCCB7B02F1AE"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFF9E" \
	    "5E9A9F5D9071FBD1522688909D0B"));
	params->cofactor = 1;
	break;

    case SEC_OID_ANSIX962_EC_PRIME239V2:
	/* Populate params for prime239v2 */
	params->fieldID.size = 239;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF" \
	    "FFFF8000000000007FFFFFFFFFFF"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF" \
	    "FFFF8000000000007FFFFFFFFFFC"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "617FAB6832576CBBFED50D99F0249C3F" \
	    "EE58B94BA0038C7AE84C8C832F2C"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "38AF09D98727705120C921BB5E9E2629" \
	    "6A3CDCF2F35757A0EAFD87B830E7" \
	    "5B0125E4DBEA0EC7206DA0FC01D9B081" \
	    "329FB555DE6EF460237DFF8BE4BA"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "7FFFFFFFFFFFFFFFFFFFFFFF800000CF" \
	    "A7E8594377D414C03821BC582063"));
	params->cofactor = 1;
	break;

    case SEC_OID_ANSIX962_EC_PRIME239V3:
	/* Populate params for prime239v3 */
	params->fieldID.size = 239;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF" \
	    "FFFF8000000000007FFFFFFFFFFF"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF" \
	    "FFFF8000000000007FFFFFFFFFFC"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "255705FA2A306654B1F4CB03D6A750A3" \
	    "0C250102D4988717D9BA15AB6D3E"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "6768AE8E18BB92CFCF005C949AA2C6D9" \
	    "4853D0E660BBF854B1C9505FE95A" \
	    "1607E6898F390C06BC1D552BAD226F3B" \
	    "6FCFE48B6E818499AF18E3ED6CF3"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFF97" \
	    "5DEB41B3A6057C3C432146526551"));
	params->cofactor = 1;
	break;

    case SEC_OID_ANSIX962_EC_PRIME256V1:
	/* Populate params for prime256v1 aka secp256r1
	 * (the NIST P-256 curve)
	 */
	params->fieldID.size = 256;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "FFFFFFFF000000010000000000000000" \
	    "00000000FFFFFFFFFFFFFFFFFFFFFFFF"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "FFFFFFFF000000010000000000000000" \
	    "00000000FFFFFFFFFFFFFFFFFFFFFFFC"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "5AC635D8AA3A93E7B3EBBD55769886BC" \
	    "651D06B0CC53B0F63BCE3C3E27D2604B"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "6B17D1F2E12C4247F8BCE6E563A440F2" \
	    "77037D812DEB33A0F4A13945D898C296" \
	    "4FE342E2FE1A7F9B8EE7EB4A7C0F9E16" \
	    "2BCE33576B315ECECBB6406837BF51F5"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "FFFFFFFF00000000FFFFFFFFFFFFFFFF" \
	    "BCE6FAADA7179E84F3B9CAC2FC632551"));
	params->cofactor = 1;
	break;

    case SEC_OID_SECG_EC_SECP112R1:
        /* Populate params for secp112r1 */
	params->fieldID.size = 112;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "DB7C2ABF62E35E668076BEAD208B"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "DB7C2ABF62E35E668076BEAD2088"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "659EF8BA043916EEDE8911702B22"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "09487239995A5EE76B55F9C2F098" \
	    "A89CE5AF8724C0A23E0E0FF77500"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "DB7C2ABF62E35E7628DFAC6561C5"));
	params->cofactor = 1;
	break;

    case SEC_OID_SECG_EC_SECP112R2:
        /* Populate params for secp112r2 */
	params->fieldID.size = 112;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "DB7C2ABF62E35E668076BEAD208B"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "6127C24C05F38A0AAAF65C0EF02C"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "51DEF1815DB5ED74FCC34C85D709"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "4BA30AB5E892B4E1649DD0928643" \
	    "ADCD46F5882E3747DEF36E956E97"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "36DF0AAFD8B8D7597CA10520D04B"));
	params->cofactor = 4;
	break;

    case SEC_OID_SECG_EC_SECP128R1:
        /* Populate params for secp128r1 */
	params->fieldID.size = 128;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "FFFFFFFDFFFFFFFFFFFFFFFFFFFFFFFF"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "FFFFFFFDFFFFFFFFFFFFFFFFFFFFFFFC"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "E87579C11079F43DD824993C2CEE5ED3"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "161FF7528B899B2D0C28607CA52C5B86" \
	    "CF5AC8395BAFEB13C02DA292DDED7A83"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "FFFFFFFE0000000075A30D1B9038A115"));
	params->cofactor = 1;
	break;

    case SEC_OID_SECG_EC_SECP128R2:
        /* Populate params for secp128r2 */
	params->fieldID.size = 128;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "FFFFFFFDFFFFFFFFFFFFFFFFFFFFFFFF"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "D6031998D1B3BBFEBF59CC9BBFF9AEE1"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "5EEEFCA380D02919DC2C6558BB6D8A5D"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "7B6AA5D85E572983E6FB32A7CDEBC140" \
	    "27B6916A894D3AEE7106FE805FC34B44"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "3FFFFFFF7FFFFFFFBE0024720613B5A3"));
	params->cofactor = 4;
	break;
	
    case SEC_OID_SECG_EC_SECP160K1:
        /* Populate params for secp160k1 */
	params->fieldID.size = 160;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE" \
	    "FFFFAC73"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "00"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "07"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "3B4C382CE37AA192A4019E763036F4F5" \
	    "DD4D7EBB" \
	    "938CF935318FDCED6BC28286531733C3" \
	    "F03C4FEE"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "0100000000000000000001B8FA16DFAB" \
	    "9ACA16B6B3"));
	params->cofactor = 1;
	break;

    case SEC_OID_SECG_EC_SECP160R1:
        /* Populate params for secp160r1 */
	params->fieldID.size = 160;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "7FFFFFFF"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "7FFFFFFC"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "1C97BEFC54BD7A8B65ACF89F81D4D4AD" \
	    "C565FA45"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "4A96B5688EF573284664698968C38BB9" \
	    "13CBFC82" \
	    "23A628553168947D59DCC91204235137" \
	    "7AC5FB32"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "0100000000000000000001F4C8F927AE" \
	    "D3CA752257"));
	params->cofactor = 1;
	break;

    case SEC_OID_SECG_EC_SECP160R2:
	/* Populate params for secp160r1 */
	params->fieldID.size = 160;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE" \
	    "FFFFAC73"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE" \
	    "FFFFAC70"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "B4E134D3FB59EB8BAB57274904664D5A" \
	    "F50388BA"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "52DCB034293A117E1F4FF11B30F7199D" \
	    "3144CE6D" \
	    "FEAFFEF2E331F296E071FA0DF9982CFE" \
	    "A7D43F2E"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "0100000000000000000000351EE786A8" \
	    "18F3A1A16B"));
	params->cofactor = 1;
	break;

    case SEC_OID_SECG_EC_SECP192K1:
	/* Populate params for secp192k1 */
	params->fieldID.size = 192;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFEFFFFEE37"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "00"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "03"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "DB4FF10EC057E9AE26B07D0280B7F434" \
	    "1DA5D1B1EAE06C7D" \
	    "9B2F2F6D9C5628A7844163D015BE8634" \
	    "4082AA88D95E2F9D"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "FFFFFFFFFFFFFFFFFFFFFFFE26F2FC17" \
	    "0F69466A74DEFD8D"));
	params->cofactor = 1;
	break;

    case SEC_OID_SECG_EC_SECP224K1:
	/* Populate params for secp224k1 */
	params->fieldID.size = 224;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFFFFFFFFFEFFFFE56D"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "00"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "05"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "A1455B334DF099DF30FC28A169A467E9" \
	    "E47075A90F7E650EB6B7A45C" \
	    "7E089FED7FBA344282CAFBD6F7E319F7" \
	    "C0B0BD59E2CA4BDB556D61A5"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "010000000000000000000000000001DC" \
	    "E8D2EC6184CAF0A971769FB1F7"));
	params->cofactor = 1;
	break;

    case SEC_OID_SECG_EC_SECP224R1:
	/* Populate params for secp224r1 
	 * (the NIST P-224 curve)
	 */
	params->fieldID.size = 224;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "000000000000000000000001"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE" \
	    "FFFFFFFFFFFFFFFFFFFFFFFE"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "B4050A850C04B3ABF54132565044B0B7" \
	    "D7BFD8BA270B39432355FFB4"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "B70E0CBD6BB4BF7F321390B94A03C1D3" \
	    "56C21122343280D6115C1D21" \
	    "BD376388B5F723FB4C22DFE6CD4375A0" \
	    "5A07476444D5819985007E34"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFF16A2" \
	    "E0B8F03E13DD29455C5C2A3D"));
	params->cofactor = 1;
	break;

    case SEC_OID_SECG_EC_SECP256K1:
	/* Populate params for secp256k1 */
	params->fieldID.size = 256;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "00"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "07"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "79BE667EF9DCBBAC55A06295CE870B07" \
	    "029BFCDB2DCE28D959F2815B16F81798" \
	    "483ADA7726A3C4655DA4FBFC0E1108A8" \
	    "FD17B448A68554199C47D08FFB10D4B8"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE" \
	    "BAAEDCE6AF48A03BBFD25E8CD0364141"));
	params->cofactor = 1;
	break;

    case SEC_OID_SECG_EC_SECP384R1:
	/* Populate params for secp384r1
	 * (the NIST P-384 curve)
	 */
	params->fieldID.size = 384;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE" \
	    "FFFFFFFF0000000000000000FFFFFFFF"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE" \
	    "FFFFFFFF0000000000000000FFFFFFFC"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "B3312FA7E23EE7E4988E056BE3F82D19" \
	    "181D9C6EFE8141120314088F5013875A" \
	    "C656398D8A2ED19D2A85C8EDD3EC2AEF"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "AA87CA22BE8B05378EB1C71EF320AD74" \
	    "6E1D3B628BA79B9859F741E082542A38" \
	    "5502F25DBF55296C3A545E3872760AB7" \
	    "3617DE4A96262C6F5D9E98BF9292DC29" \
	    "F8F41DBD289A147CE9DA3113B5F0B8C0" \
	    "0A60B1CE1D7E819D7A431D7C90EA0E5F"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFFFFFFFFFFC7634D81F4372DDF" \
	    "581A0DB248B0A77AECEC196ACCC52973"));
	params->cofactor = 1;
	break;

    case SEC_OID_SECG_EC_SECP521R1:
	/* Populate params for secp521r1 
	 * (the NIST P-521 curve)
	 */
	params->fieldID.size = 521;
	params->fieldID.type = ec_field_GFp;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.prime,
	    "01FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFF"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "01FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFC"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "0051953EB9618E1C9A1F929A21A0B685" \
	    "40EEA2DA725B99B315F3B8B489918EF1" \
	    "09E156193951EC7E937B1652C0BD3BB1" \
	    "BF073573DF883D2C34F1EF451FD46B50" \
	    "3F00"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "00C6858E06B70404E9CD9E3ECB662395" \
	    "B4429C648139053FB521F828AF606B4D" \
	    "3DBAA14B5E77EFE75928FE1DC127A2FF" \
	    "A8DE3348B3C1856A429BF97E7E31C2E5" \
	    "BD66" \
	    "011839296A789A3BC0045C8A5FB42C7D" \
	    "1BD998F54449579B446817AFBD17273E" \
	    "662C97EE72995EF42640C550B9013FAD" \
	    "0761353C7086A272C24088BE94769FD1" \
	    "6650"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "01FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFA51868783BF2F966B7FCC0148F709" \
	    "A5D03BB5C9B8899C47AEBB6FB71E9138" \
	    "6409"));
	params->cofactor = 1;
	break;
	
    default:
	break;
    };

cleanup:
    if (!params->cofactor) {
	PORT_SetError(SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
#if EC_DEBUG
	printf("Unrecognized curve, returning NULL params\n");
#endif
	return SECFailure;
    }

    return SECSuccess;
}

SECStatus
EC_DecodeParams(const SECItem *encodedParams, ECParams **ecparams)
{
    PRArenaPool *arena;
    ECParams *params;
    SECStatus rv = SECFailure;

    /* Initialize an arena for the ECParams structure */
    if (!(arena = PORT_NewArena(NSS_FREEBL_DEFAULT_CHUNKSIZE)))
	return SECFailure;

    params = (ECParams *)PORT_ArenaZAlloc(arena, sizeof(ECParams));
    if (!params) {
	PORT_FreeArena(arena, PR_TRUE);
	return SECFailure;
    }

    /* Copy the encoded params */
    SECITEM_AllocItem(arena, &(params->DEREncoding),
	encodedParams->len);
    memcpy(params->DEREncoding.data, encodedParams->data, encodedParams->len);

    /* Fill out the rest of the ECParams structure based on 
     * the encoded params 
     */
    rv = EC_FillParams(arena, encodedParams, params);
    if (rv == SECFailure) {
	PORT_FreeArena(arena, PR_TRUE);	
	return SECFailure;
    } else {
	*ecparams = params;;
	return SECSuccess;
    }
}

#endif /* NSS_ENABLE_ECC */
