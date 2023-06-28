
#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "prtypes.h" /* for PRUintXX */
#include "secport.h" /* for PORT_XXX */
#include "blapi.h"
#include "blapii.h"
#include "blapit.h"
#include "secerr.h"
#include "Hacl_Hash_SHA3.h"

struct SHA3ContextStr {
    Hacl_Streaming_Keccak_state *st;
};

SHA3_224Context *
SHA3_224_NewContext()
{
    SHA3_224Context *ctx = PORT_New(SHA3_224Context);
    ctx->st = Hacl_Streaming_Keccak_malloc(Spec_Hash_Definitions_SHA3_224);
    return ctx;
}

SHA3_256Context *
SHA3_256_NewContext()
{
    SHA3_256Context *ctx = PORT_New(SHA3_256Context);
    ctx->st = Hacl_Streaming_Keccak_malloc(Spec_Hash_Definitions_SHA3_256);
    return ctx;
}

SHA3_384Context *
SHA3_384_NewContext()
{
    SHA3_384Context *ctx = PORT_New(SHA3_384Context);
    ctx->st = Hacl_Streaming_Keccak_malloc(Spec_Hash_Definitions_SHA3_384);
    return ctx;
}

SHA3_512Context *
SHA3_512_NewContext()
{
    SHA3_512Context *ctx = PORT_New(SHA3_512Context);
    ctx->st = Hacl_Streaming_Keccak_malloc(Spec_Hash_Definitions_SHA3_512);
    return ctx;
}

void
SHA3_224_DestroyContext(SHA3_224Context *ctx, PRBool freeit)
{
    Hacl_Streaming_Keccak_reset(ctx->st);
    if (freeit) {
        Hacl_Streaming_Keccak_free(ctx->st);
        PORT_Free(ctx);
    }
}

void
SHA3_256_DestroyContext(SHA3_256Context *ctx, PRBool freeit)
{
    Hacl_Streaming_Keccak_reset(ctx->st);
    if (freeit) {
        Hacl_Streaming_Keccak_free(ctx->st);
        PORT_Free(ctx);
    }
}

void
SHA3_384_DestroyContext(SHA3_384Context *ctx, PRBool freeit)
{
    Hacl_Streaming_Keccak_reset(ctx->st);
    if (freeit) {
        Hacl_Streaming_Keccak_free(ctx->st);
        PORT_Free(ctx);
    }
}

void
SHA3_512_DestroyContext(SHA3_512Context *ctx, PRBool freeit)
{
    Hacl_Streaming_Keccak_reset(ctx->st);
    if (freeit) {
        Hacl_Streaming_Keccak_free(ctx->st);
        PORT_Free(ctx);
    }
}

unsigned int
SHA3_224_FlattenSize(SHA3_224Context *ctx)
{
    return 0;
}

unsigned int
SHA3_256_FlattenSize(SHA3_256Context *ctx)
{
    return 0;
}

unsigned int
SHA3_384_FlattenSize(SHA3_384Context *ctx)
{
    return 0;
}

unsigned int
SHA3_512_FlattenSize(SHA3_512Context *ctx)
{
    return 0;
}

void
SHA3_224_Begin(SHA3_224Context *ctx)
{
    Hacl_Streaming_Keccak_reset(ctx->st);
}

void
SHA3_256_Begin(SHA3_256Context *ctx)
{
    Hacl_Streaming_Keccak_reset(ctx->st);
}

void
SHA3_384_Begin(SHA3_384Context *ctx)
{
    Hacl_Streaming_Keccak_reset(ctx->st);
}

void
SHA3_512_Begin(SHA3_512Context *ctx)
{
    Hacl_Streaming_Keccak_reset(ctx->st);
}

void
SHA3_224_Update(SHA3_224Context *ctx, const unsigned char *input,
                unsigned int inputLen)
{
    Hacl_Streaming_Keccak_update(ctx->st, (uint8_t *)input, inputLen);
}

void
SHA3_256_Update(SHA3_256Context *ctx, const unsigned char *input,
                unsigned int inputLen)
{
    Hacl_Streaming_Keccak_update(ctx->st, (uint8_t *)input, inputLen);
}

void
SHA3_384_Update(SHA3_384Context *ctx, const unsigned char *input,
                unsigned int inputLen)
{
    Hacl_Streaming_Keccak_update(ctx->st, (uint8_t *)input, inputLen);
}

void
SHA3_512_Update(SHA3_512Context *ctx, const unsigned char *input,
                unsigned int inputLen)
{
    Hacl_Streaming_Keccak_update(ctx->st, (uint8_t *)input, inputLen);
}

