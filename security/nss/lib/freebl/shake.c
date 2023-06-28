
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

struct SHAKEContextStr {
    Hacl_Streaming_Keccak_state *st;
};

SHAKE_128Context *
SHAKE_128_NewContext()
{
    SHAKE_128Context *ctx = PORT_New(SHAKE_128Context);
    ctx->st = Hacl_Streaming_Keccak_malloc(Spec_Hash_Definitions_Shake128);
    return ctx;
}

SHAKE_256Context *
SHAKE_256_NewContext()
{
    SHAKE_256Context *ctx = PORT_New(SHAKE_256Context);
    ctx->st = Hacl_Streaming_Keccak_malloc(Spec_Hash_Definitions_Shake256);
    return ctx;
}

void
SHAKE_128_DestroyContext(SHAKE_128Context *ctx, PRBool freeit)
{
    Hacl_Streaming_Keccak_reset(ctx->st);
    if (freeit) {
        Hacl_Streaming_Keccak_free(ctx->st);
        PORT_Free(ctx);
    }
}

void
SHAKE_256_DestroyContext(SHAKE_256Context *ctx, PRBool freeit)
{
    Hacl_Streaming_Keccak_reset(ctx->st);
    if (freeit) {
        Hacl_Streaming_Keccak_free(ctx->st);
        PORT_Free(ctx);
    }
}

void
SHAKE_128_Begin(SHAKE_128Context *ctx)
{
    Hacl_Streaming_Keccak_reset(ctx->st);
}

void
SHAKE_256_Begin(SHAKE_256Context *ctx)
{
    Hacl_Streaming_Keccak_reset(ctx->st);
}

void
SHAKE_128_Absorb(SHAKE_128Context *ctx, const unsigned char *input,
                 unsigned int inputLen)
{
    Hacl_Streaming_Keccak_update(ctx->st, (uint8_t *)input, inputLen);
}

void
SHAKE_256_Absorb(SHAKE_256Context *ctx, const unsigned char *input,
                 unsigned int inputLen)
{
    Hacl_Streaming_Keccak_update(ctx->st, (uint8_t *)input, inputLen);
}

void
SHAKE_128_SqueezeEnd(SHAKE_128Context *ctx, unsigned char *digest,
                     unsigned int digestLen)
{
    Hacl_Streaming_Keccak_squeeze(ctx->st, digest, digestLen);
}

void
SHAKE_256_SqueezeEnd(SHAKE_256Context *ctx, unsigned char *digest,
                     unsigned int digestLen)
{
    Hacl_Streaming_Keccak_squeeze(ctx->st, digest, digestLen);
}

SECStatus
SHAKE_128_HashBuf(unsigned char *dest, PRUint32 dest_length,
                  const unsigned char *src, PRUint32 src_length)
{
    SHAKE_128Context *ctx = SHAKE_128_NewContext();
    SHAKE_128_Begin(ctx);
    SHAKE_128_Absorb(ctx, src, src_length);
    SHAKE_128_SqueezeEnd(ctx, dest, dest_length);
    SHAKE_128_DestroyContext(ctx, true);
    return SECSuccess;
}

SECStatus
SHAKE_256_HashBuf(unsigned char *dest, PRUint32 dest_length,
                  const unsigned char *src, PRUint32 src_length)
{
    SHAKE_256Context *ctx = SHAKE_256_NewContext();
    SHAKE_256_Begin(ctx);
    SHAKE_256_Absorb(ctx, src, src_length);
    SHAKE_256_SqueezeEnd(ctx, dest, dest_length);
    SHAKE_256_DestroyContext(ctx, true);
    return SECSuccess;
}

SECStatus
SHAKE_128_Hash(unsigned char *dest, unsigned int dest_length, const char *src)
{
    return SHAKE_128_HashBuf(dest, dest_length, (const unsigned char *)src, PORT_Strlen(src));
}

SECStatus
SHAKE_256_Hash(unsigned char *dest, unsigned int dest_length, const char *src)
{
    return SHAKE_256_HashBuf(dest, dest_length, (const unsigned char *)src, PORT_Strlen(src));
}
