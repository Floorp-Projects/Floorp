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

module Spec.Curve25519

module ST = FStar.HyperStack.ST

open FStar.Mul
open FStar.Seq
open FStar.UInt8
open FStar.Endianness
open Spec.Lib
open Spec.Curve25519.Lemmas

#reset-options "--initial_fuel 0 --max_fuel 0 --z3rlimit 20"

(* Field types and parameters *)
let prime = pow2 255 - 19
type elem : Type0 = e:int{e >= 0 /\ e < prime}
let fadd e1 e2 = (e1 + e2) % prime
let fsub e1 e2 = (e1 - e2) % prime
let fmul e1 e2 = (e1 * e2) % prime
let zero : elem = 0
let one  : elem = 1
let ( +@ ) = fadd
let ( *@ ) = fmul

(** Exponentiation *)
let rec ( ** ) (e:elem) (n:pos) : Tot elem (decreases n) =
  if n = 1 then e
  else
    if n % 2 = 0 then op_Star_Star (e `fmul` e) (n / 2)
    else e `fmul` (op_Star_Star (e `fmul` e) ((n-1)/2))

(* Type aliases *)
type scalar = lbytes 32
type serialized_point = lbytes 32
type proj_point = | Proj: x:elem -> z:elem -> proj_point

let decodeScalar25519 (k:scalar) =
  let k   = k.[0] <- (k.[0] &^ 248uy)          in
  let k   = k.[31] <- ((k.[31] &^ 127uy) |^ 64uy) in k

let decodePoint (u:serialized_point) =
  (little_endian u % pow2 255) % prime

let add_and_double qx nq nqp1 =
  let x_1 = qx in
  let x_2, z_2 = nq.x, nq.z in
  let x_3, z_3 = nqp1.x, nqp1.z in
  let a  = x_2 `fadd` z_2 in
  let aa = a**2 in
  let b  = x_2 `fsub` z_2 in
  let bb = b**2 in
  let e = aa `fsub` bb in
  let c = x_3 `fadd` z_3 in
  let d = x_3 `fsub` z_3 in
  let da = d `fmul` a in
  let cb = c `fmul` b in
  let x_3 = (da `fadd` cb)**2 in
  let z_3 = x_1 `fmul` ((da `fsub` cb)**2) in
  let x_2 = aa `fmul` bb in
  let z_2 = e `fmul` (aa `fadd` (121665 `fmul` e)) in
  Proj x_2 z_2, Proj x_3 z_3

let ith_bit (k:scalar) (i:nat{i < 256}) =
  let q = i / 8 in let r = i % 8 in
  (v (k.[q]) / pow2 r) % 2

let rec montgomery_ladder_ (init:elem) x xp1 (k:scalar) (ctr:nat{ctr<=256})
  : Tot proj_point (decreases ctr) =
  if ctr = 0 then x
  else (
    let ctr' = ctr - 1 in
    let (x', xp1') =
      if ith_bit k ctr' = 1 then (
        let nqp2, nqp1 = add_and_double init xp1 x in
        nqp1, nqp2
      ) else add_and_double init x xp1 in
    montgomery_ladder_ init x' xp1' k ctr'
  )

let montgomery_ladder (init:elem) (k:scalar) : Tot proj_point =
  montgomery_ladder_ init (Proj one zero) (Proj init one) k 256

let encodePoint (p:proj_point) : Tot serialized_point =
  let p = p.x `fmul` (p.z ** (prime - 2)) in
  little_bytes 32ul p

let scalarmult (k:scalar) (u:serialized_point) : Tot serialized_point =
  let k = decodeScalar25519 k in
  let u = decodePoint u in
  let res = montgomery_ladder u k in
  encodePoint res


(* ********************* *)
(* RFC 7748 Test Vectors *)
(* ********************* *)

let scalar1 = [
  0xa5uy; 0x46uy; 0xe3uy; 0x6buy; 0xf0uy; 0x52uy; 0x7cuy; 0x9duy;
  0x3buy; 0x16uy; 0x15uy; 0x4buy; 0x82uy; 0x46uy; 0x5euy; 0xdduy;
  0x62uy; 0x14uy; 0x4cuy; 0x0auy; 0xc1uy; 0xfcuy; 0x5auy; 0x18uy;
  0x50uy; 0x6auy; 0x22uy; 0x44uy; 0xbauy; 0x44uy; 0x9auy; 0xc4uy
]

