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

module Spec.Poly1305

module ST = FStar.HyperStack.ST

open FStar.Math.Lib
open FStar.Mul
open FStar.Seq
open FStar.UInt8
open FStar.Endianness
open Spec.Poly1305.Lemmas

#set-options "--initial_fuel 0 --max_fuel 0 --initial_ifuel 0 --max_ifuel 0"

(* Field types and parameters *)
let prime = pow2 130 - 5
type elem = e:int{e >= 0 /\ e < prime}
let fadd (e1:elem) (e2:elem) = (e1 + e2) % prime
let fmul (e1:elem) (e2:elem) = (e1 * e2) % prime
let zero : elem = 0
let one  : elem = 1
let op_Plus_At = fadd
let op_Star_At = fmul
(* Type aliases *)
let op_Amp_Bar = UInt.logand #128
type word = w:bytes{length w <= 16}
type word_16 = w:bytes{length w = 16}
type tag = word_16
type key = lbytes 32
type text = seq word

(* Specification code *)
let encode (w:word) =
  (pow2 (8 * length w)) `fadd` (little_endian w)

let rec poly (txt:text) (r:e:elem) : Tot elem (decreases (length txt)) =
  if length txt = 0 then zero
  else
    let a = poly (Seq.tail txt) r in
    let n = encode (Seq.head txt) in
    (n `fadd` a) `fmul` r

let encode_r (rb:word_16) =
  (little_endian rb) &| 0x0ffffffc0ffffffc0ffffffc0fffffff

let finish (a:elem) (s:word_16) : Tot tag =
  let n = (a + little_endian s) % pow2 128 in
  little_bytes 16ul n

let rec encode_bytes (txt:bytes) : Tot text (decreases (length txt)) =
  if length txt = 0 then createEmpty
  else
    let w, txt = split txt (min (length txt) 16) in
    append_last (encode_bytes txt) w

let poly1305 (msg:bytes) (k:key) : Tot tag =
  let text = encode_bytes msg in
  let r = encode_r (slice k 0 16) in
  let s = slice k 16 32 in
  finish (poly text r) s


(* ********************* *)
(* RFC 7539 Test Vectors *)
(* ********************* *)

#reset-options "--initial_fuel 0 --max_fuel 0 --z3rlimit 20"

unfold let msg = [
  0x43uy; 0x72uy; 0x79uy; 0x70uy; 0x74uy; 0x6fuy; 0x67uy; 0x72uy;
  0x61uy; 0x70uy; 0x68uy; 0x69uy; 0x63uy; 0x20uy; 0x46uy; 0x6fuy;
  0x72uy; 0x75uy; 0x6duy; 0x20uy; 0x52uy; 0x65uy; 0x73uy; 0x65uy;
  0x61uy; 0x72uy; 0x63uy; 0x68uy; 0x20uy; 0x47uy; 0x72uy; 0x6fuy;
  0x75uy; 0x70uy ]

unfold let k = [
  0x85uy; 0xd6uy; 0xbeuy; 0x78uy; 0x57uy; 0x55uy; 0x6duy; 0x33uy;
  0x7fuy; 0x44uy; 0x52uy; 0xfeuy; 0x42uy; 0xd5uy; 0x06uy; 0xa8uy;
  0x01uy; 0x03uy; 0x80uy; 0x8auy; 0xfbuy; 0x0duy; 0xb2uy; 0xfduy;
  0x4auy; 0xbfuy; 0xf6uy; 0xafuy; 0x41uy; 0x49uy; 0xf5uy; 0x1buy ]

unfold let expected = [
  0xa8uy; 0x06uy; 0x1duy; 0xc1uy; 0x30uy; 0x51uy; 0x36uy; 0xc6uy;
  0xc2uy; 0x2buy; 0x8buy; 0xafuy; 0x0cuy; 0x01uy; 0x27uy; 0xa9uy ]

let test () : Tot bool =
  assert_norm(List.Tot.length msg      = 34);
  assert_norm(List.Tot.length k        = 32);
  assert_norm(List.Tot.length expected = 16);
  let msg      = createL msg in
  let k        = createL k   in
  let expected = createL expected in
  poly1305 msg k = expected
