#define CHECK_COUNT(state)                                                    \
    ((state)->pool->count > (state)->pool->allocated ?                        \
        ((state)->mode == XPTXDR_ENCODE ? XPT_GrowPool((state)->pool) :    \
         PR_FALSE) : PR_TRUE)

PRBool
XPT_GrowPool(XPTXDRDatapool *pool)
{
    char *newdata = realloc(pool->data, pool->allocated + XPT_GROW_CHUNK);
    if (!newdata)
	return PR_FALSE;
    pool->data = newdata;
    pool->allocated += XPT_GROW_CHUNK;
    return PR_TRUE;
}

PRBool
XPT_DoString(XPTXDRState *state, XPTString **strp)
{
    return PR_FALSE;
}

PRBool
XPT_DoIdentifier(XPTXDRState *state, char **identp)
{
    return PR_FALSE;
}

PRBool
XPT_Do32(XPTXDRState *state, uint32 *u32p)
{
    return PR_FALSE;
}

PRBool
XPT_Do16(XPTXDRState *state, uint16 *u16p)
{
    return PR_FALSE;
}

PRBool
XPT_Do8(XPTXDRState *state, uint8 *u8p)
{
    return PR_FALSE;
}

PRBool
XPT_GetOffsetForAddr(XPTXDRState *state, void *addr, uint32 *offsetp)
{
    *offsetp = 0;
    return PR_FALSE;
}

PRBool
XPT_GetAddrForOffset(XPTXDRState *state, uint32 offset, void **addr)
{
    *addr = NULL;
    return PR_FALSE;
}

static PRBool
do_bit(XPTXDRState *state, uint8 *u8p, int bitno)
{
    int bit_value, delta, new_value;
    XPTXDRDatapool *pool = state->pool;

    if (state->mode == XPTXDR_ENCODE) {
	bit_value = (*u8p & 1) << (bitno);   /* 7 = 0100 0000, 6 = 0010 0000 */
	if (bit_value) {
	    delta = pool->bit + (bitno) - 7;
	    new_value = delta >= 0 ? bit_value >> delta : bit_value << -delta;
	    pool->data[pool->count] |= new_value;
	}
    } else {
	bit_value = pool->data[pool->count] & (1 << (7 - pool->bit));
	*u2p = bit_value >> (7 - pool->bit);
    }
    if (++pool->bit == 8) {
	pool->count++;
	pool->bit = 0;
    }

    return CHECK_COUNT(state);
}

int
XPT_DoBits(XPTXDRState *state, uint8 *u8p, uintN nbits)
{

#define DO_BIT(state, u8p, nbits)					      \
    if (!do_bit(state, u8p, nbits))					      \
       return PR_FALSE;

    switch(nbits) {
      case 7:
	DO_BIT(state, u8p, 7);
      case 6:
	DO_BIT(state, u8p, 6);
      case 5:
	DO_BIT(state, u8p, 5);
      case 4:
	DO_BIT(state, u8p, 4);
      case 3:
	DO_BIT(state, u8p, 3);
      case 2:
	DO_BIT(state, u8p, 2);
      case 1:
	DO_BIT(state, u8p, 1);
      default:;
    };

#undef DO_BIT

    return PR_TRUE;
}

int
XPT_FlushBits(XPTXDRState *state)
{
    int skipped = 8 - state->pool->bits;

    state->pool->bits = 0;
    state->count++;

    if (!CHECK_COUNT(state))
	return -1;

    return skipped == 8 ? 0 : skipped;
}
