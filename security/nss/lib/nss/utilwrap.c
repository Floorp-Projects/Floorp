/*
 * NSS utility functions
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secport.h"
#include "secoid.h"
#include "secitem.h"
#include "secdig.h"
#include "secder.h"
#include "secasn1.h"
#include "base64.h"
#include "nssb64.h"
#include "nssrwlk.h"
#include "cert.h"
#include "prerror.h"

/* wrappers for implementation in libnssutil3 */
#undef ATOB_AsciiToData
#undef ATOB_ConvertAsciiToItem
#undef BTOA_ConvertItemToAscii
#undef BTOA_DataToAscii
#undef CERT_GenTime2FormattedAscii
#undef DER_AsciiToTime
#undef DER_DecodeTimeChoice
#undef DER_Encode
#undef DER_EncodeTimeChoice
#undef DER_GeneralizedDayToAscii
#undef DER_GeneralizedTimeToTime
#undef DER_GetInteger
#undef DER_Lengths
#undef DER_TimeChoiceDayToAscii
#undef DER_TimeToGeneralizedTime
#undef DER_TimeToGeneralizedTimeArena
#undef DER_TimeToUTCTime
#undef DER_UTCDayToAscii
#undef DER_UTCTimeToAscii
#undef DER_UTCTimeToTime
#undef NSS_PutEnv
#undef NSSBase64_DecodeBuffer
#undef NSSBase64_EncodeItem
#undef NSSBase64Decoder_Create
#undef NSSBase64Decoder_Destroy
#undef NSSBase64Decoder_Update
#undef NSSBase64Encoder_Create
#undef NSSBase64Encoder_Destroy
#undef NSSBase64Encoder_Update
#undef NSSRWLock_Destroy
#undef NSSRWLock_HaveWriteLock
#undef NSSRWLock_LockRead
#undef NSSRWLock_LockWrite
#undef NSSRWLock_New
#undef NSSRWLock_UnlockRead
#undef NSSRWLock_UnlockWrite
#undef PORT_Alloc
#undef PORT_ArenaAlloc
#undef PORT_ArenaGrow
#undef PORT_ArenaMark
#undef PORT_ArenaRelease
#undef PORT_ArenaStrdup
#undef PORT_ArenaUnmark
#undef PORT_ArenaZAlloc
#undef PORT_Free
#undef PORT_FreeArena
#undef PORT_GetError
#undef PORT_NewArena
#undef PORT_Realloc
#undef PORT_SetError
#undef PORT_SetUCS2_ASCIIConversionFunction
#undef PORT_SetUCS2_UTF8ConversionFunction
#undef PORT_SetUCS4_UTF8ConversionFunction
#undef PORT_Strdup
#undef PORT_UCS2_ASCIIConversion
#undef PORT_UCS2_UTF8Conversion
#undef PORT_ZAlloc
#undef PORT_ZFree
#undef SEC_ASN1Decode
#undef SEC_ASN1DecodeInteger
#undef SEC_ASN1DecodeItem
#undef SEC_ASN1DecoderAbort
#undef SEC_ASN1DecoderClearFilterProc
#undef SEC_ASN1DecoderClearNotifyProc
#undef SEC_ASN1DecoderFinish
#undef SEC_ASN1DecoderSetFilterProc
#undef SEC_ASN1DecoderSetNotifyProc
#undef SEC_ASN1DecoderStart
#undef SEC_ASN1DecoderUpdate
#undef SEC_ASN1Encode
#undef SEC_ASN1EncodeInteger
#undef SEC_ASN1EncodeItem
#undef SEC_ASN1EncoderAbort
#undef SEC_ASN1EncoderClearNotifyProc
#undef SEC_ASN1EncoderClearStreaming
#undef SEC_ASN1EncoderClearTakeFromBuf
#undef SEC_ASN1EncoderFinish
#undef SEC_ASN1EncoderSetNotifyProc
#undef SEC_ASN1EncoderSetStreaming
#undef SEC_ASN1EncoderSetTakeFromBuf
#undef SEC_ASN1EncoderStart
#undef SEC_ASN1EncoderUpdate
#undef SEC_ASN1EncodeUnsignedInteger
#undef SEC_ASN1LengthLength
#undef SEC_QuickDERDecodeItem
#undef SECITEM_AllocItem
#undef SECITEM_ArenaDupItem
#undef SECITEM_CompareItem
#undef SECITEM_CopyItem
#undef SECITEM_DupItem
#undef SECITEM_FreeItem
#undef SECITEM_ItemsAreEqual
#undef SECITEM_ZfreeItem
#undef SECOID_AddEntry
#undef SECOID_CompareAlgorithmID
#undef SECOID_CopyAlgorithmID
#undef SECOID_DestroyAlgorithmID
#undef SECOID_FindOID
#undef SECOID_FindOIDByTag
#undef SECOID_FindOIDTag
#undef SECOID_FindOIDTagDescription
#undef SECOID_GetAlgorithmTag
#undef SECOID_SetAlgorithmID
#undef SGN_CompareDigestInfo
#undef SGN_CopyDigestInfo
#undef SGN_CreateDigestInfo
#undef SGN_DestroyDigestInfo

