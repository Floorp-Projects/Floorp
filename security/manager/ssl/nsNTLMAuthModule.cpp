/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNTLMAuthModule.h"

#include <time.h>

#include "ScopedNSSTypes.h"
#include "md4.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Endian.h"
#include "mozilla/Likely.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Snprintf.h"
#include "mozilla/Telemetry.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsICryptoHMAC.h"
#include "nsICryptoHash.h"
#include "nsIKeyModule.h"
#include "nsKeyModule.h"
#include "nsNSSShutDown.h"
#include "nsNativeCharsetUtils.h"
#include "nsNetCID.h"
#include "nsUnicharUtils.h"
#include "pk11pub.h"
#include "prsystem.h"

static bool sNTLMv1Forced = false;
static mozilla::LazyLogModule sNTLMLog("NTLM");

#define LOG(x) MOZ_LOG(sNTLMLog, mozilla::LogLevel::Debug, x)
#define LOG_ENABLED() MOZ_LOG_TEST(sNTLMLog, mozilla::LogLevel::Debug)

static void des_makekey(const uint8_t *raw, uint8_t *key);
static void des_encrypt(const uint8_t *key, const uint8_t *src, uint8_t *hash);

//-----------------------------------------------------------------------------
// this file contains a cross-platform NTLM authentication implementation. it
// is based on documentation from: http://davenport.sourceforge.net/ntlm.html
//-----------------------------------------------------------------------------

#define NTLM_NegotiateUnicode               0x00000001
#define NTLM_NegotiateOEM                   0x00000002
#define NTLM_RequestTarget                  0x00000004
#define NTLM_Unknown1                       0x00000008
#define NTLM_NegotiateSign                  0x00000010
#define NTLM_NegotiateSeal                  0x00000020
#define NTLM_NegotiateDatagramStyle         0x00000040
#define NTLM_NegotiateLanManagerKey         0x00000080
#define NTLM_NegotiateNetware               0x00000100
#define NTLM_NegotiateNTLMKey               0x00000200
#define NTLM_Unknown2                       0x00000400
#define NTLM_Unknown3                       0x00000800
#define NTLM_NegotiateDomainSupplied        0x00001000
#define NTLM_NegotiateWorkstationSupplied   0x00002000
#define NTLM_NegotiateLocalCall             0x00004000
#define NTLM_NegotiateAlwaysSign            0x00008000
#define NTLM_TargetTypeDomain               0x00010000
#define NTLM_TargetTypeServer               0x00020000
#define NTLM_TargetTypeShare                0x00040000
#define NTLM_NegotiateNTLM2Key              0x00080000
#define NTLM_RequestInitResponse            0x00100000
#define NTLM_RequestAcceptResponse          0x00200000
#define NTLM_RequestNonNTSessionKey         0x00400000
#define NTLM_NegotiateTargetInfo            0x00800000
#define NTLM_Unknown4                       0x01000000
#define NTLM_Unknown5                       0x02000000
#define NTLM_Unknown6                       0x04000000
#define NTLM_Unknown7                       0x08000000
#define NTLM_Unknown8                       0x10000000
#define NTLM_Negotiate128                   0x20000000
#define NTLM_NegotiateKeyExchange           0x40000000
#define NTLM_Negotiate56                    0x80000000

// we send these flags with our type 1 message
#define NTLM_TYPE1_FLAGS      \
  (NTLM_NegotiateUnicode |    \
   NTLM_NegotiateOEM |        \
   NTLM_RequestTarget |       \
   NTLM_NegotiateNTLMKey |    \
   NTLM_NegotiateAlwaysSign | \
   NTLM_NegotiateNTLM2Key)

static const char NTLM_SIGNATURE[] = "NTLMSSP";
static const char NTLM_TYPE1_MARKER[] = { 0x01, 0x00, 0x00, 0x00 };
static const char NTLM_TYPE2_MARKER[] = { 0x02, 0x00, 0x00, 0x00 };
static const char NTLM_TYPE3_MARKER[] = { 0x03, 0x00, 0x00, 0x00 };

#define NTLM_TYPE1_HEADER_LEN 32
#define NTLM_TYPE2_HEADER_LEN 48
#define NTLM_TYPE3_HEADER_LEN 64

/**
 * We don't actually send a LM response, but we still have to send something in this spot
 */
#define LM_RESP_LEN 24

#define NTLM_CHAL_LEN 8

#define NTLM_HASH_LEN 16
#define NTLMv2_HASH_LEN 16
#define NTLM_RESP_LEN 24
#define NTLMv2_RESP_LEN 16
#define NTLMv2_BLOB1_LEN 28

//-----------------------------------------------------------------------------

/**
 * Prints a description of flags to the NSPR Log, if enabled.
 */
