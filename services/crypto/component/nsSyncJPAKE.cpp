/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSyncJPAKE.h"
#include "mozilla/ModuleUtils.h"
#include <pk11pub.h>
#include <keyhi.h>
#include <pkcs11.h>
#include <nscore.h>
#include <secmodt.h>
#include <secport.h>
#include <secerr.h>
#include <nsDebug.h>
#include <nsError.h>
#include <base64.h>
#include <nsString.h>

using mozilla::fallible;

static bool
hex_from_2char(const unsigned char *c2, unsigned char *byteval)
{
  int i;
  unsigned char offset;
  *byteval = 0;
  for (i=0; i<2; i++) {
    if (c2[i] >= '0' && c2[i] <= '9') {
      offset = c2[i] - '0';
      *byteval |= offset << 4*(1-i);
    } else if (c2[i] >= 'a' && c2[i] <= 'f') {
      offset = c2[i] - 'a';
      *byteval |= (offset + 10) << 4*(1-i);
    } else if (c2[i] >= 'A' && c2[i] <= 'F') {
      offset = c2[i] - 'A';
      *byteval |= (offset + 10) << 4*(1-i);
    } else {
      return false;
    }
  }
  return true;
}

static bool
fromHex(const char * str, unsigned char * p, size_t sLen)
{
  size_t i;
  if (sLen & 1)
    return false;

  for (i = 0; i < sLen / 2; ++i) {
    if (!hex_from_2char((const unsigned char *) str + (2*i),
                        (unsigned char *) p + i)) {
      return false;
    }
  }
  return true;
}

