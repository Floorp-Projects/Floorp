/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2004 Carnegie Mellon University.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */
/*
 * cmd_ln.h -- Command line argument parsing.
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * 
 * 15-Jul-1997	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Added required arguments types.
 * 
 * 07-Dec-96	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Created, based on Eric's implementation.  Basically, combined several
 *		functions into one, eliminated validation, and simplified the interface.
 */


#ifndef _LIBUTIL_CMD_LN_H_
#define _LIBUTIL_CMD_LN_H_

#include <stdio.h>
#include <stdarg.h>

/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/prim_type.h>

/**
 * @file cmd_ln.h
 * @brief Command-line and other configurationparsing and handling.
 *  
 * Configuration parameters, optionally parsed from the command line.
 */
  

#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

/**
 * @struct arg_t
 * Argument definition structure.
 */
typedef struct arg_s {
	char const *name;   /**< Name of the command line switch */
	int type;           /**< Type of the argument in question */
	char const *deflt;  /**< Default value (as a character string), or NULL if none */
	char const *doc;    /**< Documentation/description string */
} arg_t;

/**
 * @name Values for arg_t::type
 */
/* @{ */
/**
 * Bit indicating a required argument.
 */
#define ARG_REQUIRED (1<<0)
/**
 * Integer argument (optional).
 */
#define ARG_INTEGER  (1<<1)
/**
 * Floating point argument (optional).
 */
#define ARG_FLOATING (1<<2)
/**
 * String argument (optional).
 */
#define ARG_STRING   (1<<3)
/**
 * Boolean (true/false) argument (optional).
 */
#define ARG_BOOLEAN  (1<<4)
/**
 * Boolean (true/false) argument (optional).
 */
#define ARG_STRING_LIST  (1<<5)

/**
 * Required integer argument.
 */
#define REQARG_INTEGER (ARG_INTEGER | ARG_REQUIRED)
/**
 * Required floating point argument.
 */
#define REQARG_FLOATING (ARG_FLOATING | ARG_REQUIRED)
/**
 * Required string argument.
 */
#define REQARG_STRING (ARG_STRING | ARG_REQUIRED)
/**
 * Required boolean argument.
 */
#define REQARG_BOOLEAN (ARG_BOOLEAN | ARG_REQUIRED)

/**
 * @deprecated Use ARG_INTEGER instead.
 */
#define ARG_INT32   ARG_INTEGER
/**
 * @deprecated Use ARG_FLOATING instead.
 */
#define ARG_FLOAT32 ARG_FLOATING
/**
 * @deprecated Use ARG_FLOATING instead.
 */
#define ARG_FLOAT64 ARG_FLOATING
/**
 * @deprecated Use REQARG_INTEGER instead.
 */
#define REQARG_INT32 (ARG_INT32 | ARG_REQUIRED)
/**
 * @deprecated Use REQARG_FLOATING instead.
 */
#define REQARG_FLOAT32 (ARG_FLOAT32 | ARG_REQUIRED)
/**
 * @deprecated Use REQARG_FLOATING instead.
 */
#define REQARG_FLOAT64 (ARG_FLOAT64 | ARG_REQUIRED)
/* @} */


/**
 * Helper macro to stringify enums and other non-string values for
 * default arguments.
 **/
#define ARG_STRINGIFY(s) ARG_STRINGIFY1(s)
#define ARG_STRINGIFY1(s) #s

/**
 * @struct cmd_ln_t
 * Opaque structure used to hold the results of command-line parsing.
 */
typedef struct cmd_ln_s cmd_ln_t;

/**
 * Create a cmd_ln_t from NULL-terminated list of arguments.
 *
 * This function creates a cmd_ln_t from a NULL-terminated list of
 * argument strings.  For example, to create the equivalent of passing
 * "-hmm foodir -dsratio 2 -lm bar.lm" on the command-line:
 *
 *  config = cmd_ln_init(NULL, defs, TRUE, "-hmm", "foodir", "-dsratio", "2",
 *                       "-lm", "bar.lm", NULL);
 *
 * Note that for simplicity, <strong>all</strong> arguments are passed
 * as strings, regardless of the actual underlying type.
 *
 * @param inout_cmdln Previous command-line to update, or NULL to create a new one.
 * @param defn Array of argument name definitions, or NULL to allow any arguments.
 * @param strict Whether to fail on duplicate or unknown arguments.
 * @return A cmd_ln_t* containing the results of command line parsing, or NULL on failure.
 */
