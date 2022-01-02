# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Edward Diener 2013.
#  *     Distributed under the Boost Software License, Version 1.0. (See
#  *     accompanying file LICENSE_1_0.txt or copy at
#  *     http://www.boost.org/LICENSE_1_0.txt)
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef MSGPACK_PREPROCESSOR_TUPLE_POP_BACK_HPP
# define MSGPACK_PREPROCESSOR_TUPLE_POP_BACK_HPP
#
# include <msgpack/preprocessor/config/config.hpp>
#
# if MSGPACK_PP_VARIADICS
#
# include <msgpack/preprocessor/array/pop_back.hpp>
# include <msgpack/preprocessor/array/to_tuple.hpp>
# include <msgpack/preprocessor/comparison/greater.hpp>
# include <msgpack/preprocessor/control/iif.hpp>
# include <msgpack/preprocessor/tuple/size.hpp>
# include <msgpack/preprocessor/tuple/to_array.hpp>
#
# /* MSGPACK_PP_TUPLE_POP_BACK */
#
# define MSGPACK_PP_TUPLE_POP_BACK(tuple) \
	MSGPACK_PP_IIF \
		( \
		MSGPACK_PP_GREATER(MSGPACK_PP_TUPLE_SIZE(tuple),1), \
		MSGPACK_PP_TUPLE_POP_BACK_EXEC, \
		MSGPACK_PP_TUPLE_POP_BACK_RETURN \
		) \
	(tuple) \
/**/
#
# define MSGPACK_PP_TUPLE_POP_BACK_EXEC(tuple) \
	MSGPACK_PP_ARRAY_TO_TUPLE(MSGPACK_PP_ARRAY_POP_BACK(MSGPACK_PP_TUPLE_TO_ARRAY(tuple))) \
/**/
#
# define MSGPACK_PP_TUPLE_POP_BACK_RETURN(tuple) tuple
#
# /* MSGPACK_PP_TUPLE_POP_BACK_Z */
#
# define MSGPACK_PP_TUPLE_POP_BACK_Z(z, tuple) \
	MSGPACK_PP_IIF \
		( \
		MSGPACK_PP_GREATER(MSGPACK_PP_TUPLE_SIZE(tuple),1), \
		MSGPACK_PP_TUPLE_POP_BACK_Z_EXEC, \
		MSGPACK_PP_TUPLE_POP_BACK_Z_RETURN \
		) \
	(z, tuple) \
/**/
#
# define MSGPACK_PP_TUPLE_POP_BACK_Z_EXEC(z, tuple) \
	MSGPACK_PP_ARRAY_TO_TUPLE(MSGPACK_PP_ARRAY_POP_BACK_Z(z, MSGPACK_PP_TUPLE_TO_ARRAY(tuple))) \
/**/
#
# define MSGPACK_PP_TUPLE_POP_BACK_Z_RETURN(z, tuple) tuple
#
# endif // MSGPACK_PP_VARIADICS
#
# endif // MSGPACK_PREPROCESSOR_TUPLE_POP_BACK_HPP
