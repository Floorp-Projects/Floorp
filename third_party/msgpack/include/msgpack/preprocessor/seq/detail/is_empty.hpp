# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Edward Diener 2015.
#  *     Distributed under the Boost Software License, Version 1.0. (See
#  *     accompanying file LICENSE_1_0.txt or copy at
#  *     http://www.boost.org/LICENSE_1_0.txt)
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef MSGPACK_PREPROCESSOR_SEQ_DETAIL_IS_EMPTY_HPP
# define MSGPACK_PREPROCESSOR_SEQ_DETAIL_IS_EMPTY_HPP
#
# include <msgpack/preprocessor/config/config.hpp>
# include <msgpack/preprocessor/arithmetic/dec.hpp>
# include <msgpack/preprocessor/logical/bool.hpp>
# include <msgpack/preprocessor/logical/compl.hpp>
# include <msgpack/preprocessor/seq/size.hpp>
#
/* An empty seq is one that is just MSGPACK_PP_SEQ_NIL */
#
# define MSGPACK_PP_SEQ_DETAIL_IS_EMPTY(seq) \
	MSGPACK_PP_COMPL \
		( \
		MSGPACK_PP_SEQ_DETAIL_IS_NOT_EMPTY(seq) \
		) \
/**/
#
# define MSGPACK_PP_SEQ_DETAIL_IS_EMPTY_SIZE(size) \
	MSGPACK_PP_COMPL \
		( \
		MSGPACK_PP_SEQ_DETAIL_IS_NOT_EMPTY_SIZE(size) \
		) \
/**/
#
# define MSGPACK_PP_SEQ_DETAIL_IS_NOT_EMPTY(seq) \
	MSGPACK_PP_SEQ_DETAIL_IS_NOT_EMPTY_SIZE(MSGPACK_PP_SEQ_DETAIL_EMPTY_SIZE(seq)) \
/**/
#
# define MSGPACK_PP_SEQ_DETAIL_IS_NOT_EMPTY_SIZE(size) \
	MSGPACK_PP_BOOL(size) \
/**/
#
# define MSGPACK_PP_SEQ_DETAIL_EMPTY_SIZE(seq) \
	MSGPACK_PP_DEC(MSGPACK_PP_SEQ_SIZE(seq (nil))) \
/**/
#
# endif
