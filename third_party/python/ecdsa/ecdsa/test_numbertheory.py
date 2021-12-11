import operator
from six import print_
from functools import reduce
import operator
try:
    import unittest2 as unittest
except ImportError:
    import unittest
import hypothesis.strategies as st
import pytest
from hypothesis import given, settings, example
try:
    from hypothesis import HealthCheck
    HC_PRESENT=True
except ImportError:  # pragma: no cover
    HC_PRESENT=False
from .numbertheory import (SquareRootError, factorization, gcd, lcm,
                           jacobi, inverse_mod,
                           is_prime, next_prime, smallprimes,
                           square_root_mod_prime)


BIGPRIMES = (999671,
             999683,
             999721,
             999727,
             999749,
             999763,
             999769,
             999773,
             999809,
             999853,
             999863,
             999883,
             999907,
             999917,
             999931,
             999953,
             999959,
             999961,
             999979,
             999983)


@pytest.mark.parametrize(
    "prime, next_p",
    [(p, q) for p, q in zip(BIGPRIMES[:-1], BIGPRIMES[1:])])
def test_next_prime(prime, next_p):
    assert next_prime(prime) == next_p


@pytest.mark.parametrize(
    "val",
    [-1, 0, 1])
def test_next_prime_with_nums_less_2(val):
    assert next_prime(val) == 2


