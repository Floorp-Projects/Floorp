/* Copyright 2016-2017 INRIA and Microsoft Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

module Spec.CTR

module ST = FStar.HyperStack.ST

open FStar.Mul
open FStar.Seq
open Spec.Lib

#reset-options "--initial_fuel 0 --max_fuel 0 --initial_ifuel 0 --max_ifuel 0"

type block_cipher_ctx = {
     keylen: nat ;
     blocklen: (x:nat{x>0});
     noncelen: nat;
     counterbits: nat;
     incr: pos}

type key (c:block_cipher_ctx) = lbytes c.keylen
type nonce (c:block_cipher_ctx) = lbytes c.noncelen
type block (c:block_cipher_ctx) = lbytes (c.blocklen*c.incr)
type counter (c:block_cipher_ctx) = UInt.uint_t c.counterbits
type block_cipher (c:block_cipher_ctx) =  key c -> nonce c -> counter c -> block c

val xor: #len:nat -> x:lbytes len -> y:lbytes len -> Tot (lbytes len)
let xor #len x y = map2 FStar.UInt8.(fun x y -> x ^^ y) x y


val counter_mode_blocks:
  ctx: block_cipher_ctx ->
  bc: block_cipher ctx ->
  k:key ctx -> n:nonce ctx -> c:counter ctx ->
  plain:seq UInt8.t{c + ctx.incr * (length plain / ctx.blocklen) < pow2 ctx.counterbits /\
    length plain % (ctx.blocklen * ctx.incr) = 0} ->
  Tot (lbytes (length plain))
      (decreases (length plain))
#reset-options "--z3rlimit 200 --max_fuel 0"
let rec counter_mode_blocks ctx block_enc key nonce counter plain =
  let len  = length plain in
  let len' = len / (ctx.blocklen * ctx.incr) in
  Math.Lemmas.lemma_div_mod len (ctx.blocklen * ctx.incr) ;
  if len = 0 then Seq.createEmpty #UInt8.t
  else (
    let prefix, block = split plain (len - ctx.blocklen * ctx.incr) in    
      (* TODO: move to a single lemma for clarify *)
      Math.Lemmas.lemma_mod_plus (length prefix) 1 (ctx.blocklen * ctx.incr);
      Math.Lemmas.lemma_div_le (length prefix) len ctx.blocklen;
      Spec.CTR.Lemmas.lemma_div len (ctx.blocklen * ctx.incr);
      (* End TODO *)
    let cipher        = counter_mode_blocks ctx block_enc key nonce counter prefix in
    let mask          = block_enc key nonce (counter + (len / ctx.blocklen - 1) * ctx.incr) in
    let eb            = xor block mask in
    cipher @| eb
  )


val counter_mode:
  ctx: block_cipher_ctx ->
  bc: block_cipher ctx ->
  k:key ctx -> n:nonce ctx -> c:counter ctx ->
  plain:seq UInt8.t{c + ctx.incr * (length plain / ctx.blocklen) < pow2 ctx.counterbits} ->
  Tot (lbytes (length plain))
      (decreases (length plain))
#reset-options "--z3rlimit 200 --max_fuel 0"
let counter_mode ctx block_enc key nonce counter plain =
  let len = length plain in
  let blocks_len = (ctx.incr * ctx.blocklen) * (len / (ctx.blocklen * ctx.incr)) in
  let part_len   = len % (ctx.blocklen * ctx.incr) in
    (* TODO: move to a single lemma for clarify *)
    Math.Lemmas.lemma_div_mod len (ctx.blocklen * ctx.incr);
    Math.Lemmas.multiple_modulo_lemma (len / (ctx.blocklen * ctx.incr)) (ctx.blocklen * ctx.incr);
    Math.Lemmas.lemma_div_le (blocks_len) len ctx.blocklen;
    (* End TODO *)
  let blocks, last_block = split plain blocks_len in
  let cipher_blocks = counter_mode_blocks ctx block_enc key nonce counter blocks in
  let cipher_last_block =
    if part_len > 0
    then (* encrypt final partial block(s) *)
      let mask = block_enc key nonce (counter+ctx.incr*(length plain / ctx.blocklen)) in
      let mask = slice mask 0 part_len in
      assert(length last_block = part_len);
      xor #part_len last_block mask
    else createEmpty in
  cipher_blocks @| cipher_last_block
