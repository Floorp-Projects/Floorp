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
# ifndef MSGPACK_PREPROCESSOR_SEQ_REPLACE_HPP
# define MSGPACK_PREPROCESSOR_SEQ_REPLACE_HPP
#
# include <msgpack/preprocessor/arithmetic/dec.hpp>
# include <msgpack/preprocessor/arithmetic/inc.hpp>
# include <msgpack/preprocessor/config/config.hpp>
# include <msgpack/preprocessor/comparison/equal.hpp>
# include <msgpack/preprocessor/control/iif.hpp>
# include <msgpack/preprocessor/seq/first_n.hpp>
# include <msgpack/preprocessor/seq/rest_n.hpp>
# include <msgpack/preprocessor/seq/size.hpp>
#
# /* MSGPACK_PP_SEQ_REPLACE */
#
# if ~MSGPACK_PP_CONFIG_FLAGS() & MSGPACK_PP_CONFIG_EDG()
#    define MSGPACK_PP_SEQ_REPLACE(seq, i, elem) MSGPACK_PP_SEQ_FIRST_N(i, seq) (elem) MSGPACK_PP_SEQ_REPLACE_DETAIL_REST(seq, i)
# else
#    define MSGPACK_PP_SEQ_REPLACE(seq, i, elem) MSGPACK_PP_SEQ_REPLACE_I(seq, i, elem)
#    define MSGPACK_PP_SEQ_REPLACE_I(seq, i, elem) MSGPACK_PP_SEQ_FIRST_N(i, seq) (elem) MSGPACK_PP_SEQ_REPLACE_DETAIL_REST(seq, i)
# endif
#
#    define MSGPACK_PP_SEQ_REPLACE_DETAIL_REST_EMPTY(seq, i)
#    define MSGPACK_PP_SEQ_REPLACE_DETAIL_REST_VALID(seq, i) MSGPACK_PP_SEQ_REST_N(MSGPACK_PP_INC(i), seq)
#    define MSGPACK_PP_SEQ_REPLACE_DETAIL_REST(seq, i) \
		MSGPACK_PP_IIF \
			( \
			MSGPACK_PP_EQUAL(i,MSGPACK_PP_DEC(MSGPACK_PP_SEQ_SIZE(seq))), \
			MSGPACK_PP_SEQ_REPLACE_DETAIL_REST_EMPTY, \
			MSGPACK_PP_SEQ_REPLACE_DETAIL_REST_VALID \
			) \
		(seq, i) \
/**/
#
# endif
