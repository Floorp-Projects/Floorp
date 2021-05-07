#!/usr/bin/env python3
# This script converts the test vectors referenced by the specification into
# a form that matches our implementation.

import json
import sys


def pkcs8(sk, pk):
    print(
        f'"3067020100301406072a8648ce3d020106092b06010401da470f01044c304a0201010420{sk}a123032100{pk}",'
    )


i = 0
for tc in json.load(sys.stdin):
    # Only mode_base and mode_psk
    if tc["mode"] != 0 and tc["mode"] != 1:
        continue
    # X25519
    if tc["kem_id"] != 32:
        continue
    # SHA-2 256, 384, and 512 (1..3)
    if not tc["kdf_id"] in [1, 2, 3]:
        continue
    # AES-128-GCM, AES-256-GCM, and ChaCha20Poly1305 (1..3 also)
    if not tc["aead_id"] in [1, 2, 3]:
        continue

    print(f"{{{i},")
    print(f"static_cast<HpkeModeId>({tc['mode']}),")
    print(f"static_cast<HpkeKemId>({tc['kem_id']}),")
    print(f"static_cast<HpkeKdfId>({tc['kdf_id']}),")
    print(f"static_cast<HpkeAeadId>({tc['aead_id']}),")
    print(f'"{tc["info"]}", // info')
    pkcs8(tc["skEm"], tc["pkEm"])
    pkcs8(tc["skRm"], tc["pkRm"])
    print(f'"{tc.get("psk", "")}", // psk')
    print(f'"{tc.get("psk_id", "")}", // psk_id')
    print(f'"{tc["enc"]}", // enc')
    print(f'"{tc["key"]}", // key')
    print(f'"{tc["base_nonce"]}", // nonce')

    print("{ // Encryptions")
    for e in tc["encryptions"]:
        print("{")
        print(f'"{e["plaintext"]}", // pt')
        print(f'"{e["aad"]}", // aad')
        print(f'"{e["ciphertext"]}", // ct')
        print("},")
    print("},")

    print("{ // Exports")
    for e in tc["exports"]:
        print("{")
        print(f'"{e["exporter_context"]}", // context')
        print(f'{e["L"]}, // len')
        print(f'"{e["exported_value"]}", // exported')
        print("},")
    print("},")
    print("},")
    i = i + 1
