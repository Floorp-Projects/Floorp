/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Benoit Jacob <bjacob@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "CheckedInt.h"
#include <iostream>

#ifndef CHECKEDINT_ENABLE_MOZ_ASSERTS
    #error CHECKEDINT_ENABLE_MOZ_ASSERTS should be defined by CheckedInt.h
#endif

namespace CheckedInt_test {

using namespace mozilla::detail;
using mozilla::CheckedInt;

int g_integer_types_tested = 0;
int g_tests_passed = 0;
int g_tests_failed = 0;

void verify_impl_function(bool x, bool expected,
                          const char* file, int line,
                          int T_size, bool T_is_T_signed)
{
    if (x == expected) {
        g_tests_passed++;
    } else {
        g_tests_failed++;
        std::cerr << "Test failed at " << file << ":" << line;
        std::cerr << " with T a ";
        if(T_is_T_signed)
            std::cerr << "signed";
        else
            std::cerr << "unsigned";
        std::cerr << " " << CHAR_BIT*T_size << "-bit integer type" << std::endl;
    }
}

#define VERIFY_IMPL(x, expected) \
    verify_impl_function((x), (expected), __FILE__, __LINE__, sizeof(T), is_signed<T>::value)

#define VERIFY(x)            VERIFY_IMPL(x, true)
#define VERIFY_IS_FALSE(x)   VERIFY_IMPL(x, false)
#define VERIFY_IS_VALID(x)   VERIFY_IMPL((x).valid(), true)
#define VERIFY_IS_INVALID(x) VERIFY_IMPL((x).valid(), false)
#define VERIFY_IS_VALID_IF(x,condition) VERIFY_IMPL((x).valid(), (condition))

template<typename T, unsigned int size = sizeof(T)>
struct test_twice_bigger_type
{
    static void run()
    {
        VERIFY(is_supported<typename twice_bigger_type<T>::type>::value);
        VERIFY(sizeof(typename twice_bigger_type<T>::type)
                   == 2 * sizeof(T));
        VERIFY(bool(is_signed<typename twice_bigger_type<T>::type>::value)
                   == bool(is_signed<T>::value));
    }
};

template<typename T>
struct test_twice_bigger_type<T, 8>
{
    static void run()
    {
        VERIFY_IS_FALSE(is_supported<typename twice_bigger_type<T>::type>::value);
    }
};


template<typename T>
void test()
{
    static bool already_run = false;
    // integer types from different families may just be typedefs for types from other families.
    // e.g. int32_t might be just a typedef for int. No point re-running the same tests then.
    if (already_run)
        return;
    already_run = true;
    g_integer_types_tested++;

    VERIFY(is_supported<T>::value);
    enum{ is_T_signed = is_signed<T>::value };
    VERIFY(bool(is_T_signed) == !bool(T(-1) > T(0)));

    test_twice_bigger_type<T>::run();

    typedef typename unsigned_type<T>::type unsigned_T;

    VERIFY(sizeof(unsigned_T) == sizeof(T));
    VERIFY(is_signed<unsigned_T>::value == false);

    const CheckedInt<T> max(max_value<T>::value());
    const CheckedInt<T> min(min_value<T>::value());

    // check min() and max(), since they are custom implementations and a mistake there
    // could potentially NOT be caught by any other tests... while making everything wrong!

    T bit = 1;
    for(unsigned int i = 0; i < sizeof(T) * CHAR_BIT - 1; i++)
    {
        VERIFY((min.value() & bit) == 0);
        bit <<= 1;
    }
    VERIFY((min.value() & bit) == (is_T_signed ? bit : T(0)));
    VERIFY(max.value() == T(~(min.value())));

    const CheckedInt<T> zero(0);
    const CheckedInt<T> one(1);
    const CheckedInt<T> two(2);
    const CheckedInt<T> three(3);
    const CheckedInt<T> four(4);

    /* addition / substraction checks */

    VERIFY_IS_VALID(zero+zero);
    VERIFY(zero+zero == zero);
    VERIFY_IS_FALSE(zero+zero == one); // check that == doesn't always return true
    VERIFY_IS_VALID(zero+one);
    VERIFY(zero+one == one);
    VERIFY_IS_VALID(one+one);
    VERIFY(one+one == two);

    const CheckedInt<T> max_minus_one = max - one;
    const CheckedInt<T> max_minus_two = max - two;
    VERIFY_IS_VALID(max_minus_one);
    VERIFY_IS_VALID(max_minus_two);
    VERIFY_IS_VALID(max_minus_one + one);
    VERIFY_IS_VALID(max_minus_two + one);
    VERIFY_IS_VALID(max_minus_two + two);
    VERIFY(max_minus_one + one == max);
    VERIFY(max_minus_two + one == max_minus_one);
    VERIFY(max_minus_two + two == max);

    VERIFY_IS_VALID(max + zero);
    VERIFY_IS_VALID(max - zero);
    VERIFY_IS_INVALID(max + one);
    VERIFY_IS_INVALID(max + two);
    VERIFY_IS_INVALID(max + max_minus_one);
    VERIFY_IS_INVALID(max + max);

    const CheckedInt<T> min_plus_one = min + one;
    const CheckedInt<T> min_plus_two = min + two;
    VERIFY_IS_VALID(min_plus_one);
    VERIFY_IS_VALID(min_plus_two);
    VERIFY_IS_VALID(min_plus_one - one);
    VERIFY_IS_VALID(min_plus_two - one);
    VERIFY_IS_VALID(min_plus_two - two);
    VERIFY(min_plus_one - one == min);
    VERIFY(min_plus_two - one == min_plus_one);
    VERIFY(min_plus_two - two == min);

    const CheckedInt<T> min_minus_one = min - one;
    VERIFY_IS_VALID(min + zero);
    VERIFY_IS_VALID(min - zero);
    VERIFY_IS_INVALID(min - one);
    VERIFY_IS_INVALID(min - two);
    VERIFY_IS_INVALID(min - min_minus_one);
    VERIFY_IS_VALID(min - min);

    const CheckedInt<T> max_over_two = max / two;
    VERIFY_IS_VALID(max_over_two + max_over_two);
    VERIFY_IS_VALID(max_over_two + one);
    VERIFY((max_over_two + one) - one == max_over_two);
    VERIFY_IS_VALID(max_over_two - max_over_two);
    VERIFY(max_over_two - max_over_two == zero);

    const CheckedInt<T> min_over_two = min / two;
    VERIFY_IS_VALID(min_over_two + min_over_two);
    VERIFY_IS_VALID(min_over_two + one);
    VERIFY((min_over_two + one) - one == min_over_two);
    VERIFY_IS_VALID(min_over_two - min_over_two);
    VERIFY(min_over_two - min_over_two == zero);

    VERIFY_IS_INVALID(min - one);
    VERIFY_IS_INVALID(min - two);

    if (is_T_signed) {
        VERIFY_IS_INVALID(min + min);
        VERIFY_IS_INVALID(min_over_two + min_over_two + min_over_two);
        VERIFY_IS_INVALID(zero - min + min);
        VERIFY_IS_INVALID(one - min + min);
    }

    /* unary operator- checks */

    const CheckedInt<T> neg_one = -one;
    const CheckedInt<T> neg_two = -two;

    if (is_T_signed) {
        VERIFY_IS_VALID(-max);
        VERIFY_IS_VALID(-max - one);
        VERIFY_IS_VALID(neg_one);
        VERIFY_IS_VALID(-max + neg_one);
        VERIFY_IS_VALID(neg_one + one);
        VERIFY(neg_one + one == zero);
        VERIFY_IS_VALID(neg_two);
        VERIFY_IS_VALID(neg_one + neg_one);
        VERIFY(neg_one + neg_one == neg_two);
    } else {
        VERIFY_IS_INVALID(neg_one);
    }

    /* multiplication checks */

    VERIFY_IS_VALID(zero*zero);
    VERIFY(zero*zero == zero);
    VERIFY_IS_VALID(zero*one);
    VERIFY(zero*one == zero);
    VERIFY_IS_VALID(one*zero);
    VERIFY(one*zero == zero);
    VERIFY_IS_VALID(one*one);
    VERIFY(one*one == one);
    VERIFY_IS_VALID(one*three);
    VERIFY(one*three == three);
    VERIFY_IS_VALID(two*two);
    VERIFY(two*two == four);

    VERIFY_IS_INVALID(max * max);
    VERIFY_IS_INVALID(max_over_two * max);
    VERIFY_IS_INVALID(max_over_two * max_over_two);

    const CheckedInt<T> max_approx_sqrt(T(T(1) << (CHAR_BIT*sizeof(T)/2)));

    VERIFY_IS_VALID(max_approx_sqrt);
    VERIFY_IS_VALID(max_approx_sqrt * two);
    VERIFY_IS_INVALID(max_approx_sqrt * max_approx_sqrt);
    VERIFY_IS_INVALID(max_approx_sqrt * max_approx_sqrt * max_approx_sqrt);

    if (is_T_signed) {
        VERIFY_IS_INVALID(min * min);
        VERIFY_IS_INVALID(min_over_two * min);
        VERIFY_IS_INVALID(min_over_two * min_over_two);

        const CheckedInt<T> min_approx_sqrt = -max_approx_sqrt;

        VERIFY_IS_VALID(min_approx_sqrt);
        VERIFY_IS_VALID(min_approx_sqrt * two);
        VERIFY_IS_INVALID(min_approx_sqrt * max_approx_sqrt);
        VERIFY_IS_INVALID(min_approx_sqrt * min_approx_sqrt);
    }

    // make sure to check all 4 paths in signed multiplication validity check.
    // test positive * positive
    VERIFY_IS_VALID(max * one);
    VERIFY(max * one == max);
    VERIFY_IS_INVALID(max * two);
    VERIFY_IS_VALID(max_over_two * two);
    VERIFY((max_over_two + max_over_two) == (max_over_two * two));

    if (is_T_signed) {
        // test positive * negative
        VERIFY_IS_VALID(max * neg_one);
        VERIFY_IS_VALID(-max);
        VERIFY(max * neg_one == -max);
        VERIFY_IS_VALID(one * min);
        VERIFY_IS_INVALID(max * neg_two);
        VERIFY_IS_VALID(max_over_two * neg_two);
        VERIFY_IS_VALID(two * min_over_two);
        VERIFY_IS_VALID((max_over_two + one) * neg_two);
        VERIFY_IS_INVALID((max_over_two + two) * neg_two);
        VERIFY_IS_INVALID(two * (min_over_two - one));

        // test negative * positive
        VERIFY_IS_VALID(min * one);
        VERIFY_IS_VALID(min_plus_one * one);
        VERIFY_IS_INVALID(min * two);
        VERIFY_IS_VALID(min_over_two * two);
        VERIFY(min_over_two * two == min);
        VERIFY_IS_INVALID((min_over_two - one) * neg_two);
        VERIFY_IS_INVALID(neg_two * max);
        VERIFY_IS_VALID(min_over_two * two);
        VERIFY(min_over_two * two == min);
        VERIFY_IS_VALID(neg_two * max_over_two);
        VERIFY_IS_INVALID((min_over_two - one) * two);
        VERIFY_IS_VALID(neg_two * (max_over_two + one));
        VERIFY_IS_INVALID(neg_two * (max_over_two + two));

        // test negative * negative
        VERIFY_IS_INVALID(min * neg_one);
        VERIFY_IS_VALID(min_plus_one * neg_one);
        VERIFY(min_plus_one * neg_one == max);
        VERIFY_IS_INVALID(min * neg_two);
        VERIFY_IS_INVALID(min_over_two * neg_two);
        VERIFY_IS_INVALID(neg_one * min);
        VERIFY_IS_VALID(neg_one * min_plus_one);
        VERIFY(neg_one * min_plus_one == max);
        VERIFY_IS_INVALID(neg_two * min);
        VERIFY_IS_INVALID(neg_two * min_over_two);
    }

    /* division checks */

    VERIFY_IS_VALID(one / one);
    VERIFY(one / one == one);
    VERIFY_IS_VALID(three / three);
    VERIFY(three / three == one);
    VERIFY_IS_VALID(four / two);
    VERIFY(four / two == two);
    VERIFY((four*three)/four == three);

    // check that div by zero is invalid
    VERIFY_IS_INVALID(zero / zero);
    VERIFY_IS_INVALID(one / zero);
    VERIFY_IS_INVALID(two / zero);
    VERIFY_IS_INVALID(neg_one / zero);
    VERIFY_IS_INVALID(max / zero);
    VERIFY_IS_INVALID(min / zero);

    if (is_T_signed) {
        // check that min / -1 is invalid
        VERIFY_IS_INVALID(min / neg_one);

        // check that the test for div by -1 isn't banning other numerators than min
        VERIFY_IS_VALID(one / neg_one);
        VERIFY_IS_VALID(zero / neg_one);
        VERIFY_IS_VALID(neg_one / neg_one);
        VERIFY_IS_VALID(max / neg_one);
    }

    /* check that invalidity is correctly preserved by arithmetic ops */

    const CheckedInt<T> some_invalid = max + max;
    VERIFY_IS_INVALID(some_invalid + zero);
    VERIFY_IS_INVALID(some_invalid - zero);
    VERIFY_IS_INVALID(zero + some_invalid);
    VERIFY_IS_INVALID(zero - some_invalid);
    VERIFY_IS_INVALID(-some_invalid);
    VERIFY_IS_INVALID(some_invalid * zero);
    VERIFY_IS_INVALID(some_invalid * one);
    VERIFY_IS_INVALID(zero * some_invalid);
    VERIFY_IS_INVALID(one * some_invalid);
    VERIFY_IS_INVALID(some_invalid / zero);
    VERIFY_IS_INVALID(some_invalid / one);
    VERIFY_IS_INVALID(zero / some_invalid);
    VERIFY_IS_INVALID(one / some_invalid);
    VERIFY_IS_INVALID(some_invalid + some_invalid);
    VERIFY_IS_INVALID(some_invalid - some_invalid);
    VERIFY_IS_INVALID(some_invalid * some_invalid);
    VERIFY_IS_INVALID(some_invalid / some_invalid);

    /* check that mixing checked integers with plain integers in expressions is allowed */

    VERIFY(one + T(2) == three);
    VERIFY(2 + one == three);
    {
        CheckedInt<T> x = one;
        x += 2;
        VERIFY(x == three);
    }
    VERIFY(two - 1 == one);
    VERIFY(2 - one == one);
    {
        CheckedInt<T> x = two;
        x -= 1;
        VERIFY(x == one);
    }
    VERIFY(one * 2 == two);
    VERIFY(2 * one == two);
    {
        CheckedInt<T> x = one;
        x *= 2;
        VERIFY(x == two);
    }
    VERIFY(four / 2 == two);
    VERIFY(4 / two == two);
    {
        CheckedInt<T> x = four;
        x /= 2;
        VERIFY(x == two);
    }

    VERIFY(one == 1);
    VERIFY(1 == one);
    VERIFY_IS_FALSE(two == 1);
    VERIFY_IS_FALSE(1 == two);
    VERIFY_IS_FALSE(some_invalid == 1);
    VERIFY_IS_FALSE(1 == some_invalid);

    /* Check that construction of CheckedInt from an integer value of a mismatched type is checked */

    #define VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(U) \
    { \
        bool is_U_signed = is_signed<U>::value; \
        VERIFY_IS_VALID(CheckedInt<T>(U(0))); \
        VERIFY_IS_VALID(CheckedInt<T>(U(1))); \
        VERIFY_IS_VALID(CheckedInt<T>(U(100))); \
        if (is_U_signed) \
            VERIFY_IS_VALID_IF(CheckedInt<T>(U(-1)), is_T_signed); \
        if (sizeof(U) > sizeof(T)) \
            VERIFY_IS_INVALID(CheckedInt<T>(U(max_value<T>::value())+1)); \
        VERIFY_IS_VALID_IF(CheckedInt<T>(max_value<U>::value()), \
            (sizeof(T) > sizeof(U) || ((sizeof(T) == sizeof(U)) && (is_U_signed || !is_T_signed)))); \
        VERIFY_IS_VALID_IF(CheckedInt<T>(min_value<U>::value()), \
            is_U_signed == false ? 1 : \
            bool(is_T_signed) == false ? 0 : \
            sizeof(T) >= sizeof(U)); \
    }
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(int8_t)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(uint8_t)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(int16_t)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(uint16_t)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(int32_t)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(uint32_t)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(int64_t)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(uint64_t)

    typedef unsigned char unsigned_char;
    typedef unsigned short unsigned_short;
    typedef unsigned int  unsigned_int;
    typedef unsigned long unsigned_long;
    
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(char)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(unsigned_char)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(short)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(unsigned_short)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(int)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(unsigned_int)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(long)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(unsigned_long)

    /* Test increment/decrement operators */

    CheckedInt<T> x, y;
    x = one;
    y = x++;
    VERIFY(x == two);
    VERIFY(y == one);
    x = one;
    y = ++x;
    VERIFY(x == two);
    VERIFY(y == two);
    x = one;
    y = x--;
    VERIFY(x == zero);
    VERIFY(y == one);
    x = one;
    y = --x;
    VERIFY(x == zero);
    VERIFY(y == zero);
    x = max;
    VERIFY_IS_VALID(x++);
    x = max;
    VERIFY_IS_INVALID(++x);
    x = min;
    VERIFY_IS_VALID(x--);
    x = min;
    VERIFY_IS_INVALID(--x);
}

} // end namespace CheckedInt_test

int main()
{
    CheckedInt_test::test<int8_t>();
    CheckedInt_test::test<uint8_t>();
    CheckedInt_test::test<int16_t>();
    CheckedInt_test::test<uint16_t>();
    CheckedInt_test::test<int32_t>();
    CheckedInt_test::test<uint32_t>();
    CheckedInt_test::test<int64_t>();
    CheckedInt_test::test<uint64_t>();

    CheckedInt_test::test<char>();
    CheckedInt_test::test<unsigned char>();
    CheckedInt_test::test<short>();
    CheckedInt_test::test<unsigned short>();
    CheckedInt_test::test<int>();
    CheckedInt_test::test<unsigned int>();
    CheckedInt_test::test<long>();
    CheckedInt_test::test<unsigned long>();

    std::cerr << CheckedInt_test::g_tests_failed << " tests failed, "
              << CheckedInt_test::g_tests_passed << " tests passed out of "
              << CheckedInt_test::g_tests_failed + CheckedInt_test::g_tests_passed
              << " tests, covering " << CheckedInt_test::g_integer_types_tested
              << " distinct integer types." << std::endl;

    return CheckedInt_test::g_tests_failed > 0
        || CheckedInt_test::g_integer_types_tested < 8;
}