SPHINXBASE_EXPORT
cmd_ln_t *cmd_ln_init(cmd_ln_t *inout_cmdln, arg_t const *defn, int32 strict, ...);

/**
 * Retain ownership of a command-line argument set.
 *
 * @return pointer to retained command-line argument set.
 */
SPHINXBASE_EXPORT
cmd_ln_t *cmd_ln_retain(cmd_ln_t *cmdln);

/**
 * Release a command-line argument set and all associated strings.
 *
 * @return new reference count (0 if freed completely)
 */
SPHINXBASE_EXPORT
int cmd_ln_free_r(cmd_ln_t *cmdln);

/**
 * Parse a list of strings into argumetns.
 *
 * Parse the given list of arguments (name-value pairs) according to
 * the given definitions.  Argument values can be retrieved in future
 * using cmd_ln_access().  argv[0] is assumed to be the program name
 * and skipped.  Any unknown argument name causes a fatal error.  The
 * routine also prints the prevailing argument values (to stderr)
 * after parsing.
 *
 * @note It is currently assumed that the strings in argv are
 *       allocated statically, or at least that they will be valid as
 *       long as the cmd_ln_t returned from this function.
 *       Unpredictable behaviour will result if they are freed or
 *       otherwise become invalidated.
 *
 * @return A cmd_ln_t containing the results of command line parsing,
 *         or NULL on failure.
 **/
SPHINXBASE_EXPORT
cmd_ln_t *cmd_ln_parse_r(cmd_ln_t *inout_cmdln, /**< In/Out: Previous command-line to update,
                                                     or NULL to create a new one. */
                         arg_t const *defn,	/**< In: Array of argument name definitions */
                         int32 argc,		/**< In: Number of actual arguments */
                         char *argv[],		/**< In: Actual arguments */
                         int32 strict           /**< In: Fail on duplicate or unknown
                                                   arguments, or no arguments? */
    );

/**
 * Parse an arguments file by deliminating on " \r\t\n" and putting each tokens
 * into an argv[] for cmd_ln_parse().
 *
 * @return A cmd_ln_t containing the results of command line parsing, or NULL on failure.
 */
SPHINXBASE_EXPORT
cmd_ln_t *cmd_ln_parse_file_r(cmd_ln_t *inout_cmdln, /**< In/Out: Previous command-line to update,
                                                     or NULL to create a new one. */
                              arg_t const *defn,   /**< In: Array of argument name definitions*/
                              char const *filename,/**< In: A file that contains all
                                                     the arguments */ 
                              int32 strict         /**< In: Fail on duplicate or unknown
                                                     arguments, or no arguments? */
    );

/**
 * Access the generic type union for a command line argument.
 */
SPHINXBASE_EXPORT
anytype_t *cmd_ln_access_r(cmd_ln_t *cmdln, char const *name);

/**
 * Retrieve a string from a command-line object.
 *
 * The command-line object retains ownership of this string, so you
 * should not attempt to free it manually.
 *
 * @param cmdln Command-line object.
 * @param name the command-line flag to retrieve.
 * @return the string value associated with <tt>name</tt>, or NULL if
 *         <tt>name</tt> does not exist.  You must use
 *         cmd_ln_exists_r() to distinguish between cases where a
 *         value is legitimately NULL and where the corresponding flag
 *         is unknown.
 */
SPHINXBASE_EXPORT
char const *cmd_ln_str_r(cmd_ln_t *cmdln, char const *name);

/**
 * Retrieve an array of strings from a command-line object.
 *
 * The command-line object retains ownership of this array, so you
 * should not attempt to free it manually.
 *
 * @param cmdln Command-line object.
 * @param name the command-line flag to retrieve.
 * @return the array of strings associated with <tt>name</tt>, or NULL if
 *         <tt>name</tt> does not exist.  You must use
 *         cmd_ln_exists_r() to distinguish between cases where a
 *         value is legitimately NULL and where the corresponding flag
 *         is unknown.
 */
SPHINXBASE_EXPORT
char const **cmd_ln_str_list_r(cmd_ln_t *cmdln, char const *name);

