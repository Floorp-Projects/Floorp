#include "blapi.h"
#include "../pqg.h"

static inline void
sha512_pre_msg(uint8_t *hash, uint8_t *prefix, uint32_t len, uint8_t *input)
{
    SHA512Context *ctx = SHA512_NewContext();
    uint32_t l = SHA512_LENGTH;
    SHA512_Begin(ctx);
    SHA512_Update(ctx, prefix, 32);
    SHA512_Update(ctx, input, len);
    SHA512_End(ctx, hash, &l, SHA512_LENGTH);
    SHA512_DestroyContext(ctx, PR_TRUE);
}

static inline void
sha512_pre_pre2_msg(
    uint8_t *hash,
    uint8_t *prefix,
    uint8_t *prefix2,
    uint32_t len,
    uint8_t *input)
{
    SHA512Context *ctx = SHA512_NewContext();
    uint32_t l = SHA512_LENGTH;
    SHA512_Begin(ctx);
    SHA512_Update(ctx, prefix, 32);
    SHA512_Update(ctx, prefix2, 32);
    SHA512_Update(ctx, input, len);
    SHA512_End(ctx, hash, &l, SHA512_LENGTH);
    SHA512_DestroyContext(ctx, PR_TRUE);
}

static void
Hacl_Streaming_SHA2_hash_512(uint8_t *secret, uint32_t len, uint8_t *expanded)
{
    SHA512_HashBuf(expanded, secret, len);
}