void *
PORT_Alloc(size_t bytes)
{
    return PORT_Alloc_Util(bytes);
}

void *
PORT_Realloc(void *oldptr, size_t bytes)
{
    return PORT_Realloc_Util(oldptr, bytes);
}

void *
PORT_ZAlloc(size_t bytes)
{
    return PORT_ZAlloc_Util(bytes);
}

void
PORT_Free(void *ptr)
{
    PORT_Free_Util(ptr);
}

void
PORT_ZFree(void *ptr, size_t len)
{
    PORT_ZFree_Util(ptr, len);
}

char *
PORT_Strdup(const char *str)
{
    return PORT_Strdup_Util(str);
}

void
PORT_SetError(int value)
{
    PORT_SetError_Util(value);
}

int
PORT_GetError(void)
{
    return PORT_GetError_Util();
}

PLArenaPool *
PORT_NewArena(unsigned long chunksize)
{
    return PORT_NewArena_Util(chunksize);
}

void *
PORT_ArenaAlloc(PLArenaPool *arena, size_t size)
{
    return PORT_ArenaAlloc_Util(arena, size);
}

void *
PORT_ArenaZAlloc(PLArenaPool *arena, size_t size)
{
    return PORT_ArenaZAlloc_Util(arena, size);
}

void
PORT_FreeArena(PLArenaPool *arena, PRBool zero)
{
    PORT_FreeArena_Util(arena, zero);
}

void *
PORT_ArenaGrow(PLArenaPool *arena, void *ptr, size_t oldsize, size_t newsize)
{
    return PORT_ArenaGrow_Util(arena, ptr, oldsize, newsize);
}

void *
PORT_ArenaMark(PLArenaPool *arena)
{
    return PORT_ArenaMark_Util(arena);
}

void
PORT_ArenaRelease(PLArenaPool *arena, void *mark)
{
    PORT_ArenaRelease_Util(arena, mark);
}

void
PORT_ArenaUnmark(PLArenaPool *arena, void *mark)
{
    PORT_ArenaUnmark_Util(arena, mark);
}

char *
PORT_ArenaStrdup(PLArenaPool *arena, const char *str)
{
    return PORT_ArenaStrdup_Util(arena, str);
}

void
PORT_SetUCS4_UTF8ConversionFunction(PORTCharConversionFunc convFunc)
{
    PORT_SetUCS4_UTF8ConversionFunction_Util(convFunc);
}

void
PORT_SetUCS2_ASCIIConversionFunction(PORTCharConversionWSwapFunc convFunc)
{ 
    PORT_SetUCS2_ASCIIConversionFunction_Util(convFunc);
}

void
PORT_SetUCS2_UTF8ConversionFunction(PORTCharConversionFunc convFunc)
{ 
    PORT_SetUCS2_UTF8ConversionFunction_Util(convFunc);
}

PRBool 
PORT_UCS2_UTF8Conversion(PRBool toUnicode, unsigned char *inBuf,
			 unsigned int inBufLen, unsigned char *outBuf,
			 unsigned int maxOutBufLen, unsigned int *outBufLen)
{
    return PORT_UCS2_UTF8Conversion_Util(toUnicode, inBuf, inBufLen, outBuf,
                                          maxOutBufLen, outBufLen);
} 

PRBool 
PORT_UCS2_ASCIIConversion(PRBool toUnicode, unsigned char *inBuf,
			  unsigned int inBufLen, unsigned char *outBuf,
			  unsigned int maxOutBufLen, unsigned int *outBufLen,
			  PRBool swapBytes)
{
    return PORT_UCS2_ASCIIConversion_Util(toUnicode, inBuf, inBufLen, outBuf,
			  maxOutBufLen, outBufLen, swapBytes);
}

int
NSS_PutEnv(const char * envVarName, const char * envValue)
{
    return NSS_PutEnv_Util(envVarName, envValue);
}

