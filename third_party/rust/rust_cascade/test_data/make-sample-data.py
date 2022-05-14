import filtercascade
import hashlib
from pathlib import Path

import sys
import logging

logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)


def predictable_serial_gen(start, end):
    counter = start
    while counter < end:
        counter += 1
        m = hashlib.sha256()
        m.update(counter.to_bytes(4, byteorder="big"))
        yield m.hexdigest()


def store(fc, path):
    if path.exists():
        path.unlink()
    with open(path, "wb") as f:
        fc.tofile(f)


small_set = list(set(predictable_serial_gen(0, 500)))
large_set = set(predictable_serial_gen(500, 10_000))

# filter parameters
growth_factor = 1.0
min_filter_length = 177  # 177 * 1.44 ~ 256, so smallest filter will have 256 bits

print("--- v2_sha256l32_with_salt ---")
v2_sha256l32_with_salt = filtercascade.FilterCascade(
    [],
    defaultHashAlg=filtercascade.fileformats.HashAlgorithm.SHA256,
    salt=b"nacl",
    growth_factor=growth_factor,
    min_filter_length=min_filter_length,
)
v2_sha256l32_with_salt.initialize(
    include=[b"this", b"that"] + small_set, exclude=large_set | set([b"other"])
)
store(v2_sha256l32_with_salt, Path("test_v2_sha256l32_salt_mlbf"))

print("--- v2_sha256l32 ---")
v2_sha256l32 = filtercascade.FilterCascade(
    [],
    defaultHashAlg=filtercascade.fileformats.HashAlgorithm.SHA256,
    growth_factor=growth_factor,
    min_filter_length=min_filter_length,
)
v2_sha256l32.initialize(
    include=[b"this", b"that"] + small_set, exclude=large_set | set([b"other"])
)
store(v2_sha256l32, Path("test_v2_sha256l32_mlbf"))

print("--- v2_murmur ---")
v2_murmur = filtercascade.FilterCascade(
    [],
    defaultHashAlg=filtercascade.fileformats.HashAlgorithm.MURMUR3,
    growth_factor=growth_factor,
    min_filter_length=min_filter_length,
)
v2_murmur.initialize(
    include=[b"this", b"that"] + small_set, exclude=large_set | set([b"other"])
)
store(v2_murmur, Path("test_v2_murmur_mlbf"))

print("--- v2_murmur_inverted ---")
v2_murmur_inverted = filtercascade.FilterCascade(
    [],
    defaultHashAlg=filtercascade.fileformats.HashAlgorithm.MURMUR3,
    growth_factor=growth_factor,
    min_filter_length=min_filter_length,
)
v2_murmur_inverted.initialize(
    include=large_set | set([b"this", b"that"]), exclude=[b"other"] + small_set
)
store(v2_murmur_inverted, Path("test_v2_murmur_inverted_mlbf"))

print("--- v2_sha256l32_inverted ---")
v2_sha256l32_inverted = filtercascade.FilterCascade(
    [],
    defaultHashAlg=filtercascade.fileformats.HashAlgorithm.SHA256,
    growth_factor=growth_factor,
    min_filter_length=min_filter_length,
)
v2_sha256l32_inverted.initialize(
    include=large_set | set([b"this", b"that"]), exclude=[b"other"] + small_set
)
store(v2_sha256l32_inverted, Path("test_v2_sha256l32_inverted_mlbf"))

print("--- v2_sha256ctr_with_salt ---")
v2_sha256ctr_with_salt = filtercascade.FilterCascade(
    [],
    defaultHashAlg=filtercascade.fileformats.HashAlgorithm.SHA256CTR,
    salt=b"nacl",
    growth_factor=growth_factor,
    min_filter_length=min_filter_length,
)
v2_sha256ctr_with_salt.initialize(
    include=[b"this", b"that"] + small_set, exclude=large_set | set([b"other"])
)
store(v2_sha256ctr_with_salt, Path("test_v2_sha256ctr_salt_mlbf"))
