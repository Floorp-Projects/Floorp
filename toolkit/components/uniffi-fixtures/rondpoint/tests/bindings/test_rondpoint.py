import ctypes
import sys

from rondpoint import (
    Dictionnaire,
    Enumeration,
    EnumerationAvecDonnees,
    Retourneur,
    Stringifier,
    copie_carte,
    copie_dictionnaire,
    copie_enumeration,
    copie_enumerations,
    switcheroo,
)

dico = Dictionnaire(
    un=Enumeration.DEUX, deux=True, petit_nombre=0, gros_nombre=123456789
)
copyDico = copie_dictionnaire(dico)
assert dico == copyDico

assert copie_enumeration(Enumeration.DEUX) == Enumeration.DEUX
assert copie_enumerations([Enumeration.UN, Enumeration.DEUX]) == [
    Enumeration.UN,
    Enumeration.DEUX,
]
assert copie_carte(
    {
        "0": EnumerationAvecDonnees.ZERO(),
        "1": EnumerationAvecDonnees.UN(1),
        "2": EnumerationAvecDonnees.DEUX(2, "deux"),
    }
) == {
    "0": EnumerationAvecDonnees.ZERO(),
    "1": EnumerationAvecDonnees.UN(1),
    "2": EnumerationAvecDonnees.DEUX(2, "deux"),
}

assert switcheroo(False) is True

assert EnumerationAvecDonnees.ZERO() != EnumerationAvecDonnees.UN(1)
assert EnumerationAvecDonnees.UN(1) == EnumerationAvecDonnees.UN(1)
assert EnumerationAvecDonnees.UN(1) != EnumerationAvecDonnees.UN(2)

# Test the roundtrip across the FFI.
# This shows that the values we send come back in exactly the same state as we sent them.
# i.e. it shows that lowering from python and lifting into rust is symmetrical with
#      lowering from rust and lifting into python.
rt = Retourneur()


def affirmAllerRetour(vals, identique):
    for v in vals:
        id_v = identique(v)
        assert id_v == v, f"Round-trip failure: {v} => {id_v}"


MIN_I8 = -1 * 2**7
MAX_I8 = 2**7 - 1
MIN_I16 = -1 * 2**15
MAX_I16 = 2**15 - 1
MIN_I32 = -1 * 2**31
MAX_I32 = 2**31 - 1
MIN_I64 = -1 * 2**31
MAX_I64 = 2**31 - 1

# Python floats are always doubles, so won't round-trip through f32 correctly.
# This truncates them appropriately.
F32_ONE_THIRD = ctypes.c_float(1.0 / 3).value

# Booleans
affirmAllerRetour([True, False], rt.identique_boolean)

# Bytes.
affirmAllerRetour([MIN_I8, -1, 0, 1, MAX_I8], rt.identique_i8)
affirmAllerRetour([0x00, 0x12, 0xFF], rt.identique_u8)

# Shorts
affirmAllerRetour([MIN_I16, -1, 0, 1, MAX_I16], rt.identique_i16)
affirmAllerRetour([0x0000, 0x1234, 0xFFFF], rt.identique_u16)

# Ints
affirmAllerRetour([MIN_I32, -1, 0, 1, MAX_I32], rt.identique_i32)
affirmAllerRetour([0x00000000, 0x12345678, 0xFFFFFFFF], rt.identique_u32)

# Longs
affirmAllerRetour([MIN_I64, -1, 0, 1, MAX_I64], rt.identique_i64)
affirmAllerRetour(
    [0x0000000000000000, 0x1234567890ABCDEF, 0xFFFFFFFFFFFFFFFF], rt.identique_u64
)

# Floats
affirmAllerRetour([0.0, 0.5, 0.25, 1.0, F32_ONE_THIRD], rt.identique_float)

# Doubles
affirmAllerRetour(
    [0.0, 0.5, 0.25, 1.0, 1.0 / 3, sys.float_info.max, sys.float_info.min],
    rt.identique_double,
)

# Strings
affirmAllerRetour(
    [
        "",
        "abc",
        "été",
        "ښي لاس ته لوستلو لوستل",
        "😻emoji 👨‍👧‍👦multi-emoji, 🇨🇭a flag, a canal, panama",
    ],
    rt.identique_string,
)

# Test one way across the FFI.
#
# We send one representation of a value to lib.rs, and it transforms it into another, a string.
# lib.rs sends the string back, and then we compare here in python.
#
# This shows that the values are transformed into strings the same way in both python and rust.
# i.e. if we assume that the string return works (we test this assumption elsewhere)
#      we show that lowering from python and lifting into rust has values that both python and rust
#      both stringify in the same way. i.e. the same values.
#
# If we roundtripping proves the symmetry of our lowering/lifting from here to rust, and lowering/lifting from rust to here,
# and this convinces us that lowering/lifting from here to rust is correct, then
# together, we've shown the correctness of the return leg.
st = Stringifier()


def affirmEnchaine(vals, toString, rustyStringify=lambda v: str(v).lower()):
    for v in vals:
        str_v = toString(v)
        assert rustyStringify(v) == str_v, f"String compare error {v} => {str_v}"


# Test the efficacy of the string transport from rust. If this fails, but everything else
# works, then things are very weird.
wellKnown = st.well_known_string("python")
assert "uniffi 💚 python!" == wellKnown

# Booleans
affirmEnchaine([True, False], st.to_string_boolean)

# Bytes.
affirmEnchaine([MIN_I8, -1, 0, 1, MAX_I8], st.to_string_i8)
affirmEnchaine([0x00, 0x12, 0xFF], st.to_string_u8)

# Shorts
affirmEnchaine([MIN_I16, -1, 0, 1, MAX_I16], st.to_string_i16)
affirmEnchaine([0x0000, 0x1234, 0xFFFF], st.to_string_u16)

# Ints
affirmEnchaine([MIN_I32, -1, 0, 1, MAX_I32], st.to_string_i32)
affirmEnchaine([0x00000000, 0x12345678, 0xFFFFFFFF], st.to_string_u32)

# Longs
affirmEnchaine([MIN_I64, -1, 0, 1, MAX_I64], st.to_string_i64)
affirmEnchaine(
    [0x0000000000000000, 0x1234567890ABCDEF, 0xFFFFFFFFFFFFFFFF], st.to_string_u64
)


# Floats
def rustyFloatToStr(v):
    """Stringify a float in the same way that rust seems to."""
    # Rust doesn't include the decimal part of whole enumber floats when stringifying.
    if int(v) == v:
        return str(int(v))
    return str(v)


affirmEnchaine([0.0, 0.5, 0.25, 1.0], st.to_string_float, rustyFloatToStr)
assert (
    st.to_string_float(F32_ONE_THIRD) == "0.33333334"
)  # annoyingly different string repr

# Doubles
# TODO: float_info.max/float_info.min don't stringify-roundtrip properly yet, TBD.
affirmEnchaine(
    [0.0, 0.5, 0.25, 1.0, 1.0 / 3],
    st.to_string_double,
    rustyFloatToStr,
)
