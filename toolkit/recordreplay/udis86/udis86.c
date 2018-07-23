/* udis86 - libudis86/udis86.c
 *
 * Copyright (c) 2002-2013 Vivek Thampi
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright notice, 
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice, 
 *       this list of conditions and the following disclaimer in the documentation 
 *       and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR 
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "udint.h"
#include "decode.h"

static void ud_inp_init(struct ud *u);




/* =============================================================================
 * ud_set_mode() - Set Disassemly Mode.
 * =============================================================================
 */
extern void 
ud_set_mode(struct ud* u, uint8_t m)
{
  switch(m) {
  case 16:
  case 32:
  case 64: u->dis_mode = m ; return;
  default: u->dis_mode = 16; return;
  }
}



/* =============================================================================
 * ud_init
 *    Initializes ud_t object.
 * =============================================================================
 */
extern void 
ud_init(struct ud* u)
{
  memset((void*)u, 0, sizeof(struct ud));
  ud_set_mode(u, 16);
  u->mnemonic = UD_Iinvalid;
}


/* =============================================================================
 * ud_insn_get_opr
 *    Return the operand struct representing the nth operand of
 *    the currently disassembled instruction. Returns NULL if
 *    there's no such operand.
 * =============================================================================
 */
const struct ud_operand*
ud_insn_opr(const struct ud *u, unsigned int n)
{
  if (n > 2 || u->operand[n].type == UD_NONE) {
    return NULL; 
  } else {
    return &u->operand[n];
  }
}






/* =============================================================================
 * ud_insn_mnemonic
 *    Return the current instruction mnemonic.
 * =============================================================================
 */
enum ud_mnemonic_code
ud_insn_mnemonic(const struct ud *u)
{
  return u->mnemonic;
}



/* 
 * ud_inp_init
 *    Initializes the input system.
 */
static void
ud_inp_init(struct ud *u)
{
  u->inp_buf       = NULL;
  u->inp_buf_size  = 0;
  u->inp_buf_index = 0;
  u->inp_curr      = 0;
  u->inp_ctr       = 0;
  u->inp_end       = 0;
}


/* =============================================================================
 * ud_inp_set_buffer
 *    Set buffer as input.
 * =============================================================================
 */
void 
ud_set_input_buffer(struct ud* u, const uint8_t* buf, size_t len)
{
  ud_inp_init(u);
  u->inp_buf = buf;
  u->inp_buf_size = len;
  u->inp_buf_index = 0;
}

/* vim:set ts=2 sw=2 expandtab */
