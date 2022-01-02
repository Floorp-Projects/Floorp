# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Paul Mensonides 2002.
#  *     Distributed under the Boost Software License, Version 1.0. (See
#  *     accompanying file LICENSE_1_0.txt or copy at
#  *     http://www.boost.org/LICENSE_1_0.txt)
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef MSGPACK_PREPROCESSOR_SEQ_FOR_EACH_I_HPP
# define MSGPACK_PREPROCESSOR_SEQ_FOR_EACH_I_HPP
#
# include <msgpack/preprocessor/arithmetic/dec.hpp>
# include <msgpack/preprocessor/arithmetic/inc.hpp>
# include <msgpack/preprocessor/config/config.hpp>
# include <msgpack/preprocessor/control/if.hpp>
# include <msgpack/preprocessor/control/iif.hpp>
# include <msgpack/preprocessor/repetition/for.hpp>
# include <msgpack/preprocessor/seq/seq.hpp>
# include <msgpack/preprocessor/seq/size.hpp>
# include <msgpack/preprocessor/seq/detail/is_empty.hpp>
# include <msgpack/preprocessor/tuple/elem.hpp>
# include <msgpack/preprocessor/tuple/rem.hpp>
#
# /* MSGPACK_PP_SEQ_FOR_EACH_I */
#
# if ~MSGPACK_PP_CONFIG_FLAGS() & MSGPACK_PP_CONFIG_EDG()
#    define MSGPACK_PP_SEQ_FOR_EACH_I(macro, data, seq) MSGPACK_PP_SEQ_FOR_EACH_I_DETAIL_CHECK(macro, data, seq)
# else
#    define MSGPACK_PP_SEQ_FOR_EACH_I(macro, data, seq) MSGPACK_PP_SEQ_FOR_EACH_I_I(macro, data, seq)
#    define MSGPACK_PP_SEQ_FOR_EACH_I_I(macro, data, seq) MSGPACK_PP_SEQ_FOR_EACH_I_DETAIL_CHECK(macro, data, seq)
# endif
#
#    define MSGPACK_PP_SEQ_FOR_EACH_I_DETAIL_CHECK_EXEC(macro, data, seq) MSGPACK_PP_FOR((macro, data, seq, 0, MSGPACK_PP_SEQ_SIZE(seq)), MSGPACK_PP_SEQ_FOR_EACH_I_P, MSGPACK_PP_SEQ_FOR_EACH_I_O, MSGPACK_PP_SEQ_FOR_EACH_I_M)
#    define MSGPACK_PP_SEQ_FOR_EACH_I_DETAIL_CHECK_EMPTY(macro, data, seq)
#
#    define MSGPACK_PP_SEQ_FOR_EACH_I_DETAIL_CHECK(macro, data, seq) \
		MSGPACK_PP_IIF \
			( \
			MSGPACK_PP_SEQ_DETAIL_IS_NOT_EMPTY(seq), \
			MSGPACK_PP_SEQ_FOR_EACH_I_DETAIL_CHECK_EXEC, \
			MSGPACK_PP_SEQ_FOR_EACH_I_DETAIL_CHECK_EMPTY \
			) \
		(macro, data, seq) \
/**/
#
# define MSGPACK_PP_SEQ_FOR_EACH_I_P(r, x) MSGPACK_PP_TUPLE_ELEM(5, 4, x)
#
# if MSGPACK_PP_CONFIG_FLAGS() & MSGPACK_PP_CONFIG_STRICT()
#    define MSGPACK_PP_SEQ_FOR_EACH_I_O(r, x) MSGPACK_PP_SEQ_FOR_EACH_I_O_I x
# else
#    define MSGPACK_PP_SEQ_FOR_EACH_I_O(r, x) MSGPACK_PP_SEQ_FOR_EACH_I_O_I(MSGPACK_PP_TUPLE_ELEM(5, 0, x), MSGPACK_PP_TUPLE_ELEM(5, 1, x), MSGPACK_PP_TUPLE_ELEM(5, 2, x), MSGPACK_PP_TUPLE_ELEM(5, 3, x), MSGPACK_PP_TUPLE_ELEM(5, 4, x))
# endif
#
# define MSGPACK_PP_SEQ_FOR_EACH_I_O_I(macro, data, seq, i, sz) \
	MSGPACK_PP_SEQ_FOR_EACH_I_O_I_DEC(macro, data, seq, i, MSGPACK_PP_DEC(sz)) \
