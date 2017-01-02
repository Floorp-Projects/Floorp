/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _P12LOCAL_H_
#define _P12LOCAL_H_

#include "plarena.h"
#include "secoidt.h"
#include "secasn1.h"
#include "secder.h"
#include "certt.h"
#include "secpkcs7.h"
#include "pkcs12.h"
#include "p12.h"

/* helper functions */
extern const SEC_ASN1Template *
sec_pkcs12_choose_bag_type(void *src_or_dest, PRBool encoding);
extern const SEC_ASN1Template *
sec_pkcs12_choose_cert_crl_type(void *src_or_dest, PRBool encoding);
extern const SEC_ASN1Template *
sec_pkcs12_choose_shroud_type(void *src_or_dest, PRBool encoding);
extern SECItem *sec_pkcs12_generate_salt(void);
extern SECItem *sec_pkcs12_generate_key_from_password(SECOidTag algorithm,
                                                      SECItem *salt, SECItem *password);
extern SECItem *sec_pkcs12_generate_mac(SECItem *key, SECItem *msg,
                                        PRBool old_method);
extern SGNDigestInfo *sec_pkcs12_compute_thumbprint(SECItem *der_cert);
extern SECItem *sec_pkcs12_create_virtual_password(SECItem *password,
                                                   SECItem *salt, PRBool swapUnicodeBytes);
extern SECStatus sec_pkcs12_append_shrouded_key(SEC_PKCS12BaggageItem *bag,
                                                SEC_PKCS12ESPVKItem *espvk);
extern void *sec_pkcs12_find_object(SEC_PKCS12SafeContents *safe,
                                    SEC_PKCS12Baggage *baggage, SECOidTag objType,
                                    SECItem *nickname, SGNDigestInfo *thumbprint);
extern PRBool sec_pkcs12_convert_item_to_unicode(PLArenaPool *arena, SECItem *dest,
                                                 SECItem *src, PRBool zeroTerm,
                                                 PRBool asciiConvert, PRBool toUnicode);
extern CK_MECHANISM_TYPE sec_pkcs12_algtag_to_mech(SECOidTag algtag);

/* create functions */
extern SEC_PKCS12PFXItem *sec_pkcs12_new_pfx(void);
extern SEC_PKCS12SafeContents *sec_pkcs12_create_safe_contents(
    PLArenaPool *poolp);
extern SEC_PKCS12Baggage *sec_pkcs12_create_baggage(PLArenaPool *poolp);
extern SEC_PKCS12BaggageItem *sec_pkcs12_create_external_bag(SEC_PKCS12Baggage *luggage);
extern void SEC_PKCS12DestroyPFX(SEC_PKCS12PFXItem *pfx);
extern SEC_PKCS12AuthenticatedSafe *sec_pkcs12_new_asafe(PLArenaPool *poolp);

/* conversion from old to new */
extern SEC_PKCS12DecoderContext *
sec_PKCS12ConvertOldSafeToNew(PLArenaPool *arena, PK11SlotInfo *slot,
                              PRBool swapUnicode, SECItem *pwitem,
                              void *wincx, SEC_PKCS12SafeContents *safe,
                              SEC_PKCS12Baggage *baggage);

#endif
