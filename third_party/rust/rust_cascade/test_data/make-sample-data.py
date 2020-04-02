import filtercascade
import hashlib
from pathlib import Path


def predictable_serial_gen(end):
    counter = 0
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


large_set = set(predictable_serial_gen(100_000))

v2_sha256_with_salt = filtercascade.FilterCascade(
    [], defaultHashAlg=filtercascade.fileformats.HashAlgorithm.SHA256, salt=b"nacl"
)
v2_sha256_with_salt.initialize(
    include=[b"this", b"that"], exclude=large_set | set([b"other"])
)
store(v2_sha256_with_salt, Path("test_v2_sha256_salt_mlbf"))

v2_sha256 = filtercascade.FilterCascade(
    [], defaultHashAlg=filtercascade.fileformats.HashAlgorithm.SHA256
)
v2_sha256.initialize(include=[b"this", b"that"], exclude=large_set | set([b"other"]))
store(v2_sha256, Path("test_v2_sha256_mlbf"))

v2_murmur = filtercascade.FilterCascade(
    [], defaultHashAlg=filtercascade.fileformats.HashAlgorithm.MURMUR3
)
v2_murmur.initialize(include=[b"this", b"that"], exclude=large_set | set([b"other"]))
store(v2_murmur, Path("test_v2_murmur_mlbf"))

v2_murmur_inverted = filtercascade.FilterCascade(
    [], defaultHashAlg=filtercascade.fileformats.HashAlgorithm.MURMUR3
)
v2_murmur_inverted.initialize(
    include=large_set | set([b"this", b"that"]), exclude=[b"other"]
)
store(v2_murmur_inverted, Path("test_v2_murmur_inverted_mlbf"))


v2_sha256_inverted = filtercascade.FilterCascade(
    [], defaultHashAlg=filtercascade.fileformats.HashAlgorithm.SHA256
)
v2_sha256_inverted.initialize(
    include=large_set | set([b"this", b"that"]), exclude=[b"other"]
)
store(v2_sha256_inverted, Path("test_v2_sha256_inverted_mlbf"))