/**
 * Retrieve an integer from a command-line object.
 *
 * @param cmdln Command-line object.
 * @param name the command-line flag to retrieve.
 * @return the integer value associated with <tt>name</tt>, or 0 if
 *         <tt>name</tt> does not exist.  You must use
 *         cmd_ln_exists_r() to distinguish between cases where a
 *         value is legitimately zero and where the corresponding flag
 *         is unknown.
 */
SPHINXBASE_EXPORT
long cmd_ln_int_r(cmd_ln_t *cmdln, char const *name);

/**
 * Retrieve a floating-point number from a command-line object.
 *
 * @param cmdln Command-line object.
 * @param name the command-line flag to retrieve.
 * @return the float value associated with <tt>name</tt>, or 0.0 if
 *         <tt>name</tt> does not exist.  You must use
 *         cmd_ln_exists_r() to distinguish between cases where a
 *         value is legitimately zero and where the corresponding flag
 *         is unknown.
 */
SPHINXBASE_EXPORT
double cmd_ln_float_r(cmd_ln_t *cmdln, char const *name);

/**
 * Retrieve a boolean value from a command-line object.
 */
#define cmd_ln_boolean_r(c,n) (cmd_ln_int_r(c,n) != 0)

/**
 * Set a string in a command-line object.
 *
 * @param cmdln Command-line object.
 * @param name The command-line flag to set.
 * @param str String value to set.  The command-line object does not
 *            retain ownership of this pointer.
 */
SPHINXBASE_EXPORT
void cmd_ln_set_str_r(cmd_ln_t *cmdln, char const *name, char const *str);

/**
 * Set an integer in a command-line object.
 *
 * @param cmdln Command-line object.
 * @param name The command-line flag to set.
 * @param iv Integer value to set.
 */
SPHINXBASE_EXPORT
void cmd_ln_set_int_r(cmd_ln_t *cmdln, char const *name, long iv);

/**
 * Set a floating-point number in a command-line object.
 *
 * @param cmdln Command-line object.
 * @param name The command-line flag to set.
 * @param fv Integer value to set.
 */
SPHINXBASE_EXPORT
void cmd_ln_set_float_r(cmd_ln_t *cmdln, char const *name, double fv);

/**
 * Set a boolean value in a command-line object.
 */
#define cmd_ln_set_boolean_r(c,n,b) (cmd_ln_set_int_r(c,n,(b)!=0))

/*
 * Compatibility macros
 */
#define cmd_ln_int32_r(c,n)	(int32)cmd_ln_int_r(c,n)
#define cmd_ln_float32_r(c,n)	(float32)cmd_ln_float_r(c,n)
#define cmd_ln_float64_r(c,n)	(float64)cmd_ln_float_r(c,n)
#define cmd_ln_set_int32_r(c,n,i)   cmd_ln_set_int_r(c,n,i)
#define cmd_ln_set_float32_r(c,n,f) cmd_ln_set_float_r(c,n,(double)f)
#define cmd_ln_set_float64_r(c,n,f) cmd_ln_set_float_r(c,n,(double)f)

/**
 * Re-entrant version of cmd_ln_exists().
 *
 * @return True if the command line argument exists (i.e. it
 * was one of the arguments defined in the call to cmd_ln_parse_r().
 */
SPHINXBASE_EXPORT
int cmd_ln_exists_r(cmd_ln_t *cmdln, char const *name);

/**
 * Print a help message listing the valid argument names, and the associated
 * attributes as given in defn.
 *
 * @param fp   output stream
 * @param defn Array of argument name definitions.
 */
SPHINXBASE_EXPORT
void cmd_ln_print_help_r (cmd_ln_t *cmdln, FILE *fp, const arg_t *defn);

/**
 * Non-reentrant version of cmd_ln_parse().
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_parse_r().
 * @return 0 if successful, <0 if error.
 */
SPHINXBASE_EXPORT
int32 cmd_ln_parse(const arg_t *defn,  /**< In: Array of argument name definitions */
                   int32 argc,	       /**< In: Number of actual arguments */
                   char *argv[],       /**< In: Actual arguments */
                   int32 strict        /**< In: Fail on duplicate or unknown
                                          arguments, or no arguments? */
	);

/**
 * Parse an arguments file by deliminating on " \r\t\n" and putting each tokens
 * into an argv[] for cmd_ln_parse().
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_parse_file_r().
 *
 * @return 0 if successful, <0 on error.
 */