SECOidData *SECOID_FindOID( const SECItem *oid)
{
    return SECOID_FindOID_Util(oid);
}

SECOidTag SECOID_FindOIDTag(const SECItem *oid)
{
    return SECOID_FindOIDTag_Util(oid);
}

SECOidData *SECOID_FindOIDByTag(SECOidTag tagnum)
{
    return SECOID_FindOIDByTag_Util(tagnum);
}

SECStatus SECOID_SetAlgorithmID(PRArenaPool *arena, SECAlgorithmID *aid,
				   SECOidTag tag, SECItem *params)
{
    return SECOID_SetAlgorithmID_Util(arena, aid, tag, params);
}

SECStatus SECOID_CopyAlgorithmID(PRArenaPool *arena, SECAlgorithmID *dest,
				    SECAlgorithmID *src)
{
    return SECOID_CopyAlgorithmID_Util(arena, dest, src);
}

SECOidTag SECOID_GetAlgorithmTag(SECAlgorithmID *aid)
{
    return SECOID_GetAlgorithmTag_Util(aid);
}

void SECOID_DestroyAlgorithmID(SECAlgorithmID *aid, PRBool freeit)
{
    SECOID_DestroyAlgorithmID_Util(aid, freeit);
}

SECComparison SECOID_CompareAlgorithmID(SECAlgorithmID *a,
					   SECAlgorithmID *b)
{
    return SECOID_CompareAlgorithmID_Util(a, b);
}

const char *SECOID_FindOIDTagDescription(SECOidTag tagnum)
{
    return SECOID_FindOIDTagDescription_Util(tagnum);
}

SECOidTag SECOID_AddEntry(const SECOidData * src)
{
    return SECOID_AddEntry_Util(src);
}

SECItem *SECITEM_AllocItem(PRArenaPool *arena, SECItem *item,
				  unsigned int len)
{
    return SECITEM_AllocItem_Util(arena, item, len);
}

SECComparison SECITEM_CompareItem(const SECItem *a, const SECItem *b)
{
    return SECITEM_CompareItem_Util(a, b);
}

PRBool SECITEM_ItemsAreEqual(const SECItem *a, const SECItem *b)
{
    return SECITEM_ItemsAreEqual_Util(a, b);
}

SECStatus SECITEM_CopyItem(PRArenaPool *arena, SECItem *to, 
                                  const SECItem *from)
{
    return SECITEM_CopyItem_Util(arena, to, from);
}

SECItem *SECITEM_DupItem(const SECItem *from)
{
    return SECITEM_DupItem_Util(from);
}

SECItem *SECITEM_ArenaDupItem(PRArenaPool *arena, const SECItem *from)
{
    return SECITEM_ArenaDupItem_Util(arena, from);
}

void SECITEM_FreeItem(SECItem *zap, PRBool freeit)
{
    SECITEM_FreeItem_Util(zap, freeit);
}

void SECITEM_ZfreeItem(SECItem *zap, PRBool freeit)
{
    SECITEM_ZfreeItem_Util(zap, freeit);
}

SGNDigestInfo *SGN_CreateDigestInfo(SECOidTag algorithm,
					   unsigned char *sig,
					   unsigned int sigLen)
{
    return SGN_CreateDigestInfo_Util(algorithm, sig, sigLen);
}

void SGN_DestroyDigestInfo(SGNDigestInfo *info)
{
    SGN_DestroyDigestInfo_Util(info);
}

SECStatus  SGN_CopyDigestInfo(PRArenaPool *poolp,
					SGNDigestInfo *a, 
					SGNDigestInfo *b)
{
    return SGN_CopyDigestInfo_Util(poolp, a, b);
}

SECComparison SGN_CompareDigestInfo(SGNDigestInfo *a, SGNDigestInfo *b)
{
    return SGN_CompareDigestInfo_Util(a, b);
}

SECStatus DER_Encode(PRArenaPool *arena, SECItem *dest, DERTemplate *t,
			   void *src)
{
    return DER_Encode_Util(arena, dest, t, src);
}

SECStatus DER_Lengths(SECItem *item, int *header_len_p,
                             PRUint32 *contents_len_p)
{
    return DER_Lengths_Util(item, header_len_p, contents_len_p);
}

long DER_GetInteger(const SECItem *src)
{
    return DER_GetInteger_Util(src);
}

SECStatus DER_TimeToUTCTime(SECItem *result, int64 time)
{
    return DER_TimeToUTCTime_Util(result, time);
}