let scalar2 = [
  0x4buy; 0x66uy; 0xe9uy; 0xd4uy; 0xd1uy; 0xb4uy; 0x67uy; 0x3cuy;
  0x5auy; 0xd2uy; 0x26uy; 0x91uy; 0x95uy; 0x7duy; 0x6auy; 0xf5uy;
  0xc1uy; 0x1buy; 0x64uy; 0x21uy; 0xe0uy; 0xeauy; 0x01uy; 0xd4uy;
  0x2cuy; 0xa4uy; 0x16uy; 0x9euy; 0x79uy; 0x18uy; 0xbauy; 0x0duy
]

let input1 = [
  0xe6uy; 0xdbuy; 0x68uy; 0x67uy; 0x58uy; 0x30uy; 0x30uy; 0xdbuy;
  0x35uy; 0x94uy; 0xc1uy; 0xa4uy; 0x24uy; 0xb1uy; 0x5fuy; 0x7cuy;
  0x72uy; 0x66uy; 0x24uy; 0xecuy; 0x26uy; 0xb3uy; 0x35uy; 0x3buy;
  0x10uy; 0xa9uy; 0x03uy; 0xa6uy; 0xd0uy; 0xabuy; 0x1cuy; 0x4cuy
]

let input2 = [
  0xe5uy; 0x21uy; 0x0fuy; 0x12uy; 0x78uy; 0x68uy; 0x11uy; 0xd3uy;
  0xf4uy; 0xb7uy; 0x95uy; 0x9duy; 0x05uy; 0x38uy; 0xaeuy; 0x2cuy;
  0x31uy; 0xdbuy; 0xe7uy; 0x10uy; 0x6fuy; 0xc0uy; 0x3cuy; 0x3euy;
  0xfcuy; 0x4cuy; 0xd5uy; 0x49uy; 0xc7uy; 0x15uy; 0xa4uy; 0x93uy
]

let expected1 = [
  0xc3uy; 0xdauy; 0x55uy; 0x37uy; 0x9duy; 0xe9uy; 0xc6uy; 0x90uy;
  0x8euy; 0x94uy; 0xeauy; 0x4duy; 0xf2uy; 0x8duy; 0x08uy; 0x4fuy;
  0x32uy; 0xecuy; 0xcfuy; 0x03uy; 0x49uy; 0x1cuy; 0x71uy; 0xf7uy;
  0x54uy; 0xb4uy; 0x07uy; 0x55uy; 0x77uy; 0xa2uy; 0x85uy; 0x52uy
]
let expected2 = [
  0x95uy; 0xcbuy; 0xdeuy; 0x94uy; 0x76uy; 0xe8uy; 0x90uy; 0x7duy;
  0x7auy; 0xaduy; 0xe4uy; 0x5cuy; 0xb4uy; 0xb8uy; 0x73uy; 0xf8uy;
  0x8buy; 0x59uy; 0x5auy; 0x68uy; 0x79uy; 0x9fuy; 0xa1uy; 0x52uy;
  0xe6uy; 0xf8uy; 0xf7uy; 0x64uy; 0x7auy; 0xacuy; 0x79uy; 0x57uy
]

let test () =
  assert_norm(List.Tot.length scalar1 = 32);
  assert_norm(List.Tot.length scalar2 = 32);
  assert_norm(List.Tot.length input1 = 32);
  assert_norm(List.Tot.length input2 = 32);
  assert_norm(List.Tot.length expected1 = 32);
  assert_norm(List.Tot.length expected2 = 32);
  let scalar1 = createL scalar1 in
  let scalar2 = createL scalar2 in
  let input1 = createL input1 in
  let input2 = createL input2 in
  let expected1 = createL expected1 in
  let expected2 = createL expected2 in
  scalarmult scalar1 input1 = expected1
  && scalarmult scalar2 input2 = expected2