static nsresult
fromHexString(const nsACString & str, unsigned char * p, size_t pMaxLen)
{
  char * strData = (char *) str.Data();
  unsigned len = str.Length();
  NS_ENSURE_ARG(len / 2 <= pMaxLen);
  if (!fromHex(strData, p, len)) {
    return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}

static bool
toHexString(const unsigned char * str, unsigned len, nsACString & out)
{
  static const char digits[] = "0123456789ABCDEF";
  if (!out.SetCapacity(2 * len, fallible))
    return false;
  out.SetLength(0);
  for (unsigned i = 0; i < len; ++i) {
    out.Append(digits[str[i] >> 4]);
    out.Append(digits[str[i] & 0x0f]);
  }
  return true;
}

static nsresult
mapErrno()
{
  int err = PORT_GetError();
  switch (err) {
    case SEC_ERROR_NO_MEMORY:   return NS_ERROR_OUT_OF_MEMORY;
    default:                    return NS_ERROR_UNEXPECTED;
  }
}

#define NUM_ELEM(x) (sizeof(x) / sizeof (x)[0])

static const char p[] = 
  "90066455B5CFC38F9CAA4A48B4281F292C260FEEF01FD61037E56258A7795A1C"
  "7AD46076982CE6BB956936C6AB4DCFE05E6784586940CA544B9B2140E1EB523F"
  "009D20A7E7880E4E5BFA690F1B9004A27811CD9904AF70420EEFD6EA11EF7DA1"
  "29F58835FF56B89FAA637BC9AC2EFAAB903402229F491D8D3485261CD068699B"
  "6BA58A1DDBBEF6DB51E8FE34E8A78E542D7BA351C21EA8D8F1D29F5D5D159394"
  "87E27F4416B0CA632C59EFD1B1EB66511A5A0FBF615B766C5862D0BD8A3FE7A0"
  "E0DA0FB2FE1FCB19E8F9996A8EA0FCCDE538175238FC8B0EE6F29AF7F642773E"
  "BE8CD5402415A01451A840476B2FCEB0E388D30D4B376C37FE401C2A2C2F941D"
  "AD179C540C1C8CE030D460C4D983BE9AB0B20F69144C1AE13F9383EA1C08504F"
  "B0BF321503EFE43488310DD8DC77EC5B8349B8BFE97C2C560EA878DE87C11E3D"
  "597F1FEA742D73EEC7F37BE43949EF1A0D15C3F3E3FC0A8335617055AC91328E"
  "C22B50FC15B941D3D1624CD88BC25F3E941FDDC6200689581BFEC416B4B2CB73";
static const char q[] =
  "CFA0478A54717B08CE64805B76E5B14249A77A4838469DF7F7DC987EFCCFB11D";
static const char g[] = 
  "5E5CBA992E0A680D885EB903AEA78E4A45A469103D448EDE3B7ACCC54D521E37"
  "F84A4BDD5B06B0970CC2D2BBB715F7B82846F9A0C393914C792E6A923E2117AB"
  "805276A975AADB5261D91673EA9AAFFEECBFA6183DFCB5D3B7332AA19275AFA1"
  "F8EC0B60FB6F66CC23AE4870791D5982AAD1AA9485FD8F4A60126FEB2CF05DB8"
  "A7F0F09B3397F3937F2E90B9E5B9C9B6EFEF642BC48351C46FB171B9BFA9EF17"
  "A961CE96C7E7A7CC3D3D03DFAD1078BA21DA425198F07D2481622BCE45969D9C"
  "4D6063D72AB7A0F08B2F49A7CC6AF335E08C4720E31476B67299E231F8BD90B3"
  "9AC3AE3BE0C6B6CACEF8289A2E2873D58E51E029CAFBD55E6841489AB66B5B4B"
  "9BA6E2F784660896AFF387D92844CCB8B69475496DE19DA2E58259B090489AC8"
  "E62363CDF82CFD8EF2A427ABCD65750B506F56DDE3B988567A88126B914D7828"
  "E2B63A6D7ED0747EC59E0E0A23CE7D8A74C1D2C2A7AFB6A29799620F00E11C33"
  "787F7DED3B30E1A22D09F1FBDA1ABBBFBF25CAE05A13F812E34563F99410E73B";

NS_IMETHODIMP nsSyncJPAKE::Round1(const nsACString & aSignerID,
                                  nsACString & aGX1,
                                  nsACString & aGV1,
                                  nsACString & aR1,
                                  nsACString & aGX2,
                                  nsACString & aGV2,
                                  nsACString & aR2)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_STATE(round == JPAKENotStarted);
  NS_ENSURE_STATE(key == nullptr);

  static CK_MECHANISM_TYPE mechanisms[] = {
    CKM_NSS_JPAKE_ROUND1_SHA256,
    CKM_NSS_JPAKE_ROUND2_SHA256,
    CKM_NSS_JPAKE_FINAL_SHA256
  };

  ScopedPK11SlotInfo slot(PK11_GetBestSlotMultiple(mechanisms,
                                                   NUM_ELEM(mechanisms),
                                                   nullptr));
  NS_ENSURE_STATE(slot != nullptr);
    
  CK_BYTE pBuf[(NUM_ELEM(p) - 1) / 2];
  CK_BYTE qBuf[(NUM_ELEM(q) - 1) / 2];
  CK_BYTE gBuf[(NUM_ELEM(g) - 1) / 2];
    
  CK_KEY_TYPE keyType = CKK_NSS_JPAKE_ROUND1;
  NS_ENSURE_STATE(fromHex(p, pBuf, (NUM_ELEM(p) - 1)));
  NS_ENSURE_STATE(fromHex(q, qBuf, (NUM_ELEM(q) - 1)));
  NS_ENSURE_STATE(fromHex(g, gBuf, (NUM_ELEM(g) - 1)));
  CK_ATTRIBUTE keyTemplate[] = {
    { CKA_NSS_JPAKE_SIGNERID, (CK_BYTE *) aSignerID.Data(),
                              aSignerID.Length() },
    { CKA_KEY_TYPE, &keyType, sizeof keyType },
    { CKA_PRIME,    pBuf,     sizeof pBuf },
    { CKA_SUBPRIME, qBuf,     sizeof qBuf },
    { CKA_BASE,     gBuf,     sizeof gBuf }
  };

  CK_BYTE gx1Buf[NUM_ELEM(p) / 2];
  CK_BYTE gv1Buf[NUM_ELEM(p) / 2];
  CK_BYTE r1Buf [NUM_ELEM(p) / 2];
  CK_BYTE gx2Buf[NUM_ELEM(p) / 2];
  CK_BYTE gv2Buf[NUM_ELEM(p) / 2];
  CK_BYTE r2Buf [NUM_ELEM(p) / 2];
  CK_NSS_JPAKERound1Params rp = {
      { gx1Buf, sizeof gx1Buf, gv1Buf, sizeof gv1Buf, r1Buf, sizeof r1Buf },
      { gx2Buf, sizeof gx2Buf, gv2Buf, sizeof gv2Buf, r2Buf, sizeof r2Buf }
  };
  SECItem paramsItem;
  paramsItem.data = (unsigned char *) &rp;
  paramsItem.len = sizeof rp;
  key = PK11_KeyGenWithTemplate(slot, CKM_NSS_JPAKE_ROUND1_SHA256,
                                CKM_NSS_JPAKE_ROUND1_SHA256,
                                &paramsItem, keyTemplate,
                                NUM_ELEM(keyTemplate), nullptr);
  nsresult rv = key != nullptr
              ? NS_OK
              : mapErrno();
  if (rv == NS_OK) {
    NS_ENSURE_TRUE(toHexString(rp.gx1.pGX, rp.gx1.ulGXLen, aGX1) &&
                   toHexString(rp.gx1.pGV, rp.gx1.ulGVLen, aGV1) &&
                   toHexString(rp.gx1.pR,  rp.gx1.ulRLen,  aR1)  &&
                   toHexString(rp.gx2.pGX, rp.gx2.ulGXLen, aGX2) &&
                   toHexString(rp.gx2.pGV, rp.gx2.ulGVLen, aGV2) &&
                   toHexString(rp.gx2.pR,  rp.gx2.ulRLen,  aR2),
                   NS_ERROR_OUT_OF_MEMORY);
    round = JPAKEBeforeRound2;
  }
  return rv;
}