SECStatus DER_AsciiToTime(int64 *result, const char *string)
{
    return DER_AsciiToTime_Util(result, string);
}

SECStatus DER_UTCTimeToTime(int64 *result, const SECItem *time)
{
    return DER_UTCTimeToTime_Util(result, time);
}

char *DER_UTCTimeToAscii(SECItem *utcTime)
{
    return DER_UTCTimeToAscii_Util(utcTime);
}

char *DER_UTCDayToAscii(SECItem *utctime)
{
    return DER_UTCDayToAscii_Util(utctime);
}

char *DER_GeneralizedDayToAscii(SECItem *gentime)
{
    return DER_GeneralizedDayToAscii_Util(gentime);
}

char *DER_TimeChoiceDayToAscii(SECItem *timechoice)
{
    return DER_TimeChoiceDayToAscii_Util(timechoice);
}

SECStatus DER_TimeToGeneralizedTime(SECItem *dst, int64 gmttime)
{
    return DER_TimeToGeneralizedTime_Util(dst, gmttime);
}

SECStatus DER_TimeToGeneralizedTimeArena(PRArenaPool* arenaOpt,
                                                SECItem *dst, int64 gmttime)
{
    return DER_TimeToGeneralizedTimeArena_Util(arenaOpt, dst, gmttime);
}

SECStatus DER_GeneralizedTimeToTime(int64 *dst, const SECItem *time)
{
    return DER_GeneralizedTimeToTime_Util(dst, time);
}

char *CERT_GenTime2FormattedAscii (int64 genTime, char *format)
{
    return CERT_GenTime2FormattedAscii_Util(genTime, format);
}

SECStatus DER_DecodeTimeChoice(PRTime* output, const SECItem* input)
{
    return DER_DecodeTimeChoice_Util(output, input);
}

SECStatus DER_EncodeTimeChoice(PRArenaPool* arena, SECItem* output,
                                       PRTime input)
{
    return DER_EncodeTimeChoice_Util(arena, output, input);
}

SEC_ASN1DecoderContext *SEC_ASN1DecoderStart(PRArenaPool *pool,
						    void *dest,
						    const SEC_ASN1Template *t)
{
    return SEC_ASN1DecoderStart_Util(pool, dest, t);
}

SECStatus SEC_ASN1DecoderUpdate(SEC_ASN1DecoderContext *cx,
				       const char *buf,
				       unsigned long len)
{
    return SEC_ASN1DecoderUpdate_Util(cx, buf, len);
}

SECStatus SEC_ASN1DecoderFinish(SEC_ASN1DecoderContext *cx)
{
    return SEC_ASN1DecoderFinish_Util(cx);
}

void SEC_ASN1DecoderAbort(SEC_ASN1DecoderContext *cx, int error)
{
    SEC_ASN1DecoderAbort_Util(cx, error);
}

void SEC_ASN1DecoderSetFilterProc(SEC_ASN1DecoderContext *cx,
					 SEC_ASN1WriteProc fn,
					 void *arg, PRBool no_store)
{
    SEC_ASN1DecoderSetFilterProc_Util(cx, fn, arg, no_store);
}

void SEC_ASN1DecoderClearFilterProc(SEC_ASN1DecoderContext *cx)
{
    SEC_ASN1DecoderClearFilterProc_Util(cx);
}

void SEC_ASN1DecoderSetNotifyProc(SEC_ASN1DecoderContext *cx,
					 SEC_ASN1NotifyProc fn,
					 void *arg)
{
    SEC_ASN1DecoderSetNotifyProc_Util(cx, fn, arg);
}

void SEC_ASN1DecoderClearNotifyProc(SEC_ASN1DecoderContext *cx)
{
    SEC_ASN1DecoderClearNotifyProc_Util(cx);
}

SECStatus SEC_ASN1Decode(PRArenaPool *pool, void *dest,
				const SEC_ASN1Template *t,
				const char *buf, long len)
{
    return SEC_ASN1Decode_Util(pool, dest, t, buf, len);
}

SECStatus SEC_ASN1DecodeItem(PRArenaPool *pool, void *dest,
				    const SEC_ASN1Template *t,
				    const SECItem *src)
{
    return SEC_ASN1DecodeItem_Util(pool, dest, t, src);
}

SECStatus SEC_QuickDERDecodeItem(PRArenaPool* arena, void* dest,
                     const SEC_ASN1Template* templateEntry,
                     const SECItem* src)
{
    return SEC_QuickDERDecodeItem_Util(arena, dest, templateEntry, src);
}

