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
# ifndef MSGPACK_PREPROCESSOR_SEQ_FOR_EACH_HPP
# define MSGPACK_PREPROCESSOR_SEQ_FOR_EACH_HPP
#
# include <msgpack/preprocessor/arithmetic/dec.hpp>
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
# /* MSGPACK_PP_SEQ_FOR_EACH */
#
# if ~MSGPACK_PP_CONFIG_FLAGS() & MSGPACK_PP_CONFIG_EDG()
#    define MSGPACK_PP_SEQ_FOR_EACH(macro, data, seq) MSGPACK_PP_SEQ_FOR_EACH_DETAIL_CHECK(macro, data, seq)
# else
#    define MSGPACK_PP_SEQ_FOR_EACH(macro, data, seq) MSGPACK_PP_SEQ_FOR_EACH_D(macro, data, seq)
#    define MSGPACK_PP_SEQ_FOR_EACH_D(macro, data, seq) MSGPACK_PP_SEQ_FOR_EACH_DETAIL_CHECK(macro, data, seq)
# endif
#
#    define MSGPACK_PP_SEQ_FOR_EACH_DETAIL_CHECK_EXEC(macro, data, seq) MSGPACK_PP_FOR((macro, data, seq, MSGPACK_PP_SEQ_SIZE(seq)), MSGPACK_PP_SEQ_FOR_EACH_P, MSGPACK_PP_SEQ_FOR_EACH_O, MSGPACK_PP_SEQ_FOR_EACH_M)
#    define MSGPACK_PP_SEQ_FOR_EACH_DETAIL_CHECK_EMPTY(macro, data, seq)
#
#    define MSGPACK_PP_SEQ_FOR_EACH_DETAIL_CHECK(macro, data, seq) \
		MSGPACK_PP_IIF \
			( \
			MSGPACK_PP_SEQ_DETAIL_IS_NOT_EMPTY(seq), \
			MSGPACK_PP_SEQ_FOR_EACH_DETAIL_CHECK_EXEC, \
			MSGPACK_PP_SEQ_FOR_EACH_DETAIL_CHECK_EMPTY \
			) \
		(macro, data, seq) \
/**/
#
# define MSGPACK_PP_SEQ_FOR_EACH_P(r, x) MSGPACK_PP_TUPLE_ELEM(4, 3, x)
#
# if MSGPACK_PP_CONFIG_FLAGS() & MSGPACK_PP_CONFIG_STRICT()
#    define MSGPACK_PP_SEQ_FOR_EACH_O(r, x) MSGPACK_PP_SEQ_FOR_EACH_O_I x
# else
#    define MSGPACK_PP_SEQ_FOR_EACH_O(r, x) MSGPACK_PP_SEQ_FOR_EACH_O_I(MSGPACK_PP_TUPLE_ELEM(4, 0, x), MSGPACK_PP_TUPLE_ELEM(4, 1, x), MSGPACK_PP_TUPLE_ELEM(4, 2, x), MSGPACK_PP_TUPLE_ELEM(4, 3, x))
# endif
#
# define MSGPACK_PP_SEQ_FOR_EACH_O_I(macro, data, seq, sz) \
	MSGPACK_PP_SEQ_FOR_EACH_O_I_DEC(macro, data, seq, MSGPACK_PP_DEC(sz)) \
/**/
# define MSGPACK_PP_SEQ_FOR_EACH_O_I_DEC(macro, data, seq, sz) \
	( \
	macro, \
	data, \
	MSGPACK_PP_IF \
		( \
		sz, \
		MSGPACK_PP_SEQ_FOR_EACH_O_I_TAIL, \
		MSGPACK_PP_SEQ_FOR_EACH_O_I_NIL \
		) \
	(seq), \
	sz \
	) \
/**/
# define MSGPACK_PP_SEQ_FOR_EACH_O_I_TAIL(seq) MSGPACK_PP_SEQ_TAIL(seq)
# define MSGPACK_PP_SEQ_FOR_EACH_O_I_NIL(seq) MSGPACK_PP_NIL
#
# if MSGPACK_PP_CONFIG_FLAGS() & MSGPACK_PP_CONFIG_STRICT()
#    define MSGPACK_PP_SEQ_FOR_EACH_M(r, x) MSGPACK_PP_SEQ_FOR_EACH_M_IM(r, MSGPACK_PP_TUPLE_REM_4 x)
#    define MSGPACK_PP_SEQ_FOR_EACH_M_IM(r, im) MSGPACK_PP_SEQ_FOR_EACH_M_I(r, im)
# else
#    define MSGPACK_PP_SEQ_FOR_EACH_M(r, x) MSGPACK_PP_SEQ_FOR_EACH_M_I(r, MSGPACK_PP_TUPLE_ELEM(4, 0, x), MSGPACK_PP_TUPLE_ELEM(4, 1, x), MSGPACK_PP_TUPLE_ELEM(4, 2, x), MSGPACK_PP_TUPLE_ELEM(4, 3, x))
# endif
#
# define MSGPACK_PP_SEQ_FOR_EACH_M_I(r, macro, data, seq, sz) macro(r, data, MSGPACK_PP_SEQ_HEAD(seq))
#
# /* MSGPACK_PP_SEQ_FOR_EACH_R */
#
# if ~MSGPACK_PP_CONFIG_FLAGS() & MSGPACK_PP_CONFIG_EDG()
#    define MSGPACK_PP_SEQ_FOR_EACH_R(r, macro, data, seq) MSGPACK_PP_SEQ_FOR_EACH_DETAIL_CHECK_R(r, macro, data, seq)
# else
#    define MSGPACK_PP_SEQ_FOR_EACH_R(r, macro, data, seq) MSGPACK_PP_SEQ_FOR_EACH_R_I(r, macro, data, seq)
#    define MSGPACK_PP_SEQ_FOR_EACH_R_I(r, macro, data, seq) MSGPACK_PP_SEQ_FOR_EACH_DETAIL_CHECK_R(r, macro, data, seq)
# endif
#
#    define MSGPACK_PP_SEQ_FOR_EACH_DETAIL_CHECK_EXEC_R(r, macro, data, seq) MSGPACK_PP_FOR_ ## r((macro, data, seq, MSGPACK_PP_SEQ_SIZE(seq)), MSGPACK_PP_SEQ_FOR_EACH_P, MSGPACK_PP_SEQ_FOR_EACH_O, MSGPACK_PP_SEQ_FOR_EACH_M)
#    define MSGPACK_PP_SEQ_FOR_EACH_DETAIL_CHECK_EMPTY_R(r, macro, data, seq)
#
#    define MSGPACK_PP_SEQ_FOR_EACH_DETAIL_CHECK_R(r, macro, data, seq) \
		MSGPACK_PP_IIF \
			( \
			MSGPACK_PP_SEQ_DETAIL_IS_NOT_EMPTY(seq), \
			MSGPACK_PP_SEQ_FOR_EACH_DETAIL_CHECK_EXEC_R, \
			MSGPACK_PP_SEQ_FOR_EACH_DETAIL_CHECK_EMPTY_R \
			) \
		(r, macro, data, seq) \
/**/
#
# endif