static void LogFlags(uint32_t flags)
{
  if (!LOG_ENABLED())
    return;
#define TEST(_flag) \
  if (flags & NTLM_ ## _flag) \
    PR_LogPrint("    0x%08x (" # _flag ")\n", NTLM_ ## _flag)

  TEST(NegotiateUnicode);
  TEST(NegotiateOEM);
  TEST(RequestTarget);
  TEST(Unknown1);
  TEST(NegotiateSign);
  TEST(NegotiateSeal);
  TEST(NegotiateDatagramStyle);
  TEST(NegotiateLanManagerKey);
  TEST(NegotiateNetware);
  TEST(NegotiateNTLMKey);
  TEST(Unknown2);
  TEST(Unknown3);
  TEST(NegotiateDomainSupplied);
  TEST(NegotiateWorkstationSupplied);
  TEST(NegotiateLocalCall);
  TEST(NegotiateAlwaysSign);
  TEST(TargetTypeDomain);
  TEST(TargetTypeServer);
  TEST(TargetTypeShare);
  TEST(NegotiateNTLM2Key);
  TEST(RequestInitResponse);
  TEST(RequestAcceptResponse);
  TEST(RequestNonNTSessionKey);
  TEST(NegotiateTargetInfo);
  TEST(Unknown4);
  TEST(Unknown5);
  TEST(Unknown6);
  TEST(Unknown7);
  TEST(Unknown8);
  TEST(Negotiate128);
  TEST(NegotiateKeyExchange);
  TEST(Negotiate56);

#undef TEST
}

/**
 * Prints a hexdump of buf to the NSPR Log, if enabled.
 * @param tag Description of the data, will be printed in front of the data
 * @param buf the data to print
 * @param bufLen length of the data
 */
static void
LogBuf(const char *tag, const uint8_t *buf, uint32_t bufLen)
{
  int i;

  if (!LOG_ENABLED())
    return;

  PR_LogPrint("%s =\n", tag);
  char line[80];
  while (bufLen > 0)
  {
    int count = bufLen;
    if (count > 8)
      count = 8;

    strcpy(line, "    ");
    for (i=0; i<count; ++i)
    {
      int len = strlen(line);
      snprintf(line + len, sizeof(line) - len, "0x%02x ", int(buf[i]));
    }
    for (; i<8; ++i)
    {
      int len = strlen(line);
      snprintf(line + len, sizeof(line) - len, "     ");
    }

    int len = strlen(line);
    snprintf(line + len, sizeof(line) - len, "   ");
    for (i=0; i<count; ++i)
    {
      len = strlen(line);
      if (isprint(buf[i]))
        snprintf(line + len, sizeof(line) - len, "%c", buf[i]);
      else
        snprintf(line + len, sizeof(line) - len, ".");
    }
    PR_LogPrint("%s\n", line);

    bufLen -= count;
    buf += count;
  }
}

#include "plbase64.h"
#include "prmem.h"
/**
 * Print base64-encoded token to the NSPR Log.
 * @param name Description of the token, will be printed in front
 * @param token The token to print
 * @param tokenLen length of the data in token
 */
static void LogToken(const char *name, const void *token, uint32_t tokenLen)
{
  if (!LOG_ENABLED())
    return;

  char *b64data = PL_Base64Encode((const char *) token, tokenLen, nullptr);
  if (b64data)
  {
    PR_LogPrint("%s: %s\n", name, b64data);
    PR_Free(b64data);
  }
}

//-----------------------------------------------------------------------------

// byte order swapping
#define SWAP16(x) ((((x) & 0xff) << 8) | (((x) >> 8) & 0xff))
#define SWAP32(x) ((SWAP16((x) & 0xffff) << 16) | (SWAP16((x) >> 16)))

static void *
WriteBytes(void *buf, const void *data, uint32_t dataLen)
{
  memcpy(buf, data, dataLen);
  return (uint8_t *) buf + dataLen;
}

static void *
WriteDWORD(void *buf, uint32_t dword)
{
#ifdef IS_BIG_ENDIAN 
  // NTLM uses little endian on the wire
  dword = SWAP32(dword);
#endif
  return WriteBytes(buf, &dword, sizeof(dword));
}

static void *
WriteSecBuf(void *buf, uint16_t length, uint32_t offset)
{
#ifdef IS_BIG_ENDIAN
  length = SWAP16(length);
  offset = SWAP32(offset);
#endif
  buf = WriteBytes(buf, &length, sizeof(length));
  buf = WriteBytes(buf, &length, sizeof(length));
  buf = WriteBytes(buf, &offset, sizeof(offset));
  return buf;
}

#ifdef IS_BIG_ENDIAN
/**
 * WriteUnicodeLE copies a unicode string from one buffer to another.  The
 * resulting unicode string is in little-endian format.  The input string is
 * assumed to be in the native endianness of the local machine.  It is safe
 * to pass the same buffer as both input and output, which is a handy way to
 * convert the unicode buffer to little-endian on big-endian platforms.
 */
static void *
WriteUnicodeLE(void *buf, const char16_t *str, uint32_t strLen)
{
  // convert input string from BE to LE
  uint8_t *cursor = (uint8_t *) buf,
          *input  = (uint8_t *) str;
  for (uint32_t i=0; i<strLen; ++i, input+=2, cursor+=2)
  {
    // allow for the case where |buf == str|
    uint8_t temp = input[0];
    cursor[0] = input[1];
    cursor[1] = temp;
  }
  return buf;
}
#endif

static uint16_t
ReadUint16(const uint8_t *&buf)
{
  uint16_t x = ((uint16_t) buf[0]) | ((uint16_t) buf[1] << 8);
  buf += sizeof(x);
  return x;
}

static uint32_t
ReadUint32(const uint8_t *&buf)
{
  uint32_t x = ( (uint32_t) buf[0])        |
               (((uint32_t) buf[1]) << 8)  |
               (((uint32_t) buf[2]) << 16) |
               (((uint32_t) buf[3]) << 24);
  buf += sizeof(x);
  return x;
}

//-----------------------------------------------------------------------------

static void
ZapBuf(void *buf, size_t bufLen)
{
  memset(buf, 0, bufLen);
}

static void
ZapString(nsString &s)
{
  ZapBuf(s.BeginWriting(), s.Length() * 2);
}

/**
 * NTLM_Hash computes the NTLM hash of the given password.
 *
 * @param password
 *        null-terminated unicode password.
 * @param hash
 *        16-byte result buffer
 */
static void
NTLM_Hash(const nsString &password, unsigned char *hash)
{
  uint32_t len = password.Length();
  uint8_t *passbuf;
  
#ifdef IS_BIG_ENDIAN
  passbuf = (uint8_t *) malloc(len * 2);
  WriteUnicodeLE(passbuf, password.get(), len);
#else
  passbuf = (uint8_t *) password.get();
#endif

  md4sum(passbuf, len * 2, hash);

#ifdef IS_BIG_ENDIAN
  ZapBuf(passbuf, len * 2);
  free(passbuf);
#endif
}

//-----------------------------------------------------------------------------

/** 
 * LM_Response generates the LM response given a 16-byte password hash and the
 * challenge from the Type-2 message.
 *
 * @param hash
 *        16-byte password hash
 * @param challenge
 *        8-byte challenge from Type-2 message
 * @param response
 *        24-byte buffer to contain the LM response upon return
 */
static void
LM_Response(const uint8_t *hash, const uint8_t *challenge, uint8_t *response)
{
  uint8_t keybytes[21], k1[8], k2[8], k3[8];

  memcpy(keybytes, hash, 16);
  ZapBuf(keybytes + 16, 5);

  des_makekey(keybytes     , k1);
  des_makekey(keybytes +  7, k2);
  des_makekey(keybytes + 14, k3);

  des_encrypt(k1, challenge, response);
  des_encrypt(k2, challenge, response + 8);
  des_encrypt(k3, challenge, response + 16);
}

//-----------------------------------------------------------------------------

static nsresult
GenerateType1Msg(void **outBuf, uint32_t *outLen)
{
  //
  // verify that bufLen is sufficient
  //
  *outLen = NTLM_TYPE1_HEADER_LEN;
  *outBuf = moz_xmalloc(*outLen);
  if (!*outBuf)
    return NS_ERROR_OUT_OF_MEMORY;

  //
  // write out type 1 msg
  //
  void *cursor = *outBuf;

  // 0 : signature
  cursor = WriteBytes(cursor, NTLM_SIGNATURE, sizeof(NTLM_SIGNATURE));

  // 8 : marker
  cursor = WriteBytes(cursor, NTLM_TYPE1_MARKER, sizeof(NTLM_TYPE1_MARKER));

  // 12 : flags
  cursor = WriteDWORD(cursor, NTLM_TYPE1_FLAGS);

  //
  // NOTE: it is common for the domain and workstation fields to be empty.
  //       this is true of Win2k clients, and my guess is that there is
  //       little utility to sending these strings before the charset has
  //       been negotiated.  we follow suite -- anyways, it doesn't hurt
  //       to save some bytes on the wire ;-)
  //

  // 16 : supplied domain security buffer (empty)
  cursor = WriteSecBuf(cursor, 0, 0);

  // 24 : supplied workstation security buffer (empty)
  cursor = WriteSecBuf(cursor, 0, 0);

  return NS_OK;
}

struct Type2Msg
{
  uint32_t    flags;                    // NTLM_Xxx bitwise combination
  uint8_t     challenge[NTLM_CHAL_LEN]; // 8 byte challenge
  const uint8_t *target;                // target string (type depends on flags)
  uint32_t    targetLen;                // target length in bytes
  const uint8_t *targetInfo;            // target Attribute-Value pairs (DNS domain, et al)
  uint32_t    targetInfoLen;            // target AV pairs length in bytes
};

static nsresult
ParseType2Msg(const void *inBuf, uint32_t inLen, Type2Msg *msg)
{
  // make sure inBuf is long enough to contain a meaningful type2 msg.
  //
  // 0  NTLMSSP Signature
  // 8  NTLM Message Type
  // 12 Target Name
  // 20 Flags
  // 24 Challenge
  // 32 targetInfo
  // 48 start of optional data blocks
  //
  if (inLen < NTLM_TYPE2_HEADER_LEN)
    return NS_ERROR_UNEXPECTED;

  const uint8_t *cursor = reinterpret_cast<const uint8_t*>(inBuf);

  // verify NTLMSSP signature
  if (memcmp(cursor, NTLM_SIGNATURE, sizeof(NTLM_SIGNATURE)) != 0)
    return NS_ERROR_UNEXPECTED;

  cursor += sizeof(NTLM_SIGNATURE);

  // verify Type-2 marker
  if (memcmp(cursor, NTLM_TYPE2_MARKER, sizeof(NTLM_TYPE2_MARKER)) != 0)
    return NS_ERROR_UNEXPECTED;

  cursor += sizeof(NTLM_TYPE2_MARKER);

  // Read target name security buffer: ...
  // ... read target length.
  uint32_t targetLen = ReadUint16(cursor);
  // ... skip next 16-bit "allocated space" value.
  ReadUint16(cursor);
  // ... read offset from inBuf.
  uint32_t offset = ReadUint32(cursor);
  mozilla::CheckedInt<uint32_t> targetEnd = offset;
  targetEnd += targetLen;
  // Check the offset / length combo is in range of the input buffer, including
  // integer overflow checking.
  if (MOZ_LIKELY(targetEnd.isValid() && targetEnd.value() <= inLen)) {
    msg->targetLen = targetLen;
    msg->target = reinterpret_cast<const uint8_t*>(inBuf) + offset;
  } else {
    // Do not error out, for (conservative) backward compatibility.
    msg->targetLen = 0;
    msg->target = nullptr;
  }

  // read flags
  msg->flags = ReadUint32(cursor);

  // read challenge
  memcpy(msg->challenge, cursor, sizeof(msg->challenge));
  cursor += sizeof(msg->challenge);

  LOG(("NTLM type 2 message:\n"));
  LogBuf("target", reinterpret_cast<const uint8_t*> (msg->target), msg->targetLen);
  LogBuf("flags", reinterpret_cast<const uint8_t*> (&msg->flags), 4);
  LogFlags(msg->flags);
  LogBuf("challenge", msg->challenge, sizeof(msg->challenge));

  // Read (and skip) the reserved field
  ReadUint32(cursor);
  ReadUint32(cursor);
  // Read target name security buffer: ...
  // ... read target length.
  uint32_t targetInfoLen = ReadUint16(cursor);
  // ... skip next 16-bit "allocated space" value.
  ReadUint16(cursor);
  // ... read offset from inBuf.
  offset = ReadUint32(cursor);
  mozilla::CheckedInt<uint32_t> targetInfoEnd = offset;
  targetInfoEnd += targetInfoLen;
  // Check the offset / length combo is in range of the input buffer, including
  // integer overflow checking.
  if (MOZ_LIKELY(targetInfoEnd.isValid() && targetInfoEnd.value() <= inLen)) {
    msg->targetInfoLen = targetInfoLen;
    msg->targetInfo = reinterpret_cast<const uint8_t*>(inBuf) + offset;
  } else {
    NS_ERROR("failed to get NTLMv2 target info");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

static nsresult
GenerateType3Msg(const nsString &domain,
                 const nsString &username,
                 const nsString &password,
                 const void     *inBuf,
                 uint32_t        inLen,
                 void          **outBuf,
                 uint32_t       *outLen)
{
  // inBuf contains Type-2 msg (the challenge) from server
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv;
  Type2Msg msg;

  rv = ParseType2Msg(inBuf, inLen, &msg);
  if (NS_FAILED(rv))
    return rv;

  bool unicode = (msg.flags & NTLM_NegotiateUnicode);

  // There is no negotiation for NTLMv2, so we just do it unless we are forced
  // by explict user configuration to use the older DES-based cryptography.
  bool ntlmv2 = (sNTLMv1Forced == false);

  // temporary buffers for unicode strings
#ifdef IS_BIG_ENDIAN
  nsAutoString ucsDomainBuf, ucsUserBuf;
#endif
  nsAutoCString hostBuf;
  nsAutoString ucsHostBuf; 
  // temporary buffers for oem strings
  nsAutoCString oemDomainBuf, oemUserBuf, oemHostBuf;
  // pointers and lengths for the string buffers; encoding is unicode if
  // the "negotiate unicode" flag was set in the Type-2 message.
  const void *domainPtr, *userPtr, *hostPtr;
  uint32_t domainLen, userLen, hostLen;

  // This is for NTLM, for NTLMv2 we set the new full length once we know it
  mozilla::CheckedInt<uint16_t> ntlmRespLen = NTLM_RESP_LEN;

  //
  // get domain name
  //
  if (unicode)
  {
#ifdef IS_BIG_ENDIAN
    ucsDomainBuf = domain;
    domainPtr = ucsDomainBuf.get();
    domainLen = ucsDomainBuf.Length() * 2;
    WriteUnicodeLE((void *) domainPtr, reinterpret_cast<const char16_t*> (domainPtr),
                   ucsDomainBuf.Length());
#else
    domainPtr = domain.get();
    domainLen = domain.Length() * 2;
#endif
  }
  else
  {
    NS_CopyUnicodeToNative(domain, oemDomainBuf);
    domainPtr = oemDomainBuf.get();
    domainLen = oemDomainBuf.Length();
  }

  //
  // get user name
  //
  if (unicode)
  {
#ifdef IS_BIG_ENDIAN
    ucsUserBuf = username;
    userPtr = ucsUserBuf.get();
    userLen = ucsUserBuf.Length() * 2;
    WriteUnicodeLE((void *) userPtr, reinterpret_cast<const char16_t*> (userPtr),
                   ucsUserBuf.Length());
#else
    userPtr = username.get();
    userLen = username.Length() * 2;
#endif
  }
  else
  {
    NS_CopyUnicodeToNative(username, oemUserBuf);
    userPtr = oemUserBuf.get();
    userLen = oemUserBuf.Length();
  }

  //
  // get workstation name
  // (do not use local machine's hostname after bug 1046421)
  //
  rv = mozilla::Preferences::GetCString("network.generic-ntlm-auth.workstation",
                                        &hostBuf);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (unicode)
  {
    ucsHostBuf = NS_ConvertUTF8toUTF16(hostBuf);
    hostPtr = ucsHostBuf.get();
    hostLen = ucsHostBuf.Length() * 2;
#ifdef IS_BIG_ENDIAN
    WriteUnicodeLE((void *) hostPtr, reinterpret_cast<const char16_t*> (hostPtr),
                   ucsHostBuf.Length());
#endif
  }
  else
  {
    hostPtr = hostBuf.get();
    hostLen = hostBuf.Length();
  }

  //
  // now that we have generated all of the strings, we can allocate outBuf.
  //
  //
  // next, we compute the NTLM or NTLM2 responses.
  //
  uint8_t lmResp[LM_RESP_LEN];
  uint8_t ntlmResp[NTLM_RESP_LEN];
  uint8_t ntlmv2Resp[NTLMv2_RESP_LEN];
  uint8_t ntlmHash[NTLM_HASH_LEN];
  uint8_t ntlmv2_blob1[NTLMv2_BLOB1_LEN];
  if (ntlmv2) {
    // NTLMv2 mode, the default
    nsString userUpper, domainUpper;
    nsAutoCString ntlmHashStr;
    nsAutoCString ntlmv2HashStr;
    nsAutoCString lmv2ResponseStr;
    nsAutoCString ntlmv2ResponseStr;

    // temporary buffers for unicode strings
    nsAutoString ucsDomainUpperBuf;
    nsAutoString ucsUserUpperBuf;
    const void *domainUpperPtr;
    const void *userUpperPtr;
    uint32_t domainUpperLen;
    uint32_t userUpperLen;

    if (msg.targetInfoLen == 0) {
      NS_ERROR("failed to get NTLMv2 target info, can not do NTLMv2");
      return NS_ERROR_UNEXPECTED;
    }

    ToUpperCase(username, ucsUserUpperBuf);
    userUpperPtr = ucsUserUpperBuf.get();
    userUpperLen = ucsUserUpperBuf.Length() * 2;
#ifdef IS_BIG_ENDIAN
    WriteUnicodeLE((void *) userUpperPtr, reinterpret_cast<const char16_t*> (userUpperPtr),
                   ucsUserUpperBuf.Length());
#endif
    ToUpperCase(domain, ucsDomainUpperBuf);
    domainUpperPtr = ucsDomainUpperBuf.get();
    domainUpperLen = ucsDomainUpperBuf.Length() * 2;
#ifdef IS_BIG_ENDIAN
    WriteUnicodeLE((void *) domainUpperPtr, reinterpret_cast<const char16_t*> (domainUpperPtr),
                   ucsDomainUpperBuf.Length());
#endif

    NTLM_Hash(password, ntlmHash);
    ntlmHashStr = nsAutoCString(reinterpret_cast<const char *>(ntlmHash), NTLM_HASH_LEN);

    nsCOMPtr<nsIKeyObjectFactory> keyFactory =
        do_CreateInstance(NS_KEYMODULEOBJECTFACTORY_CONTRACTID, &rv);

    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsIKeyObject> ntlmKey =
        do_CreateInstance(NS_KEYMODULEOBJECT_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = keyFactory->KeyFromString(nsIKeyObject::HMAC, ntlmHashStr, getter_AddRefs(ntlmKey));
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsICryptoHMAC> hasher =
        do_CreateInstance(NS_CRYPTO_HMAC_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = hasher->Init(nsICryptoHMAC::MD5, ntlmKey);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = hasher->Update(reinterpret_cast<const uint8_t*> (userUpperPtr), userUpperLen);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = hasher->Update(reinterpret_cast<const uint8_t*> (domainUpperPtr), domainUpperLen);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = hasher->Finish(false, ntlmv2HashStr);
    if (NS_FAILED(rv)) {
      return rv;
    }

    uint8_t client_random[NTLM_CHAL_LEN];
    PK11_GenerateRandom(client_random, NTLM_CHAL_LEN);

    nsCOMPtr<nsIKeyObject> ntlmv2Key =
        do_CreateInstance(NS_KEYMODULEOBJECT_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Prepare the LMv2 response
    rv = keyFactory->KeyFromString(nsIKeyObject::HMAC, ntlmv2HashStr, getter_AddRefs(ntlmv2Key));
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = hasher->Init(nsICryptoHMAC::MD5, ntlmv2Key);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = hasher->Update(msg.challenge, NTLM_CHAL_LEN);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = hasher->Update(client_random, NTLM_CHAL_LEN);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = hasher->Finish(false, lmv2ResponseStr);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (lmv2ResponseStr.Length() != NTLMv2_HASH_LEN) {
      return NS_ERROR_UNEXPECTED;
    }

    memcpy(lmResp, lmv2ResponseStr.get(), NTLMv2_HASH_LEN);
    memcpy(lmResp + NTLMv2_HASH_LEN, client_random, NTLM_CHAL_LEN);

    memset(ntlmv2_blob1, 0, NTLMv2_BLOB1_LEN);

    time_t unix_time;
    uint64_t nt_time = time(&unix_time);
    nt_time += 11644473600LL;    // Number of seconds betwen 1601 and 1970
    nt_time *= 1000 * 1000 * 10; // Convert seconds to 100 ns units

    ntlmv2_blob1[0] = 1;
    ntlmv2_blob1[1] = 1;
    mozilla::LittleEndian::writeUint64(&ntlmv2_blob1[8], nt_time);
    PK11_GenerateRandom(&ntlmv2_blob1[16], NTLM_CHAL_LEN);

    rv = hasher->Init(nsICryptoHMAC::MD5, ntlmv2Key);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = hasher->Update(msg.challenge, NTLM_CHAL_LEN);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = hasher->Update(ntlmv2_blob1, NTLMv2_BLOB1_LEN);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = hasher->Update(reinterpret_cast<const uint8_t*> (msg.targetInfo), msg.targetInfoLen);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = hasher->Finish(false, ntlmv2ResponseStr);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (ntlmv2ResponseStr.Length() != NTLMv2_RESP_LEN) {
      return NS_ERROR_UNEXPECTED;
    }

    memcpy(ntlmv2Resp, ntlmv2ResponseStr.get(), NTLMv2_RESP_LEN);
    ntlmRespLen = NTLMv2_RESP_LEN + NTLMv2_BLOB1_LEN;
    ntlmRespLen += msg.targetInfoLen;
    if (!ntlmRespLen.isValid()) {
      NS_ERROR("failed to do NTLMv2: integer overflow?!?");
      return NS_ERROR_UNEXPECTED;
    }
  } else if (msg.flags & NTLM_NegotiateNTLM2Key) {
    // compute NTLM2 session response
    nsCString sessionHashString;
    const uint8_t *sessionHash;

    PK11_GenerateRandom(lmResp, NTLM_CHAL_LEN);
    memset(lmResp + NTLM_CHAL_LEN, 0, LM_RESP_LEN - NTLM_CHAL_LEN);

    nsCOMPtr<nsICryptoHash> hasher =
        do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = hasher->Init(nsICryptoHash::MD5);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = hasher->Update(msg.challenge, NTLM_CHAL_LEN);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = hasher->Update(lmResp, NTLM_CHAL_LEN);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = hasher->Finish(false, sessionHashString);
    if (NS_FAILED(rv)) {
      return rv;
    }

    sessionHash = reinterpret_cast<const uint8_t*> (sessionHashString.get());

    LogBuf("NTLM2 effective key: ", sessionHash, 8);

    NTLM_Hash(password, ntlmHash);
    LM_Response(ntlmHash, sessionHash, ntlmResp);
  } else {
    NTLM_Hash(password, ntlmHash);
    LM_Response(ntlmHash, msg.challenge, ntlmResp);

    // According to http://davenport.sourceforge.net/ntlm.html#ntlmVersion2,
    // the correct way to not send the LM hash is to send the NTLM hash twice
    // in both the LM and NTLM response fields.
    LM_Response(ntlmHash, msg.challenge, lmResp);
  }

  mozilla::CheckedInt<uint32_t> totalLen = NTLM_TYPE3_HEADER_LEN + LM_RESP_LEN;
  totalLen += hostLen;
  totalLen += domainLen;
  totalLen += userLen;
  totalLen += ntlmRespLen.value();

  if (!totalLen.isValid()) {
    NS_ERROR("failed preparing to allocate NTLM response: integer overflow?!?");
    return NS_ERROR_FAILURE;
  }
  *outBuf = moz_xmalloc(totalLen.value());
  *outLen = totalLen.value();
  if (!*outBuf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  //
  // finally, we assemble the Type-3 msg :-)
  //
  void *cursor = *outBuf;
  mozilla::CheckedInt<uint32_t> offset;

  // 0 : signature
  cursor = WriteBytes(cursor, NTLM_SIGNATURE, sizeof(NTLM_SIGNATURE));

  // 8 : marker
  cursor = WriteBytes(cursor, NTLM_TYPE3_MARKER, sizeof(NTLM_TYPE3_MARKER));

  // 12 : LM response sec buf
  offset = NTLM_TYPE3_HEADER_LEN;
  offset += domainLen;
  offset += userLen;
  offset += hostLen;
  if (!offset.isValid()) {
    NS_ERROR("failed preparing to write NTLM response: integer overflow?!?");
    return NS_ERROR_UNEXPECTED;
  }
  cursor = WriteSecBuf(cursor, LM_RESP_LEN, offset.value());
  memcpy((uint8_t *) *outBuf + offset.value(), lmResp, LM_RESP_LEN);

  // 20 : NTLM or NTLMv2 response sec buf
  offset += LM_RESP_LEN;
  if (!offset.isValid()) {
    NS_ERROR("failed preparing to write NTLM response: integer overflow?!?");
    return NS_ERROR_UNEXPECTED;
  }
  cursor = WriteSecBuf(cursor, ntlmRespLen.value(), offset.value());
  if (ntlmv2) {
    memcpy(reinterpret_cast<uint8_t*> (*outBuf) + offset.value(), ntlmv2Resp, NTLMv2_RESP_LEN);
    offset += NTLMv2_RESP_LEN;
    if (!offset.isValid()) {
      NS_ERROR("failed preparing to write NTLM response: integer overflow?!?");
      return NS_ERROR_UNEXPECTED;
    }
    memcpy(reinterpret_cast<uint8_t*> (*outBuf) + offset.value(), ntlmv2_blob1, NTLMv2_BLOB1_LEN);
    offset += NTLMv2_BLOB1_LEN;
    if (!offset.isValid()) {
      NS_ERROR("failed preparing to write NTLM response: integer overflow?!?");
      return NS_ERROR_UNEXPECTED;
    }
    memcpy(reinterpret_cast<uint8_t*> (*outBuf) + offset.value(), msg.targetInfo, msg.targetInfoLen);
  } else {
    memcpy(reinterpret_cast<uint8_t*> (*outBuf) + offset.value(), ntlmResp, NTLM_RESP_LEN);
  }
  // 28 : domain name sec buf
  offset = NTLM_TYPE3_HEADER_LEN;
  cursor = WriteSecBuf(cursor, domainLen, offset.value());
  memcpy((uint8_t *) *outBuf + offset.value(), domainPtr, domainLen);

  // 36 : user name sec buf
  offset += domainLen;
  if (!offset.isValid()) {
    NS_ERROR("failed preparing to write NTLM response: integer overflow?!?");
    return NS_ERROR_UNEXPECTED;
  }
  cursor = WriteSecBuf(cursor, userLen, offset.value());
  memcpy(reinterpret_cast<uint8_t*> (*outBuf) + offset.value(), userPtr, userLen);

  // 44 : workstation (host) name sec buf
  offset += userLen;
  if (!offset.isValid()) {
    NS_ERROR("failed preparing to write NTLM response: integer overflow?!?");
    return NS_ERROR_UNEXPECTED;
  }
  cursor = WriteSecBuf(cursor, hostLen, offset.value());
  memcpy(reinterpret_cast<uint8_t*> (*outBuf) + offset.value(), hostPtr, hostLen);

  // 52 : session key sec buf (not used)
  cursor = WriteSecBuf(cursor, 0, 0);

  // 60 : negotiated flags
  cursor = WriteDWORD(cursor, msg.flags & NTLM_TYPE1_FLAGS);

  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsNTLMAuthModule, nsIAuthModule)

nsNTLMAuthModule::~nsNTLMAuthModule()
{
  ZapString(mPassword);
}

nsresult
nsNTLMAuthModule::InitTest()
{
  static bool prefObserved = false;
  if (!prefObserved) {
    mozilla::Preferences::AddBoolVarCache(
      &sNTLMv1Forced, "network.auth.force-generic-ntlm-v1", sNTLMv1Forced);
    prefObserved = true;
  }

  nsNSSShutDownPreventionLock locker;
  //
  // disable NTLM authentication when FIPS mode is enabled.
  //
  return PK11_IsFIPS() ? NS_ERROR_NOT_AVAILABLE : NS_OK;
}

NS_IMETHODIMP
nsNTLMAuthModule::Init(const char      *serviceName,
                       uint32_t         serviceFlags,
                       const char16_t *domain,
                       const char16_t *username,
                       const char16_t *password)
{
  NS_ASSERTION((serviceFlags & ~nsIAuthModule::REQ_PROXY_AUTH) == nsIAuthModule::REQ_DEFAULT,
      "unexpected service flags");

  mDomain = domain;
  mUsername = username;
  mPassword = password;
  mNTLMNegotiateSent = false;

  static bool sTelemetrySent = false;
  if (!sTelemetrySent) {
      mozilla::Telemetry::Accumulate(
          mozilla::Telemetry::NTLM_MODULE_USED_2,
          serviceFlags & nsIAuthModule::REQ_PROXY_AUTH
              ? NTLM_MODULE_GENERIC_PROXY
              : NTLM_MODULE_GENERIC_DIRECT);
      sTelemetrySent = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNTLMAuthModule::GetNextToken(const void *inToken,
                               uint32_t    inTokenLen,
                               void      **outToken,
                               uint32_t   *outTokenLen)
{
  nsresult rv;
  nsNSSShutDownPreventionLock locker;
  //
  // disable NTLM authentication when FIPS mode is enabled.
  //
  if (PK11_IsFIPS())
    return NS_ERROR_NOT_AVAILABLE;

  if (mNTLMNegotiateSent) {
    // if inToken is non-null, and we have sent the NTLMSSP_NEGOTIATE (type 1),
    // then the NTLMSSP_CHALLENGE (type 2) is expected
    if (inToken) {
      LogToken("in-token", inToken, inTokenLen);
      // Now generate the NTLMSSP_AUTH (type 3)
      rv = GenerateType3Msg(mDomain, mUsername, mPassword, inToken,
			    inTokenLen, outToken, outTokenLen);
    } else {
      LOG(("NTLMSSP_NEGOTIATE already sent and presumably "
	   "rejected by the server, refusing to send another"));
      rv = NS_ERROR_UNEXPECTED;
    }
  } else {
    if (inToken) {
      LOG(("NTLMSSP_NEGOTIATE not sent but NTLM reply already received?!?"));
      rv = NS_ERROR_UNEXPECTED;
    } else {
      rv = GenerateType1Msg(outToken, outTokenLen);
      if (NS_SUCCEEDED(rv)) {
	mNTLMNegotiateSent = true;
      }
    }
  }

  if (NS_SUCCEEDED(rv))
    LogToken("out-token", *outToken, *outTokenLen);

  return rv;
}

NS_IMETHODIMP
nsNTLMAuthModule::Unwrap(const void *inToken,
                        uint32_t    inTokenLen,
                        void      **outToken,
                        uint32_t   *outTokenLen)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNTLMAuthModule::Wrap(const void *inToken,
                       uint32_t    inTokenLen,
                       bool        confidential,
                       void      **outToken,
                       uint32_t   *outTokenLen)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// DES support code

// set odd parity bit (in least significant bit position)
static uint8_t
des_setkeyparity(uint8_t x)
{
  if ((((x >> 7) ^ (x >> 6) ^ (x >> 5) ^
        (x >> 4) ^ (x >> 3) ^ (x >> 2) ^
        (x >> 1)) & 0x01) == 0)
    x |= 0x01;
  else
    x &= 0xfe;
  return x;
}

// build 64-bit des key from 56-bit raw key
static void
des_makekey(const uint8_t *raw, uint8_t *key)
{
  key[0] = des_setkeyparity(raw[0]);
  key[1] = des_setkeyparity((raw[0] << 7) | (raw[1] >> 1));
  key[2] = des_setkeyparity((raw[1] << 6) | (raw[2] >> 2));
  key[3] = des_setkeyparity((raw[2] << 5) | (raw[3] >> 3));
  key[4] = des_setkeyparity((raw[3] << 4) | (raw[4] >> 4));
  key[5] = des_setkeyparity((raw[4] << 3) | (raw[5] >> 5));
  key[6] = des_setkeyparity((raw[5] << 2) | (raw[6] >> 6));
  key[7] = des_setkeyparity((raw[6] << 1));
}

// run des encryption algorithm (using NSS)
static void
des_encrypt(const uint8_t *key, const uint8_t *src, uint8_t *hash)
{
  CK_MECHANISM_TYPE cipherMech = CKM_DES_ECB;
  PK11SymKey *symkey = nullptr;
  PK11Context *ctxt = nullptr;
  SECItem keyItem, *param = nullptr;
  SECStatus rv;
  unsigned int n;

  mozilla::UniquePK11SlotInfo slot(PK11_GetBestSlot(cipherMech, nullptr));
  if (!slot)
  {
    NS_ERROR("no slot");
    goto done;
  }

  keyItem.data = (uint8_t *) key;
  keyItem.len = 8;
  symkey = PK11_ImportSymKey(slot.get(), cipherMech,
                             PK11_OriginUnwrap, CKA_ENCRYPT,
                             &keyItem, nullptr);
  if (!symkey)
  {
    NS_ERROR("no symkey");
    goto done;
  }

  // no initialization vector required
  param = PK11_ParamFromIV(cipherMech, nullptr);
  if (!param)
  {
    NS_ERROR("no param");
    goto done;
  }

  ctxt = PK11_CreateContextBySymKey(cipherMech, CKA_ENCRYPT,
                                    symkey, param);
  if (!ctxt) {
    NS_ERROR("no context");
    goto done;
  }

  rv = PK11_CipherOp(ctxt, hash, (int *) &n, 8, (uint8_t *) src, 8);
  if (rv != SECSuccess) {
    NS_ERROR("des failure");
    goto done;
  }

  rv = PK11_DigestFinal(ctxt, hash+8, &n, 0);
  if (rv != SECSuccess) {
    NS_ERROR("des failure");
    goto done;
  }

done:
  if (ctxt)
    PK11_DestroyContext(ctxt, true);
  if (symkey)
    PK11_FreeSymKey(symkey);
  if (param)
    SECITEM_FreeItem(param, true);
}