SEC_ASN1EncoderContext *SEC_ASN1EncoderStart(const void *src,
						    const SEC_ASN1Template *t,
						    SEC_ASN1WriteProc fn,
						    void *output_arg)
{
    return SEC_ASN1EncoderStart_Util(src, t, fn, output_arg);
}

SECStatus SEC_ASN1EncoderUpdate(SEC_ASN1EncoderContext *cx,
				       const char *buf,
				       unsigned long len)
{
    return SEC_ASN1EncoderUpdate_Util(cx, buf, len);
}

void SEC_ASN1EncoderFinish(SEC_ASN1EncoderContext *cx)
{
    SEC_ASN1EncoderFinish_Util(cx);
}

void SEC_ASN1EncoderAbort(SEC_ASN1EncoderContext *cx, int error)
{
    SEC_ASN1EncoderAbort_Util(cx, error);
}

void SEC_ASN1EncoderSetNotifyProc(SEC_ASN1EncoderContext *cx,
					 SEC_ASN1NotifyProc fn,
					 void *arg)
{
    SEC_ASN1EncoderSetNotifyProc_Util(cx, fn, arg);
}

void SEC_ASN1EncoderClearNotifyProc(SEC_ASN1EncoderContext *cx)
{
    SEC_ASN1EncoderClearNotifyProc_Util(cx);
}

void SEC_ASN1EncoderSetStreaming(SEC_ASN1EncoderContext *cx)
{
    SEC_ASN1EncoderSetStreaming_Util(cx);
}

void SEC_ASN1EncoderClearStreaming(SEC_ASN1EncoderContext *cx)
{
    SEC_ASN1EncoderClearStreaming_Util(cx);
}

void SEC_ASN1EncoderSetTakeFromBuf(SEC_ASN1EncoderContext *cx)
{
    SEC_ASN1EncoderSetTakeFromBuf_Util(cx);
}

void SEC_ASN1EncoderClearTakeFromBuf(SEC_ASN1EncoderContext *cx)
{
    SEC_ASN1EncoderClearTakeFromBuf_Util(cx);
}

SECStatus SEC_ASN1Encode(const void *src, const SEC_ASN1Template *t,
				SEC_ASN1WriteProc output_proc,
				void *output_arg)
{
    return SEC_ASN1Encode_Util(src, t, output_proc, output_arg);
}

SECItem * SEC_ASN1EncodeItem(PRArenaPool *pool, SECItem *dest,
				    const void *src, const SEC_ASN1Template *t)
{
    return SEC_ASN1EncodeItem_Util(pool, dest, src, t);
}

SECItem * SEC_ASN1EncodeInteger(PRArenaPool *pool,
				       SECItem *dest, long value)
{
    return SEC_ASN1EncodeInteger_Util(pool, dest, value);
}

SECItem * SEC_ASN1EncodeUnsignedInteger(PRArenaPool *pool,
					       SECItem *dest,
					       unsigned long value)
{
    return SEC_ASN1EncodeUnsignedInteger_Util(pool, dest, value);
}

SECStatus SEC_ASN1DecodeInteger(SECItem *src,
				       unsigned long *value)
{
    return SEC_ASN1DecodeInteger_Util(src, value);
}

int SEC_ASN1LengthLength (unsigned long len)
{
    return SEC_ASN1LengthLength_Util(len);
}

char *BTOA_DataToAscii(const unsigned char *data, unsigned int len)
{
    return BTOA_DataToAscii_Util(data, len);
}

unsigned char *ATOB_AsciiToData(const char *string, unsigned int *lenp)
{
    return ATOB_AsciiToData_Util(string, lenp);
}
 
SECStatus ATOB_ConvertAsciiToItem(SECItem *binary_item, const char *ascii)
{
    return ATOB_ConvertAsciiToItem_Util(binary_item, ascii);
}

char *BTOA_ConvertItemToAscii(SECItem *binary_item)
{
    return BTOA_ConvertItemToAscii_Util(binary_item);
}

NSSBase64Decoder *
NSSBase64Decoder_Create (PRInt32 (*output_fn) (void *, const unsigned char *,
					       PRInt32),
			 void *output_arg)
{
    return NSSBase64Decoder_Create_Util(output_fn, output_arg);
}

NSSBase64Encoder *
NSSBase64Encoder_Create (PRInt32 (*output_fn) (void *, const char *, PRInt32),
			 void *output_arg)
{
    return NSSBase64Encoder_Create_Util(output_fn, output_arg);
}

