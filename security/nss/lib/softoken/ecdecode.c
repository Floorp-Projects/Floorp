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

#if EC_DEBUG
    printf("Curve: %s\n", SECOID_FindOIDTagDescription(tag));
#endif

    switch (tag) {
    case SEC_OID_ANSIX962_EC_C2PNB163V1:
	/* Populate params for c2pnb163v1 */
	params->fieldID.size = 163;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "08000000000000000000000000000000" \
	    "0000000107"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "072546B5435234A422E0789675F432C8" \
	    "9435DE5242"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "00C9517D06D5240D3CFF38C74B20B6CD" \
	    "4D6F9DD4D9"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "07AF69989546103D79329FCC3D74880F" \
	    "33BBE803CB" \
	    "01EC23211B5966ADEA1D3F87F7EA5848" \
	    "AEF0B7CA9F"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "0400000000000000000001E60FC8821C" \
	    "C74DAEAFC1"));
	params->cofactor = 2;
	break;

    case SEC_OID_ANSIX962_EC_C2PNB163V2:
	/* Populate params for c2pnb163v2 */
	params->fieldID.size = 163;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "08000000000000000000000000000000" \
	    "0000000107"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "0108B39E77C4B108BED981ED0E890E11" \
	    "7C511CF072"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "0667ACEB38AF4E488C407433FFAE4F1C" \
	    "811638DF20"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "0024266E4EB5106D0A964D92C4860E26" \
	    "71DB9B6CC5" \
	    "079F684DDF6684C5CD258B3890021B23" \
	    "86DFD19FC5"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "03FFFFFFFFFFFFFFFFFFFDF64DE1151A" \
	    "DBB78F10A7"));
	params->cofactor = 2;
	break;

    case SEC_OID_ANSIX962_EC_C2PNB163V3:
	/* Populate params for c2pnb163v3 */
	params->fieldID.size = 163;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "08000000000000000000000000000000" \
	    "0000000107"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "07A526C63D3E25A256A007699F5447E3" \
	    "2AE456B50E"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "03F7061798EB99E238FD6F1BF95B48FE" \
	    "EB4854252B"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "02F9F87B7C574D0BDECF8A22E6524775" \
	    "F98CDEBDCB" \
	    "05B935590C155E17EA48EB3FF3718B89" \
	    "3DF59A05D0"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "03FFFFFFFFFFFFFFFFFFFE1AEE140F11" \
	    "0AFF961309"));
	params->cofactor = 2;
	break;

    case SEC_OID_ANSIX962_EC_C2PNB176V1:
	/* Populate params for c2pnb176v1 */
	params->fieldID.size = 176;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "01000000000000000000000000000000" \
	    "00080000000007"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "E4E6DB2995065C407D9D39B8D0967B96" \
	    "704BA8E9C90B"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "5DDA470ABE6414DE8EC133AE28E9BBD7" \
	    "FCEC0AE0FFF2"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "8D16C2866798B600F9F08BB4A8E860F3" \
	    "298CE04A5798" \
	    "6FA4539C2DADDDD6BAB5167D61B436E1" \
	    "D92BB16A562C"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "00010092537397ECA4F6145799D62B0A" \
	    "19CE06FE26AD"));
	params->cofactor = 0xFF6E;
	break;

    case SEC_OID_ANSIX962_EC_C2TNB191V1:
	/* Populate params for c2tnb191v1 */
	params->fieldID.size = 191;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "80000000000000000000000000000000" \
	    "0000000000000201"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "2866537B676752636A68F56554E12640" \
	    "276B649EF7526267"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "2E45EF571F00786F67B0081B9495A3D9" \
	    "5462F5DE0AA185EC"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "36B3DAF8A23206F9C4F299D7B21A9C36" \
	    "9137F2C84AE1AA0D" \
	    "765BE73433B3F95E332932E70EA245CA" \
	    "2418EA0EF98018FB"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "40000000000000000000000004A20E90" \
	    "C39067C893BBB9A5"));
	params->cofactor = 2;
	break;

    case SEC_OID_ANSIX962_EC_C2TNB191V2:
	/* Populate params for c2tnb191v2 */
	params->fieldID.size = 191;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "80000000000000000000000000000000" \
	    "0000000000000201"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "401028774D7777C7B7666D1366EA4320" \
	    "71274F89FF01E718"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "0620048D28BCBD03B6249C99182B7C8C" \
	    "D19700C362C46A01"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "3809B2B7CC1B28CC5A87926AAD83FD28" \
	    "789E81E2C9E3BF10" \
	    "17434386626D14F3DBF01760D9213A3E" \
	    "1CF37AEC437D668A"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "20000000000000000000000050508CB8" \
	    "9F652824E06B8173"));
	params->cofactor = 4;
	break;

    case SEC_OID_ANSIX962_EC_C2TNB191V3:
	/* Populate params for c2tnb191v3 */
	params->fieldID.size = 191;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "80000000000000000000000000000000" \
	    "0000000000000201"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "6C01074756099122221056911C77D77E" \
	    "77A777E7E7E77FCB"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "71FE1AF926CF847989EFEF8DB459F663" \
	    "94D90F32AD3F15E8"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "375D4CE24FDE434489DE8746E7178601" \
	    "5009E66E38A926DD" \
	    "545A39176196575D985999366E6AD34C" \
	    "E0A77CD7127B06BE"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "155555555555555555555555610C0B19" \
	    "6812BFB6288A3EA3"));
	params->cofactor = 6;
	break;

    case SEC_OID_ANSIX962_EC_C2PNB208W1:
	/* Populate params for c2pnb208w1 */
	params->fieldID.size = 208;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "01000000000000000000000000000000" \
	    "0800000000000000000007"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "00"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "C8619ED45A62E6212E1160349E2BFA84" \
	    "4439FAFC2A3FD1638F9E"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "89FDFBE4ABE193DF9559ECF07AC0CE78" \
	    "554E2784EB8C1ED1A57A" \
	    "0F55B51A06E78E9AC38A035FF520D8B0" \
	    "1781BEB1A6BB08617DE3"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "000101BAF95C9723C57B6C21DA2EFF2D" \
	    "5ED588BDD5717E212F9D"));
	params->cofactor = 0xFE48;
	break;

    case SEC_OID_ANSIX962_EC_C2TNB239V1:
	/* Populate params for c2tnb239v1 */
	params->fieldID.size = 239;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "80000000000000000000000000000000" \
	    "0000000000000000001000000001"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "32010857077C5431123A46B808906756" \
	    "F543423E8D27877578125778AC76"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "790408F2EEDAF392B012EDEFB3392F30" \
	    "F4327C0CA3F31FC383C422AA8C16"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "57927098FA932E7C0A96D3FD5B706EF7" \
	    "E5F5C156E16B7E7C86038552E91D" \
	    "61D8EE5077C33FECF6F1A16B268DE469" \
	    "C3C7744EA9A971649FC7A9616305"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "2000000000000000000000000000000F" \
	    "4D42FFE1492A4993F1CAD666E447"));
	params->cofactor = 4;
	break;

    case SEC_OID_ANSIX962_EC_C2TNB239V2:
	/* Populate params for c2tnb239v2 */
	params->fieldID.size = 239;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "80000000000000000000000000000000" \
	    "0000000000000000001000000001"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "4230017757A767FAE42398569B746325" \
	    "D45313AF0766266479B75654E65F"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "5037EA654196CFF0CD82B2C14A2FCF2E" \
	    "3FF8775285B545722F03EACDB74B"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "28F9D04E900069C8DC47A08534FE76D2" \
	    "B900B7D7EF31F5709F200C4CA205" \
	    "5667334C45AFF3B5A03BAD9DD75E2C71" \
	    "A99362567D5453F7FA6E227EC833"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "1555555555555555555555555555553C" \
	    "6F2885259C31E3FCDF154624522D"));
	params->cofactor = 6;
	break;

    case SEC_OID_ANSIX962_EC_C2TNB239V3:
	/* Populate params for c2tnb239v3 */
	params->fieldID.size = 239;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "80000000000000000000000000000000" \
	    "0000000000000000001000000001"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "01238774666A67766D6676F778E676B6" \
	    "6999176666E687666D8766C66A9F"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "6A941977BA9F6A435199ACFC51067ED5" \
	    "87F519C5ECB541B8E44111DE1D40"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "70F6E9D04D289C4E89913CE3530BFDE9" \
	    "03977D42B146D539BF1BDE4E9C92" \
	    "2E5A0EAF6E5E1305B9004DCE5C0ED7FE" \
	    "59A35608F33837C816D80B79F461"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "0CCCCCCCCCCCCCCCCCCCCCCCCCCCCCAC" \
	    "4912D2D9DF903EF9888B8A0E4CFF"));
	params->cofactor = 0x0A;
	break;

    case SEC_OID_ANSIX962_EC_C2PNB272W1:
	/* Populate params for c2pnb272w1 */
	params->fieldID.size = 272;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "01000000000000000000000000000000" \
	    "00000000000000000000000100000000" \
	    "00000B"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "91A091F03B5FBA4AB2CCF49C4EDD220F" \
	    "B028712D42BE752B2C40094DBACDB586" \
	    "FB20"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "7167EFC92BB2E3CE7C8AAAFF34E12A9C" \
	    "557003D7C73A6FAF003F99F6CC8482E5" \
	    "40F7"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "6108BABB2CEEBCF787058A056CBE0CFE" \
	    "622D7723A289E08A07AE13EF0D10D171" \
	    "DD8D" \
	    "10C7695716851EEF6BA7F6872E6142FB" \
	    "D241B830FF5EFCACECCAB05E02005DDE" \
	    "9D23"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "000100FAF51354E0E39E4892DF6E319C" \
	    "72C8161603FA45AA7B998A167B8F1E62" \
	    "9521"));
	params->cofactor = 0xFF06;
	break;

    case SEC_OID_ANSIX962_EC_C2PNB304W1:
	/* Populate params for c2pnb304w1 */
	params->fieldID.size = 304;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "01000000000000000000000000000000" \
	    "00000000000000000000000000000000" \
	    "00000000000807"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "FD0D693149A118F651E6DCE680208537" \
	    "7E5F882D1B510B44160074C128807836" \
	    "5A0396C8E681"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "BDDB97E555A50A908E43B01C798EA5DA" \
	    "A6788F1EA2794EFCF57166B8C1403960" \
	    "1E55827340BE"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "197B07845E9BE2D96ADB0F5F3C7F2CFF" \
	    "BD7A3EB8B6FEC35C7FD67F26DDF6285A" \
	    "644F740A2614" \
	    "E19FBEB76E0DA171517ECF401B50289B" \
	    "F014103288527A9B416A105E80260B54" \
	    "9FDC1B92C03B"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "000101D556572AABAC800101D556572A" \
	    "ABAC8001022D5C91DD173F8FB561DA68" \
	    "99164443051D"));
	params->cofactor = 0xFE2E;
	break;

    case SEC_OID_ANSIX962_EC_C2TNB359V1:
	/* Populate params for c2tnb359v1 */
	params->fieldID.size = 359;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "80000000000000000000000000000000" \
	    "00000000000000000000000000000000" \
	    "00000000100000000000000001"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "5667676A654B20754F356EA92017D946" \
	    "567C46675556F19556A04616B567D223" \
	    "A5E05656FB549016A96656A557"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "2472E2D0197C49363F1FE7F5B6DB075D" \
	    "52B6947D135D8CA445805D39BC345626" \
	    "089687742B6329E70680231988"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "3C258EF3047767E7EDE0F1FDAA79DAEE" \
	    "3841366A132E163ACED4ED2401DF9C6B" \
	    "DCDE98E8E707C07A2239B1B097" \
	    "53D7E08529547048121E9C95F3791DD8" \
	    "04963948F34FAE7BF44EA82365DC7868" \
	    "FE57E4AE2DE211305A407104BD"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "01AF286BCA1AF286BCA1AF286BCA1AF2" \
	    "86BCA1AF286BC9FB8F6B85C556892C20" \
	    "A7EB964FE7719E74F490758D3B"));
	params->cofactor = 0x4C;
	break;

    case SEC_OID_ANSIX962_EC_C2PNB368W1:
	/* Populate params for c2pnb368w1 */
	params->fieldID.size = 368;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "01000000000000000000000000000000" \
	    "00000000000000000000000000000000" \
	    "000000002000000000000000000007"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "E0D2EE25095206F5E2A4F9ED229F1F25" \
	    "6E79A0E2B455970D8D0D865BD94778C5" \
	    "76D62F0AB7519CCD2A1A906AE30D"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "FC1217D4320A90452C760A58EDCD30C8" \
	    "DD069B3C34453837A34ED50CB54917E1" \
	    "C2112D84D164F444F8F74786046A"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "1085E2755381DCCCE3C1557AFA10C2F0" \
	    "C0C2825646C5B34A394CBCFA8BC16B22" \
	    "E7E789E927BE216F02E1FB136A5F" \
	    "7B3EB1BDDCBA62D5D8B2059B525797FC" \
	    "73822C59059C623A45FF3843CEE8F87C" \
	    "D1855ADAA81E2A0750B80FDA2310"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "00010090512DA9AF72B08349D98A5DD4" \
	    "C7B0532ECA51CE03E2D10F3B7AC579BD" \
	    "87E909AE40A6F131E9CFCE5BD967"));
	params->cofactor = 0xFF70;
	break;

    case SEC_OID_ANSIX962_EC_C2TNB431R1:
	/* Populate params for c2tnb431r1 */
	params->fieldID.size = 431;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "80000000000000000000000000000000" \
	    "00000000000000000000000000000000" \
	    "00000000000001000000000000000000" \
	    "000000000001"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "1A827EF00DD6FC0E234CAF046C6A5D8A" \
	    "85395B236CC4AD2CF32A0CADBDC9DDF6" \
	    "20B0EB9906D0957F6C6FEACD615468DF" \
	    "104DE296CD8F"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "10D9B4A3D9047D8B154359ABFB1B7F54" \
	    "85B04CEB868237DDC9DEDA982A679A5A" \
	    "919B626D4E50A8DD731B107A9962381F" \
	    "B5D807BF2618"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "120FC05D3C67A99DE161D2F4092622FE" \
	    "CA701BE4F50F4758714E8A87BBF2A658" \
	    "EF8C21E7C5EFE965361F6C2999C0C247" \
	    "B0DBD70CE6B7" \
	    "20D0AF8903A96F8D5FA2C255745D3C45" \
	    "1B302C9346D9B7E485E7BCE41F6B591F" \
	    "3E8F6ADDCBB0BC4C2F947A7DE1A89B62" \
	    "5D6A598B3760"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "03403403403403403403403403403403" \
	    "40340340340340340340340323C313FA" \
	    "B50589703B5EC68D3587FEC60D161CC1" \
	    "49C1AD4A91"));
	params->cofactor = 0x2760;
	break;
	
    case SEC_OID_SECG_EC_SECT113R1:
	/* Populate params for sect113r1 */
	params->fieldID.size = 113;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "020000000000000000000000000201"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "003088250CA6E7C7FE649CE85820F7"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "00E8BEE4D3E2260744188BE0E9C723"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "009D73616F35F4AB1407D73562C10F" \
	    "00A52830277958EE84D1315ED31886"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "0100000000000000D9CCEC8A39E56F"));
	params->cofactor = 2;
	break;

    case SEC_OID_SECG_EC_SECT113R2:
	/* Populate params for sect113r2 */
	params->fieldID.size = 113;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "020000000000000000000000000201"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "00689918DBEC7E5A0DD6DFC0AA55C7"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "0095E9A9EC9B297BD4BF36E059184F"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "01A57A6A7B26CA5EF52FCDB8164797" \
	    "00B3ADC94ED1FE674C06E695BABA1D"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "010000000000000108789B2496AF93"));
	params->cofactor = 2;
	break;

    case SEC_OID_SECG_EC_SECT131R1:
	/* Populate params for sect131r1 */
	params->fieldID.size = 131;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "08000000000000000000000000000001" \
	    "0D"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "07A11B09A76B562144418FF3FF8C2570" \
	    "B8"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "0217C05610884B63B9C6C7291678F9D3" \
	    "41"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "0081BAF91FDF9833C40F9C1813436383" \
	    "99" \
	    "078C6E7EA38C001F73C8134B1B4EF9E1" \
	    "50"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "0400000000000000023123953A9464B5" \
	    "4D"));
	params->cofactor = 2;
	break;

    case SEC_OID_SECG_EC_SECT131R2:
	/* Populate params for sect131r2 */
	params->fieldID.size = 131;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "08000000000000000000000000000001" \
	    "0D"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "03E5A88919D7CAFCBF415F07C2176573" \
	    "B2"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "04B8266A46C55657AC734CE38F018F21" \
	    "92"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "0356DCD8F2F95031AD652D23951BB366" \
	    "A8" \
	    "0648F06D867940A5366D9E265DE9EB24" \
	    "0F"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "0400000000000000016954A233049BA9" \
	    "8F"));
	params->cofactor = 2;
	break;

    case SEC_OID_SECG_EC_SECT163K1:
	/* Populate params for sect163k1
	 * (the NIST K-163 curve)
	 */
	params->fieldID.size = 163;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "08000000000000000000000000000000" \
	    "00000000C9"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "01"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "01"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "02FE13C0537BBC11ACAA07D793DE4E6D" \
	    "5E5C94EEE8" \
	    "0289070FB05D38FF58321F2E800536D5" \
	    "38CCDAA3D9"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "04000000000000000000020108A2E0CC" \
	    "0D99F8A5EF"));
	params->cofactor = 2;
	break;

    case SEC_OID_SECG_EC_SECT163R1:
	/* Populate params for sect163r1 */
	params->fieldID.size = 163;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "08000000000000000000000000000000" \
	    "00000000C9"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "07B6882CAAEFA84F9554FF8428BD88E2" \
	    "46D2782AE2"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "0713612DCDDCB40AAB946BDA29CA91F7" \
	    "3AF958AFD9"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "0369979697AB43897789566789567F78" \
	    "7A7876A654" \
	    "00435EDB42EFAFB2989D51FEFCE3C809" \
	    "88F41FF883"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "03FFFFFFFFFFFFFFFFFFFF48AAB689C2" \
	    "9CA710279B"));
	params->cofactor = 2;
	break;

    case SEC_OID_SECG_EC_SECT163R2:
	/* Populate params for sect163r2
	 * (the NIST B-163 curve)
	 */
	params->fieldID.size = 163;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "08000000000000000000000000000000" \
	    "00000000C9"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "01"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "020A601907B8C953CA1481EB10512F78" \
	    "744A3205FD"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "03F0EBA16286A2D57EA0991168D49946" \
	    "37E8343E36" \
	    "00D51FBC6C71A0094FA2CDD545B11C5C" \
	    "0C797324F1"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "040000000000000000000292FE77E70C" \
	    "12A4234C33"));
	params->cofactor = 2;
	break;

    case SEC_OID_SECG_EC_SECT193R1:
	/* Populate params for sect193r1 */
	params->fieldID.size = 193;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "02000000000000000000000000000000" \
	    "000000000000008001"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "0017858FEB7A98975169E171F77B4087" \
	    "DE098AC8A911DF7B01"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "00FDFB49BFE6C3A89FACADAA7A1E5BBC" \
	    "7CC1C2E5D831478814"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "01F481BC5F0FF84A74AD6CDF6FDEF4BF" \
	    "6179625372D8C0C5E1" \
	    "0025E399F2903712CCF3EA9E3A1AD17F" \
	    "B0B3201B6AF7CE1B05"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "01000000000000000000000000C7F34A" \
	    "778F443ACC920EBA49"));
	params->cofactor = 2;
	break;

    case SEC_OID_SECG_EC_SECT193R2:
	/* Populate params for sect193r2 */
	params->fieldID.size = 193;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "02000000000000000000000000000000" \
	    "000000000000008001"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "0163F35A5137C2CE3EA6ED8667190B0B" \
	    "C43ECD69977702709B"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "00C9BB9E8927D4D64C377E2AB2856A5B" \
	    "16E3EFB7F61D4316AE"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "00D9B67D192E0367C803F39E1A7E82CA1" \
	    "4A651350AAE617E8F" \
	    "01CE94335607C304AC29E7DEFBD9CA01" \
	    "F596F927224CDECF6C"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "010000000000000000000000015AAB56" \
	    "1B005413CCD4EE99D5"));
	params->cofactor = 2;
	break;

    case SEC_OID_SECG_EC_SECT233K1:
	/* Populate params for sect233k1
	 * (the NIST K-233 curve)
	 */
	params->fieldID.size = 233;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "02000000000000000000000000000000" \
	    "0000000004000000000000000001"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "00"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "01"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "017232BA853A7E731AF129F22FF41495" \
	    "63A419C26BF50A4C9D6EEFAD6126" \
	    "01DB537DECE819B7F70F555A67C427A8" \
	    "CD9BF18AEB9B56E0C11056FAE6A3"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "00800000000000000000000000000006" \
	    "9D5BB915BCD46EFB1AD5F173ABDF"));
	params->cofactor = 4;
	break;

    case SEC_OID_SECG_EC_SECT233R1:
	/* Populate params for sect233r1
	 * (the NIST B-233 curve)
	 */
	params->fieldID.size = 233;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "02000000000000000000000000000000" \
	    "0000000004000000000000000001"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "00000000000000000000000000000000" \
	    "0000000000000000000000000001"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "0066647EDE6C332C7F8C0923BB58213B" \
	    "333B20E9CE4281FE115F7D8F90AD"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "00FAC9DFCBAC8313BB2139F1BB755FEF" \
	    "65BC391F8B36F8F8EB7371FD558B" \
	    "01006A08A41903350678E58528BEBF8A" \
	    "0BEFF867A7CA36716F7E01F81052"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "01000000000000000000000000000013" \
	    "E974E72F8A6922031D2603CFE0D7"));
	params->cofactor = 2;
	break;

    case SEC_OID_SECG_EC_SECT239K1:
	/* Populate params for sect239k1 */
	params->fieldID.size = 239;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "80000000000000000000400000000000" \
	    "0000000000000000000000000001"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "00"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "01"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "29A0B6A887A983E9730988A68727A8B2" \
	    "D126C44CC2CC7B2A6555193035DC" \
	    "76310804F12E549BDB011C103089E735" \
	    "10ACB275FC312A5DC6B76553F0CA"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "2000000000000000000000000000005A" \
	    "79FEC67CB6E91F1C1DA800E478A5"));
	params->cofactor = 4;
	break;

    case SEC_OID_SECG_EC_SECT283K1:
        /* Populate params for sect283k1
	 * (the NIST K-283 curve)
	 */
	params->fieldID.size = 283;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "08000000000000000000000000000000" \
	    "00000000000000000000000000000000" \
            "000010A1"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "00"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "01"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "0503213F78CA44883F1A3B8162F188E5" \
	    "53CD265F23C1567A16876913B0C2AC24" \
	    "58492836" \
	    "01CCDA380F1C9E318D90F95D07E5426F" \
	    "E87E45C0E8184698E45962364E341161" \
	    "77DD2259"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "01FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFE9AE2ED07577265DFF7F94451E06" \
            "1E163C61"));
	params->cofactor = 4;
	break;

    case SEC_OID_SECG_EC_SECT283R1:
	/* Populate params for sect283r1
	 * (the NIST B-283 curve)
	 */
	params->fieldID.size = 283;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "08000000000000000000000000000000" \
	    "00000000000000000000000000000000" \
	    "000010A1"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "01"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "027B680AC8B8596DA5A4AF8A19A0303F" \
	    "CA97FD7645309FA2A581485AF6263E31" \
	    "3B79A2F5"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "05F939258DB7DD90E1934F8C70B0DFEC" \
	    "2EED25B8557EAC9C80E2E198F8CDBECD" \
	    "86B12053" \
	    "03676854FE24141CB98FE6D4B20D02B4" \
	    "516FF702350EDDB0826779C813F0DF45" \
	    "BE8112F4"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "03FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFEF90399660FC938A90165B042A7C" \
	    "EFADB307"));
	params->cofactor = 2;
	break;

    case SEC_OID_SECG_EC_SECT409K1:
	/* Populate params for sect409k1
	 * (the NIST K-409 curve)
	 */
	params->fieldID.size = 409;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "02000000000000000000000000000000" \
	    "00000000000000000000000000000000" \
	    "00000000000000000080000000000000" \
	    "00000001"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "00"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "01"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "0060F05F658F49C1AD3AB1890F718421" \
	    "0EFD0987E307C84C27ACCFB8F9F67CC2" \
	    "C460189EB5AAAA62EE222EB1B35540CF" \
	    "E9023746" \
	    "01E369050B7C4E42ACBA1DACBF04299C" \
	    "3460782F918EA427E6325165E9EA10E3" \
	    "DA5F6C42E9C55215AA9CA27A5863EC48" \
	    "D8E0286B"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "007FFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFFFFFFFFFFFFFFFE5F83B2D4EA" \
	    "20400EC4557D5ED3E3E7CA5B4B5C83B8" \
	    "E01E5FCF"));
	params->cofactor = 4;
	break;

    case SEC_OID_SECG_EC_SECT409R1:
	/* Populate params for sect409r1
	 * (the NIST B-409 curve)
	 */
	params->fieldID.size = 409;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "02000000000000000000000000000000" \
	    "00000000000000000000000000000000" \
	    "00000000000000000080000000000000" \
	    "00000001"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "01"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "0021A5C2C8EE9FEB5C4B9A753B7B476B" \
	    "7FD6422EF1F3DD674761FA99D6AC27C8" \
	    "A9A197B272822F6CD57A55AA4F50AE31" \
	    "7B13545F"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "015D4860D088DDB3496B0C6064756260" \
	    "441CDE4AF1771D4DB01FFE5B34E59703" \
	    "DC255A868A1180515603AEAB60794E54" \
	    "BB7996A7" \
	    "0061B1CFAB6BE5F32BBFA78324ED106A" \
	    "7636B9C5A7BD198D0158AA4F5488D08F" \
	    "38514F1FDF4B4F40D2181B3681C364BA" \
	    "0273C706"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "01000000000000000000000000000000" \
	    "0000000000000000000001E2AAD6A612" \
	    "F33307BE5FA47C3C9E052F838164CD37" \
	    "D9A21173"));
	params->cofactor = 2;
	break;

    case SEC_OID_SECG_EC_SECT571K1:
	/* Populate params for sect571k1
	 * (the NIST K-571 curve)
	 */
	params->fieldID.size = 571;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "08000000000000000000000000000000" \
	    "00000000000000000000000000000000" \
	    "00000000000000000000000000000000" \
	    "00000000000000000000000000000000" \
	    "0000000000000425"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "00"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "01"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "026EB7A859923FBC82189631F8103FE4" \
	    "AC9CA2970012D5D46024804801841CA4" \
	    "4370958493B205E647DA304DB4CEB08C" \
	    "BBD1BA39494776FB988B47174DCA88C7" \
	    "E2945283A01C8972" \
	    "0349DC807F4FBF374F4AEADE3BCA9531" \
	    "4DD58CEC9F307A54FFC61EFC006D8A2C" \
	    "9D4979C0AC44AEA74FBEBBB9F772AEDC" \
	    "B620B01A7BA7AF1B320430C8591984F6" \
	    "01CD4C143EF1C7A3"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "02000000000000000000000000000000" \
	    "00000000000000000000000000000000" \
	    "00000000131850E1F19A63E4B391A8DB" \
	    "917F4138B630D84BE5D639381E91DEB4" \
	    "5CFE778F637C1001"));
	params->cofactor = 4;
	break;

    case SEC_OID_SECG_EC_SECT571R1:
	/* Populate params for sect571r1
	 * (the NIST B-571 curve)
	 */
	params->fieldID.size = 571;
	params->fieldID.type = ec_field_GF2m;
	CHECK_OK(hexString2SECItem(params->arena, &params->fieldID.u.poly,
	    "08000000000000000000000000000000" \
	    "00000000000000000000000000000000" \
	    "00000000000000000000000000000000" \
	    "00000000000000000000000000000000" \
	    "0000000000000425"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.a,
	    "01"));
	CHECK_OK(hexString2SECItem(params->arena, &params->curve.b,
	    "02F40E7E2221F295DE297117B7F3D62F" \
	    "5C6A97FFCB8CEFF1CD6BA8CE4A9A18AD" \
	    "84FFABBD8EFA59332BE7AD6756A66E29" \
	    "4AFD185A78FF12AA520E4DE739BACA0C" \
	    "7FFEFF7F2955727A"));
	CHECK_OK(hexString2SECItem(params->arena, &params->base,
	    "04" \
	    "0303001D34B856296C16C0D40D3CD775" \
	    "0A93D1D2955FA80AA5F40FC8DB7B2ABD" \
	    "BDE53950F4C0D293CDD711A35B67FB14" \
	    "99AE60038614F1394ABFA3B4C850D927" \
	    "E1E7769C8EEC2D19" \
	    "037BF27342DA639B6DCCFFFEB73D69D7" \
	    "8C6C27A6009CBBCA1980F8533921E8A6" \
	    "84423E43BAB08A576291AF8F461BB2A8" \
	    "B3531D2F0485C19B16E2F1516E23DD3C" \
	    "1A4827AF1B8AC15B"));
	CHECK_OK(hexString2SECItem(params->arena, &params->order,
	    "03FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
	    "FFFFFFFFE661CE18FF55987308059B18" \
	    "6823851EC7DD9CA1161DE93D5174D66E" \
	    "8382E9BB2FE84E47"));
	params->cofactor = 2;
	break;

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