void
SHA3_224_End(SHA3_224Context *ctx, unsigned char *digest,
             unsigned int *digestLen, unsigned int maxDigestLen)
{
    uint8_t sha3_digest[SHA3_224_LENGTH] = { 0 };
    Hacl_Streaming_Keccak_finish(ctx->st, sha3_digest);

    unsigned int len = PR_MIN(SHA3_224_LENGTH, maxDigestLen);
    memcpy(digest, sha3_digest, len);
    if (digestLen)
        *digestLen = len;
}

void
SHA3_256_End(SHA3_256Context *ctx, unsigned char *digest,
             unsigned int *digestLen, unsigned int maxDigestLen)
{
    uint8_t sha3_digest[SHA3_256_LENGTH] = { 0 };
    Hacl_Streaming_Keccak_finish(ctx->st, sha3_digest);

    unsigned int len = PR_MIN(SHA3_256_LENGTH, maxDigestLen);
    memcpy(digest, sha3_digest, len);
    if (digestLen)
        *digestLen = len;
}

void
SHA3_384_End(SHA3_384Context *ctx, unsigned char *digest,
             unsigned int *digestLen, unsigned int maxDigestLen)
{
    uint8_t sha3_digest[SHA3_384_LENGTH] = { 0 };
    Hacl_Streaming_Keccak_finish(ctx->st, sha3_digest);

    unsigned int len = PR_MIN(SHA3_384_LENGTH, maxDigestLen);
    memcpy(digest, sha3_digest, len);
    if (digestLen)
        *digestLen = len;
}

void
SHA3_512_End(SHA3_512Context *ctx, unsigned char *digest,
             unsigned int *digestLen, unsigned int maxDigestLen)
{
    uint8_t sha3_digest[SHA3_512_LENGTH] = { 0 };
    Hacl_Streaming_Keccak_finish(ctx->st, sha3_digest);

    unsigned int len = PR_MIN(SHA3_512_LENGTH, maxDigestLen);
    memcpy(digest, sha3_digest, len);
    if (digestLen)
        *digestLen = len;
}

SECStatus
SHA3_224_HashBuf(unsigned char *dest, const unsigned char *src,
                 PRUint32 src_length)
{
    SHA3_224Context *ctx = SHA3_224_NewContext();
    SHA3_224_Begin(ctx);
    SHA3_224_Update(ctx, src, src_length);
    SHA3_224_End(ctx, dest, NULL, SHA3_224_LENGTH);
    SHA3_224_DestroyContext(ctx, true);
    return SECSuccess;
}

SECStatus
SHA3_256_HashBuf(unsigned char *dest, const unsigned char *src,
                 PRUint32 src_length)
{
    SHA3_256Context *ctx = SHA3_256_NewContext();
    SHA3_256_Begin(ctx);
    SHA3_256_Update(ctx, src, src_length);
    SHA3_256_End(ctx, dest, NULL, SHA3_256_LENGTH);
    SHA3_256_DestroyContext(ctx, true);
    return SECSuccess;
}

SECStatus
SHA3_384_HashBuf(unsigned char *dest, const unsigned char *src,
                 PRUint32 src_length)
{
    SHA3_384Context *ctx = SHA3_384_NewContext();
    SHA3_384_Begin(ctx);
    SHA3_384_Update(ctx, src, src_length);
    SHA3_384_End(ctx, dest, NULL, SHA3_384_LENGTH);
    SHA3_384_DestroyContext(ctx, true);
    return SECSuccess;
}

SECStatus
SHA3_512_HashBuf(unsigned char *dest, const unsigned char *src,
                 PRUint32 src_length)
{
    SHA3_512Context *ctx = SHA3_512_NewContext();
    SHA3_512_Begin(ctx);
    SHA3_512_Update(ctx, src, src_length);
    SHA3_512_End(ctx, dest, NULL, SHA3_512_LENGTH);
    SHA3_512_DestroyContext(ctx, true);
    return SECSuccess;
}

SECStatus
SHA3_224_Hash(unsigned char *dest, const char *src)
{
    return SHA3_224_HashBuf(dest, (const unsigned char *)src, PORT_Strlen(src));
}

SECStatus
SHA3_256_Hash(unsigned char *dest, const char *src)
{
    return SHA3_256_HashBuf(dest, (const unsigned char *)src, PORT_Strlen(src));
}

SECStatus
SHA3_384_Hash(unsigned char *dest, const char *src)
{
    return SHA3_384_HashBuf(dest, (const unsigned char *)src, PORT_Strlen(src));
}

SECStatus
SHA3_512_Hash(unsigned char *dest, const char *src)
{
    return SHA3_512_HashBuf(dest, (const unsigned char *)src, PORT_Strlen(src));
}