@pytest.mark.parametrize("prime", smallprimes)
def test_square_root_mod_prime_for_small_primes(prime):
    squares = set()
    for num in range(0, 1 + prime // 2):
        sq = num * num % prime
        squares.add(sq)
        root = square_root_mod_prime(sq, prime)
        # tested for real with TestNumbertheory.test_square_root_mod_prime
        assert root * root % prime == sq

    for nonsquare in range(0, prime):
        if nonsquare in squares:
            continue
        with pytest.raises(SquareRootError):
            square_root_mod_prime(nonsquare, prime)


@st.composite
def st_two_nums_rel_prime(draw):
    # 521-bit is the biggest curve we operate on, use 1024 for a bit
    # of breathing space
    mod = draw(st.integers(min_value=2, max_value=2**1024))
    num = draw(st.integers(min_value=1, max_value=mod-1)
               .filter(lambda x: gcd(x, mod) == 1))
    return num, mod


@st.composite
def st_primes(draw, *args, **kwargs):
    if "min_value" not in kwargs:  # pragma: no branch
        kwargs["min_value"] = 1
    prime = draw(st.sampled_from(smallprimes) |
                 st.integers(*args, **kwargs)
                 .filter(is_prime))
    return prime


@st.composite
def st_num_square_prime(draw):
    prime = draw(st_primes(max_value=2**1024))
    num = draw(st.integers(min_value=0, max_value=1 + prime // 2))
    sq = num * num % prime
    return sq, prime


@st.composite
def st_comp_with_com_fac(draw):
    """
    Strategy that returns lists of numbers, all having a common factor.
    """
    primes = draw(st.lists(st_primes(max_value=2**512), min_size=1,
                           max_size=10))
    # select random prime(s) that will make the common factor of composites
    com_fac_primes = draw(st.lists(st.sampled_from(primes),
                                   min_size=1, max_size=20))
    com_fac = reduce(operator.mul, com_fac_primes, 1)

    # select at most 20 lists (returned numbers),
    # each having at most 30 primes (factors) including none (then the number
    # will be 1)
    comp_primes = draw(
        st.integers(min_value=1, max_value=20).
        flatmap(lambda n: st.lists(st.lists(st.sampled_from(primes),
                                            max_size=30),
                                   min_size=1, max_size=n)))

    return [reduce(operator.mul, nums, 1) * com_fac for nums in comp_primes]


@st.composite
def st_comp_no_com_fac(draw):
    """
    Strategy that returns lists of numbers that don't have a common factor.
    """
    primes = draw(st.lists(st_primes(max_value=2**512),
                           min_size=2, max_size=10, unique=True))
    # first select the primes that will create the uncommon factor
    # between returned numbers
    uncom_fac_primes = draw(st.lists(
        st.sampled_from(primes),
        min_size=1, max_size=len(primes)-1, unique=True))
    uncom_fac = reduce(operator.mul, uncom_fac_primes, 1)

    # then build composites from leftover primes
    leftover_primes = [i for i in primes if i not in uncom_fac_primes]

    assert leftover_primes
    assert uncom_fac_primes

    # select at most 20 lists, each having at most 30 primes
    # selected from the leftover_primes list
    number_primes = draw(
        st.integers(min_value=1, max_value=20).
        flatmap(lambda n: st.lists(st.lists(st.sampled_from(leftover_primes),
                                            max_size=30),
                                   min_size=1, max_size=n)))

    numbers = [reduce(operator.mul, nums, 1) for nums in number_primes]

    insert_at = draw(st.integers(min_value=0, max_value=len(numbers)))
    numbers.insert(insert_at, uncom_fac)
    return numbers


HYP_SETTINGS = {}
if HC_PRESENT:  # pragma: no branch
    HYP_SETTINGS['suppress_health_check']=[HealthCheck.filter_too_much,
                                           HealthCheck.too_slow]
    # the factorization() sometimes takes a long time to finish
    HYP_SETTINGS['deadline'] = 5000


HYP_SLOW_SETTINGS=dict(HYP_SETTINGS)
HYP_SLOW_SETTINGS["max_examples"] = 10


class TestNumbertheory(unittest.TestCase):
    def test_gcd(self):
        assert gcd(3 * 5 * 7, 3 * 5 * 11, 3 * 5 * 13) == 3 * 5
        assert gcd([3 * 5 * 7, 3 * 5 * 11, 3 * 5 * 13]) == 3 * 5
        assert gcd(3) == 3

    @unittest.skipUnless(HC_PRESENT,
                         "Hypothesis 2.0.0 can't be made tolerant of hard to "
                         "meet requirements (like `is_prime()`), the test "
                         "case times-out on it")
    @settings(**HYP_SLOW_SETTINGS)
    @given(st_comp_with_com_fac())
    def test_gcd_with_com_factor(self, numbers):
        n = gcd(numbers)
        assert 1 in numbers or n != 1
        for i in numbers:
            assert i % n == 0

    @unittest.skipUnless(HC_PRESENT,
                         "Hypothesis 2.0.0 can't be made tolerant of hard to "
                         "meet requirements (like `is_prime()`), the test "
                         "case times-out on it")
    @settings(**HYP_SLOW_SETTINGS)
    @given(st_comp_no_com_fac())
    def test_gcd_with_uncom_factor(self, numbers):
        n = gcd(numbers)
        assert n == 1

    @given(st.lists(st.integers(min_value=1, max_value=2**8192),
                    min_size=1, max_size=20))
    def test_gcd_with_random_numbers(self, numbers):
        n = gcd(numbers)
        for i in numbers:
            # check that at least it's a divider
            assert i % n == 0

    def test_lcm(self):
        assert lcm(3, 5 * 3, 7 * 3) == 3 * 5 * 7
        assert lcm([3, 5 * 3, 7 * 3]) == 3 * 5 * 7
        assert lcm(3) == 3

    @given(st.lists(st.integers(min_value=1, max_value=2**8192),
                    min_size=1, max_size=20))
    def test_lcm_with_random_numbers(self, numbers):
        n = lcm(numbers)
        for i in numbers:
            assert n % i == 0

    @unittest.skipUnless(HC_PRESENT,
                         "Hypothesis 2.0.0 can't be made tolerant of hard to "
                         "meet requirements (like `is_prime()`), the test "
                         "case times-out on it")
    @settings(**HYP_SETTINGS)
    @given(st_num_square_prime())
    def test_square_root_mod_prime(self, vals):
        square, prime = vals

        calc = square_root_mod_prime(square, prime)
        assert calc * calc % prime == square

    @settings(**HYP_SETTINGS)
    @given(st.integers(min_value=1, max_value=10**12))
    @example(265399 * 1526929)
    @example(373297 ** 2 * 553991)
    def test_factorization(self, num):
        factors = factorization(num)
        mult = 1
        for i in factors:
            mult *= i[0] ** i[1]
        assert mult == num

    @settings(**HYP_SETTINGS)
    @given(st.integers(min_value=3, max_value=1000).filter(lambda x: x % 2))
    def test_jacobi(self, mod):
        if is_prime(mod):
            squares = set()
            for root in range(1, mod):
                assert jacobi(root * root, mod) == 1
                squares.add(root * root % mod)
            for i in range(1, mod):
                if i not in squares:
                    assert jacobi(i, mod) == -1
        else:
            factors = factorization(mod)
            for a in range(1, mod):
                c = 1
                for i in factors:
                    c *= jacobi(a, i[0]) ** i[1]
                assert c == jacobi(a, mod)

    @given(st_two_nums_rel_prime())
    def test_inverse_mod(self, nums):
        num, mod = nums

        inv = inverse_mod(num, mod)

        assert 0 < inv < mod
        assert num * inv % mod == 1

    def test_inverse_mod_with_zero(self):
        assert 0 == inverse_mod(0, 11)