/**/
# define MSGPACK_PP_SEQ_FOR_EACH_I_O_I_DEC(macro, data, seq, i, sz) \
	( \
	macro, \
	data, \
	MSGPACK_PP_IF \
		( \
		sz, \
		MSGPACK_PP_SEQ_FOR_EACH_I_O_I_TAIL, \
		MSGPACK_PP_SEQ_FOR_EACH_I_O_I_NIL \
		) \
	(seq), \
	MSGPACK_PP_INC(i), \
	sz \
	) \
/**/
# define MSGPACK_PP_SEQ_FOR_EACH_I_O_I_TAIL(seq) MSGPACK_PP_SEQ_TAIL(seq)
# define MSGPACK_PP_SEQ_FOR_EACH_I_O_I_NIL(seq) MSGPACK_PP_NIL
#
# if MSGPACK_PP_CONFIG_FLAGS() & MSGPACK_PP_CONFIG_STRICT()
#    define MSGPACK_PP_SEQ_FOR_EACH_I_M(r, x) MSGPACK_PP_SEQ_FOR_EACH_I_M_IM(r, MSGPACK_PP_TUPLE_REM_5 x)
#    define MSGPACK_PP_SEQ_FOR_EACH_I_M_IM(r, im) MSGPACK_PP_SEQ_FOR_EACH_I_M_I(r, im)
# else
#    define MSGPACK_PP_SEQ_FOR_EACH_I_M(r, x) MSGPACK_PP_SEQ_FOR_EACH_I_M_I(r, MSGPACK_PP_TUPLE_ELEM(5, 0, x), MSGPACK_PP_TUPLE_ELEM(5, 1, x), MSGPACK_PP_TUPLE_ELEM(5, 2, x), MSGPACK_PP_TUPLE_ELEM(5, 3, x), MSGPACK_PP_TUPLE_ELEM(5, 4, x))
# endif
#
# define MSGPACK_PP_SEQ_FOR_EACH_I_M_I(r, macro, data, seq, i, sz) macro(r, data, i, MSGPACK_PP_SEQ_HEAD(seq))
#
# /* MSGPACK_PP_SEQ_FOR_EACH_I_R */
#
# if ~MSGPACK_PP_CONFIG_FLAGS() & MSGPACK_PP_CONFIG_EDG()
#    define MSGPACK_PP_SEQ_FOR_EACH_I_R(r, macro, data, seq) MSGPACK_PP_SEQ_FOR_EACH_I_R_DETAIL_CHECK(r, macro, data, seq)
# else
#    define MSGPACK_PP_SEQ_FOR_EACH_I_R(r, macro, data, seq) MSGPACK_PP_SEQ_FOR_EACH_I_R_I(r, macro, data, seq)
#    define MSGPACK_PP_SEQ_FOR_EACH_I_R_I(r, macro, data, seq) MSGPACK_PP_SEQ_FOR_EACH_I_R_DETAIL_CHECK(r, macro, data, seq)
# endif
#
#    define MSGPACK_PP_SEQ_FOR_EACH_I_R_DETAIL_CHECK_EXEC(r, macro, data, seq) MSGPACK_PP_FOR_ ## r((macro, data, seq, 0, MSGPACK_PP_SEQ_SIZE(seq)), MSGPACK_PP_SEQ_FOR_EACH_I_P, MSGPACK_PP_SEQ_FOR_EACH_I_O, MSGPACK_PP_SEQ_FOR_EACH_I_M)
#    define MSGPACK_PP_SEQ_FOR_EACH_I_R_DETAIL_CHECK_EMPTY(r, macro, data, seq)
#
#    define MSGPACK_PP_SEQ_FOR_EACH_I_R_DETAIL_CHECK(r, macro, data, seq) \
		MSGPACK_PP_IIF \
			( \
			MSGPACK_PP_SEQ_DETAIL_IS_NOT_EMPTY(seq), \
			MSGPACK_PP_SEQ_FOR_EACH_I_R_DETAIL_CHECK_EXEC, \
			MSGPACK_PP_SEQ_FOR_EACH_I_R_DETAIL_CHECK_EMPTY \
			) \
		(r, macro, data, seq) \
/**/
#
# endif