SPHINXBASE_EXPORT
int32 cmd_ln_parse_file(const arg_t *defn,   /**< In: Array of argument name definitions*/
			char const *filename,/**< In: A file that contains all the arguments */ 
                        int32 strict         /**< In: Fail on duplicate or unknown
                                                arguments, or no arguments? */
	);

/**
 * Old application initialization routine for Sphinx3 code.
 *
 * @deprecated This is deprecated in favor of the re-entrant API.
 */
SPHINXBASE_EXPORT
void cmd_ln_appl_enter(int argc,   /**< In: Number of actual arguments */
		       char *argv[], /**< In: Number of actual arguments */
		       char const* default_argfn, /**< In: default argument file name*/
		       const arg_t *defn /**< Command-line argument definition */
	);


/**
 * Finalization routine corresponding to cmd_ln_appl_enter().
 *
 * @deprecated This is deprecated in favor of the re-entrant API.
 */

SPHINXBASE_EXPORT
void cmd_ln_appl_exit(void);

/**
 * Retrieve the global cmd_ln_t object used by non-re-entrant functions.
 *
 * @deprecated This is deprecated in favor of the re-entrant API.
 * @return global cmd_ln_t object.
 */
SPHINXBASE_EXPORT
cmd_ln_t *cmd_ln_get(void);

/**
 * Test the existence of a command-line argument in the global set of
 * definitions.
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_exists_r().
 *
 * @return True if the command line argument exists (i.e. it
 * was one of the arguments defined in the call to cmd_ln_parse().
 */
#define cmd_ln_exists(name)	cmd_ln_exists_r(cmd_ln_get(), name)

/**
 * Return a pointer to the previously parsed value for the given argument name.
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_access_r().
 */
#define cmd_ln_access(name)	cmd_ln_access_r(cmd_ln_get(), name)

/**
 * Retrieve a string from the global command line.
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_str_r().
 */
#define cmd_ln_str(name)	cmd_ln_str_r(cmd_ln_get(), name)

/**
 * Retrieve an array of strings in the global command line.
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_str_list_r().
 */
#define cmd_ln_str_list(name)	cmd_ln_str_list_r(cmd_ln_get(), name)

/**
 * Retrieve a 32-bit integer from the global command line.
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_int_r().
 */
#define cmd_ln_int32(name)	(int32)cmd_ln_int_r(cmd_ln_get(), name)
/**
 * Retrieve a 32-bit float from the global command line.
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_float_r().
 */
#define cmd_ln_float32(name)	(float32)cmd_ln_float_r(cmd_ln_get(), name)
/**
 * Retrieve a 64-bit float from the global command line.
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_float_r().
 */
#define cmd_ln_float64(name)	(float64)cmd_ln_float_r(cmd_ln_get(), name)
/**
 * Retrieve a boolean from the global command line.
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_boolean_r().
 */
#define cmd_ln_boolean(name)	cmd_ln_boolean_r(cmd_ln_get(), name)

/**
 * Set a string in the global command line.
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_set_str_r().
 */
#define cmd_ln_set_str(n,s)     cmd_ln_set_str_r(cmd_ln_get(),n,s)
/**
 * Set a 32-bit integer value in the global command line.
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_set_int_r().
 */
#define cmd_ln_set_int32(n,i)   cmd_ln_set_int_r(cmd_ln_get(),n,i)
/**
 * Set a 32-bit float in the global command line.
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_set_float_r().
 */
#define cmd_ln_set_float32(n,f) cmd_ln_set_float_r(cmd_ln_get(),n,f)
/**
 * Set a 64-bit float in the global command line.
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_set_float_r().
 */
#define cmd_ln_set_float64(n,f) cmd_ln_set_float_r(cmd_ln_get(),n,f)
/**
 * Set a boolean value in the global command line.
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_set_boolean_r().
 */
#define cmd_ln_set_boolean(n,b) cmd_ln_set_boolean_r(cmd_ln_get(),n,b)

/**
 * Print a help message listing the valid argument names, and the associated
 * attributes as given in defn.
 *
 * @deprecated This is deprecated in favor of the re-entrant API
 * function cmd_ln_print_help_r().
 */
#define cmd_ln_print_help(f,d) cmd_ln_print_help_r(cmd_ln_get(),f,d)

/**
 * Free the global command line, if any exists.
 * @deprecated Use the re-entrant API instead.
 */
SPHINXBASE_EXPORT
void cmd_ln_free (void);


#ifdef __cplusplus
}
#endif

#endif