NS_IMETHODIMP nsSyncJPAKE::Round2(const nsACString & aPeerID,
                                  const nsACString & aPIN,
                                  const nsACString & aGX3,
                                  const nsACString & aGV3,
                                  const nsACString & aR3,
                                  const nsACString & aGX4,
                                  const nsACString & aGV4,
                                  const nsACString & aR4,
                                  nsACString & aA,
                                  nsACString & aGVA,
                                  nsACString & aRA)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_STATE(round == JPAKEBeforeRound2);
  NS_ENSURE_STATE(key != nullptr);
  NS_ENSURE_ARG(!aPeerID.IsEmpty());

  /* PIN cannot be equal to zero when converted to a bignum. NSS 3.12.9 J-PAKE
     assumes that the caller has already done this check. Future versions of 
     NSS J-PAKE will do this check internally. See Bug 609068 Comment 4 */
  bool foundNonZero = false;
  for (size_t i = 0; i < aPIN.Length(); ++i) {
    if (aPIN[i] != 0) {
      foundNonZero = true;
      break;
    }
  }
  NS_ENSURE_ARG(foundNonZero);

  CK_BYTE gx3Buf[NUM_ELEM(p)/2], gv3Buf[NUM_ELEM(p)/2], r3Buf [NUM_ELEM(p)/2];
  CK_BYTE gx4Buf[NUM_ELEM(p)/2], gv4Buf[NUM_ELEM(p)/2], r4Buf [NUM_ELEM(p)/2];
  CK_BYTE gxABuf[NUM_ELEM(p)/2], gvABuf[NUM_ELEM(p)/2], rABuf [NUM_ELEM(p)/2];
  nsresult         rv = fromHexString(aGX3, gx3Buf, sizeof gx3Buf);
  if (rv == NS_OK) rv = fromHexString(aGV3, gv3Buf, sizeof gv3Buf);
  if (rv == NS_OK) rv = fromHexString(aR3,  r3Buf,  sizeof r3Buf);
  if (rv == NS_OK) rv = fromHexString(aGX4, gx4Buf, sizeof gx4Buf);
  if (rv == NS_OK) rv = fromHexString(aGV4, gv4Buf, sizeof gv4Buf);
  if (rv == NS_OK) rv = fromHexString(aR4,  r4Buf,  sizeof r4Buf);
  if (rv != NS_OK)
    return rv;

  CK_NSS_JPAKERound2Params rp;
  rp.pSharedKey = (CK_BYTE *) aPIN.Data();
  rp.ulSharedKeyLen = aPIN.Length();
  rp.gx3.pGX = gx3Buf; rp.gx3.ulGXLen = aGX3.Length() / 2;
  rp.gx3.pGV = gv3Buf; rp.gx3.ulGVLen = aGV3.Length() / 2;
  rp.gx3.pR  = r3Buf;  rp.gx3.ulRLen  = aR3 .Length() / 2;
  rp.gx4.pGX = gx4Buf; rp.gx4.ulGXLen = aGX4.Length() / 2;
  rp.gx4.pGV = gv4Buf; rp.gx4.ulGVLen = aGV4.Length() / 2;
  rp.gx4.pR  = r4Buf;  rp.gx4.ulRLen  = aR4 .Length() / 2;
  rp.A.pGX   = gxABuf; rp.A  .ulGXLen = sizeof gxABuf;
  rp.A.pGV   = gvABuf; rp.A  .ulGVLen = sizeof gxABuf;
  rp.A.pR    = rABuf;  rp.A  .ulRLen  = sizeof gxABuf;

  // Bug 629090: NSS 3.12.9 J-PAKE fails to check that gx^4 != 1, so check here.
  bool gx4Good = false;
  for (unsigned i = 0; i < rp.gx4.ulGXLen; ++i) {
    if (rp.gx4.pGX[i] > 1 || (rp.gx4.pGX[i] != 0 && i < rp.gx4.ulGXLen - 1)) {
      gx4Good = true;
      break;
    }
  }
  NS_ENSURE_ARG(gx4Good);

  SECItem paramsItem;
  paramsItem.data = (unsigned char *) &rp;
  paramsItem.len = sizeof rp;
  CK_KEY_TYPE keyType = CKK_NSS_JPAKE_ROUND2;
  CK_ATTRIBUTE keyTemplate[] = {
    { CKA_NSS_JPAKE_PEERID, (CK_BYTE *) aPeerID.Data(), aPeerID.Length(), },
    { CKA_KEY_TYPE, &keyType, sizeof keyType }
  };
  ScopedPK11SymKey newKey(PK11_DeriveWithTemplate(key,
                                                  CKM_NSS_JPAKE_ROUND2_SHA256,
                                                  &paramsItem,
                                                  CKM_NSS_JPAKE_FINAL_SHA256,
                                                  CKA_DERIVE, 0,
                                                  keyTemplate,
                                                  NUM_ELEM(keyTemplate),
                                                  false));
  if (newKey != nullptr) {
    if (toHexString(rp.A.pGX, rp.A.ulGXLen, aA) &&
        toHexString(rp.A.pGV, rp.A.ulGVLen, aGVA) &&
        toHexString(rp.A.pR, rp.A.ulRLen, aRA)) {
      round = JPAKEAfterRound2;
      key = newKey.forget();
      return NS_OK;
    } else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    rv = mapErrno();
  }

  return rv;
}