SECStatus
NSSBase64Decoder_Update (NSSBase64Decoder *data, const char *buffer,
			 PRUint32 size)
{
    return NSSBase64Decoder_Update_Util(data, buffer, size);
}

SECStatus
NSSBase64Encoder_Update (NSSBase64Encoder *data, const unsigned char *buffer,
			 PRUint32 size)
{
    return NSSBase64Encoder_Update_Util(data, buffer, size);
}

SECStatus
NSSBase64Decoder_Destroy (NSSBase64Decoder *data, PRBool abort_p)
{
    return NSSBase64Decoder_Destroy_Util(data, abort_p);
}

SECStatus
NSSBase64Encoder_Destroy (NSSBase64Encoder *data, PRBool abort_p)
{
    return NSSBase64Encoder_Destroy_Util(data, abort_p);
}

SECItem *
NSSBase64_DecodeBuffer (PRArenaPool *arenaOpt, SECItem *outItemOpt,
			const char *inStr, unsigned int inLen)
{
    return NSSBase64_DecodeBuffer_Util(arenaOpt, outItemOpt, inStr, inLen);
}

char *
NSSBase64_EncodeItem (PRArenaPool *arenaOpt, char *outStrOpt,
		      unsigned int maxOutLen, SECItem *inItem)
{
    return NSSBase64_EncodeItem_Util(arenaOpt, outStrOpt, maxOutLen, inItem);
}

NSSRWLock* NSSRWLock_New(PRUint32 lock_rank, const char *lock_name)
{
    return NSSRWLock_New_Util(lock_rank, lock_name);
}

void NSSRWLock_Destroy(NSSRWLock *lock)
{
    NSSRWLock_Destroy_Util(lock);
}

void NSSRWLock_LockRead(NSSRWLock *lock)
{
    NSSRWLock_LockRead_Util(lock);
}

void NSSRWLock_LockWrite(NSSRWLock *lock)
{
    NSSRWLock_LockWrite_Util(lock);
}

void NSSRWLock_UnlockRead(NSSRWLock *lock)
{
    NSSRWLock_UnlockRead_Util(lock);
}

void NSSRWLock_UnlockWrite(NSSRWLock *lock)
{
    NSSRWLock_UnlockWrite_Util(lock);
}

PRBool NSSRWLock_HaveWriteLock(NSSRWLock *rwlock)
{
    return NSSRWLock_HaveWriteLock_Util(rwlock);
}

SECStatus __nss_InitLock(   PZLock    **ppLock, nssILockType ltype )
{
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
}

/* templates duplicated in libnss3 and libnssutil3 */

#undef NSS_Get_SEC_AnyTemplate
#undef NSS_Get_SEC_BitStringTemplate
#undef NSS_Get_SEC_BMPStringTemplate
#undef NSS_Get_SEC_BooleanTemplate
#undef NSS_Get_SEC_GeneralizedTimeTemplate
#undef NSS_Get_SEC_IA5StringTemplate
#undef NSS_Get_SEC_IntegerTemplate
#undef NSS_Get_SEC_NullTemplate
#undef NSS_Get_SEC_ObjectIDTemplate
#undef NSS_Get_SEC_OctetStringTemplate
#undef NSS_Get_SEC_PointerToAnyTemplate
#undef NSS_Get_SEC_PointerToOctetStringTemplate
#undef NSS_Get_SEC_SetOfAnyTemplate
#undef NSS_Get_SEC_UTCTimeTemplate
#undef NSS_Get_SEC_UTF8StringTemplate
#undef NSS_Get_SECOID_AlgorithmIDTemplate
#undef NSS_Get_sgn_DigestInfoTemplate
#undef SEC_AnyTemplate
#undef SEC_BitStringTemplate
#undef SEC_BMPStringTemplate
#undef SEC_BooleanTemplate
#undef SEC_GeneralizedTimeTemplate
#undef SEC_IA5StringTemplate
#undef SEC_IntegerTemplate
#undef SEC_NullTemplate
#undef SEC_ObjectIDTemplate
#undef SEC_OctetStringTemplate
#undef SEC_PointerToAnyTemplate
#undef SEC_PointerToOctetStringTemplate
#undef SEC_SetOfAnyTemplate
#undef SEC_UTCTimeTemplate
#undef SEC_UTF8StringTemplate
#undef SECOID_AlgorithmIDTemplate
#undef sgn_DigestInfoTemplate

#include "templates.c"

