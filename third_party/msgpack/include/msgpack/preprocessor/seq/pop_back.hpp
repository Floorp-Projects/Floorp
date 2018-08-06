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
# ifndef MSGPACK_PREPROCESSOR_SEQ_POP_BACK_HPP
# define MSGPACK_PREPROCESSOR_SEQ_POP_BACK_HPP
#
# include <msgpack/preprocessor/arithmetic/dec.hpp>
# include <msgpack/preprocessor/config/config.hpp>
# include <msgpack/preprocessor/seq/first_n.hpp>
# include <msgpack/preprocessor/seq/size.hpp>
#
# /* MSGPACK_PP_SEQ_POP_BACK */
#
# if ~MSGPACK_PP_CONFIG_FLAGS() & MSGPACK_PP_CONFIG_EDG()
#    define MSGPACK_PP_SEQ_POP_BACK(seq) MSGPACK_PP_SEQ_FIRST_N(MSGPACK_PP_DEC(MSGPACK_PP_SEQ_SIZE(seq)), seq)
# else
#    define MSGPACK_PP_SEQ_POP_BACK(seq) MSGPACK_PP_SEQ_POP_BACK_I(seq)
#    define MSGPACK_PP_SEQ_POP_BACK_I(seq) MSGPACK_PP_SEQ_FIRST_N(MSGPACK_PP_DEC(MSGPACK_PP_SEQ_SIZE(seq)), seq)
# endif
#
# endif