static nsresult
setBase64(const unsigned char * data, unsigned len, nsACString & out)
{
  nsresult rv = NS_OK;
  const char * base64 = BTOA_DataToAscii(data, len);
  
  if (base64 != nullptr) {
    size_t len = PORT_Strlen(base64);
    if (out.SetCapacity(len, fallible)) {
      out.SetLength(0);
      out.Append(base64, len);
    } else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
    PORT_Free((void*) base64);
  } else {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  return rv;
}

static nsresult
base64KeyValue(PK11SymKey * key, nsACString & keyString)
{
  nsresult rv = NS_OK;
  if (PK11_ExtractKeyValue(key) == SECSuccess) {
    const SECItem * value = PK11_GetKeyData(key);
    rv = value != nullptr && value->data != nullptr && value->len > 0
       ? setBase64(value->data, value->len, keyString)
       : NS_ERROR_UNEXPECTED;
  } else {
    rv = mapErrno();
  }
  return rv;
}

static nsresult
extractBase64KeyValue(PK11SymKey * keyBlock, CK_ULONG bitPosition,
                      CK_MECHANISM_TYPE destMech, int keySize,
                      nsACString & keyString)
{
  SECItem paramsItem;
  paramsItem.data = (CK_BYTE *) &bitPosition;
  paramsItem.len = sizeof bitPosition;
  PK11SymKey * key = PK11_Derive(keyBlock, CKM_EXTRACT_KEY_FROM_KEY,
                                 &paramsItem, destMech,
                                 CKA_SIGN, keySize);
  if (key == nullptr)
    return mapErrno();
  nsresult rv = base64KeyValue(key, keyString);
  PK11_FreeSymKey(key);
  return rv;
}


NS_IMETHODIMP nsSyncJPAKE::Final(const nsACString & aB,
                                 const nsACString & aGVB,
                                 const nsACString & aRB,
                                 const nsACString & aHKDFInfo,
                                 nsACString & aAES256Key,
                                 nsACString & aHMAC256Key)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  static const unsigned AES256_KEY_SIZE = 256 / 8;
  static const unsigned HMAC_SHA256_KEY_SIZE = 256 / 8;
  CK_EXTRACT_PARAMS aesBitPosition = 0;
  CK_EXTRACT_PARAMS hmacBitPosition = aesBitPosition + (AES256_KEY_SIZE * 8);

  NS_ENSURE_STATE(round == JPAKEAfterRound2);
  NS_ENSURE_STATE(key != nullptr);

  CK_BYTE gxBBuf[NUM_ELEM(p)/2], gvBBuf[NUM_ELEM(p)/2], rBBuf [NUM_ELEM(p)/2];
  nsresult         rv = fromHexString(aB,   gxBBuf, sizeof gxBBuf);
  if (rv == NS_OK) rv = fromHexString(aGVB, gvBBuf, sizeof gvBBuf);
  if (rv == NS_OK) rv = fromHexString(aRB,  rBBuf,  sizeof rBBuf);
  if (rv != NS_OK)
    return rv;

  CK_NSS_JPAKEFinalParams rp;
  rp.B.pGX = gxBBuf; rp.B.ulGXLen = aB  .Length() / 2;
  rp.B.pGV = gvBBuf; rp.B.ulGVLen = aGVB.Length() / 2;
  rp.B.pR  = rBBuf;  rp.B.ulRLen  = aRB .Length() / 2;
  SECItem paramsItem;
  paramsItem.data = (unsigned char *) &rp;
  paramsItem.len = sizeof rp;
  ScopedPK11SymKey keyMaterial(PK11_Derive(key, CKM_NSS_JPAKE_FINAL_SHA256,
                                           &paramsItem, CKM_NSS_HKDF_SHA256,
                                           CKA_DERIVE, 0));
  ScopedPK11SymKey keyBlock;

  if (keyMaterial == nullptr)
    rv = mapErrno();

  if (rv == NS_OK) {
    CK_NSS_HKDFParams hkdfParams;
    hkdfParams.bExtract = CK_TRUE;
    hkdfParams.pSalt = nullptr;
    hkdfParams.ulSaltLen = 0;
    hkdfParams.bExpand = CK_TRUE;
    hkdfParams.pInfo = (CK_BYTE *) aHKDFInfo.Data();
    hkdfParams.ulInfoLen = aHKDFInfo.Length();
    paramsItem.data = (unsigned char *) &hkdfParams;
    paramsItem.len = sizeof hkdfParams;
    keyBlock = PK11_Derive(keyMaterial, CKM_NSS_HKDF_SHA256,
                           &paramsItem, CKM_EXTRACT_KEY_FROM_KEY,
                           CKA_DERIVE, AES256_KEY_SIZE + HMAC_SHA256_KEY_SIZE);
    if (keyBlock == nullptr)
      rv = mapErrno();
  }

  if (rv == NS_OK) {
    rv = extractBase64KeyValue(keyBlock, aesBitPosition, CKM_AES_CBC,
                               AES256_KEY_SIZE, aAES256Key);
  }
  if (rv == NS_OK) {
    rv = extractBase64KeyValue(keyBlock, hmacBitPosition, CKM_SHA256_HMAC,
                               HMAC_SHA256_KEY_SIZE, aHMAC256Key);
  }

  if (rv == NS_OK) {
    SECStatus srv = PK11_ExtractKeyValue(keyMaterial);
    NS_ENSURE_TRUE(srv == SECSuccess, NS_ERROR_UNEXPECTED);
    SECItem * keyMaterialBytes = PK11_GetKeyData(keyMaterial);
    NS_ENSURE_TRUE(keyMaterialBytes != nullptr, NS_ERROR_UNEXPECTED);
  }

  return rv;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSyncJPAKE)
NS_DEFINE_NAMED_CID(NS_SYNCJPAKE_CID);

nsSyncJPAKE::nsSyncJPAKE() : round(JPAKENotStarted), key(nullptr) { }

nsSyncJPAKE::~nsSyncJPAKE()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(ShutdownCalledFrom::Object);
}

void
nsSyncJPAKE::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void
nsSyncJPAKE::destructorSafeDestroyNSSReference()
{
  key.dispose();
}

static const mozilla::Module::CIDEntry kServicesCryptoCIDs[] = {
  { &kNS_SYNCJPAKE_CID, false, nullptr, nsSyncJPAKEConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kServicesCryptoContracts[] = {
  { NS_SYNCJPAKE_CONTRACTID, &kNS_SYNCJPAKE_CID },
  { nullptr }
};

static const mozilla::Module kServicesCryptoModule = {
  mozilla::Module::kVersion,
  kServicesCryptoCIDs,
  kServicesCryptoContracts
};

NSMODULE_DEFN(nsServicesCryptoModule) = &kServicesCryptoModule;
