/*
 * err.h
 * 
 * error status codes
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */
/*
 *	
 * Copyright (c) 2001-2006, Cisco Systems, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * 
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#ifndef ERR_H
#define ERR_H

#include "datatypes.h"

/**
 * @defgroup Error Error Codes
 * 
 * Error status codes are represented by the enumeration err_status_t.
 * 
 * @{
 */


/*
 * @brief err_status_t defines error codes.
 *
 * The enumeration err_status_t defines error codes.  Note that the
 * value of err_status_ok is equal to zero, which can simplify error
 * checking somewhat.
 *
 */
typedef enum {
  err_status_ok           = 0,  /**< nothing to report                       */
  err_status_fail         = 1,  /**< unspecified failure                     */
  err_status_bad_param    = 2,  /**< unsupported parameter                   */
  err_status_alloc_fail   = 3,  /**< couldn't allocate memory                */
  err_status_dealloc_fail = 4,  /**< couldn't deallocate properly            */
  err_status_init_fail    = 5,  /**< couldn't initialize                     */
  err_status_terminus     = 6,  /**< can't process as much data as requested */
  err_status_auth_fail    = 7,  /**< authentication failure                  */
  err_status_cipher_fail  = 8,  /**< cipher failure                          */
  err_status_replay_fail  = 9,  /**< replay check failed (bad index)         */
  err_status_replay_old   = 10, /**< replay check failed (index too old)     */
  err_status_algo_fail    = 11, /**< algorithm failed test routine           */
  err_status_no_such_op   = 12, /**< unsupported operation                   */
  err_status_no_ctx       = 13, /**< no appropriate context found            */
  err_status_cant_check   = 14, /**< unable to perform desired validation    */
  err_status_key_expired  = 15, /**< can't use key any more                  */
  err_status_socket_err   = 16, /**< error in use of socket                  */
  err_status_signal_err   = 17, /**< error in use POSIX signals              */
  err_status_nonce_bad    = 18, /**< nonce check failed                      */
  err_status_read_fail    = 19, /**< couldn't read data                      */
  err_status_write_fail   = 20, /**< couldn't write data                     */
  err_status_parse_err    = 21, /**< error pasring data                      */
  err_status_encode_err   = 22, /**< error encoding data                     */
  err_status_semaphore_err = 23,/**< error while using semaphores            */
  err_status_pfkey_err    = 24  /**< error while using pfkey                 */
} err_status_t;

/**
 * @}
 */

typedef enum {
  err_level_emergency = 0,
  err_level_alert,
  err_level_critical,
  err_level_error,
  err_level_warning,
  err_level_notice,
  err_level_info,
  err_level_debug,
  err_level_none
} err_reporting_level_t;

/*
 * err_reporting_init prepares the error system.  If
 * ERR_REPORTING_SYSLOG is defined, it will open syslog.
 *
 * The ident argument is a string that will be prepended to
 * all syslog messages.  It is conventionally argv[0].
 */

err_status_t
err_reporting_init(char *ident);

#ifdef SRTP_KERNEL_LINUX
extern err_reporting_level_t err_level;
#else

/*
 * keydaemon_report_error reports a 'printf' formatted error
 * string, followed by a an arg list.  The priority argument
 * is equivalent to that defined for syslog.
 *
 * Errors will be reported to ERR_REPORTING_FILE, if defined, and to
 * syslog, if ERR_REPORTING_SYSLOG is defined.
 *
 */

void
err_report(int priority, char *format, ...);
#endif /* ! SRTP_KERNEL_LINUX */


/*
 * debug_module_t defines a debug module
 */

typedef struct {
  int   on;          /* 1 if debugging is on, 0 if it is off */
  char *name;        /* printable name for debug module      */
} debug_module_t;

#ifdef ENABLE_DEBUGGING

#define debug_on(mod)  (mod).on = 1

#define debug_off(mod) (mod).on = 0

/* use err_report() to report debug message */
#define debug_print(mod, format, arg)                  \
  if (mod.on) err_report(err_level_debug, ("%s: " format "\n"), mod.name, arg)
#define debug_print2(mod, format, arg1,arg2)                  \
  if (mod.on) err_report(err_level_debug, ("%s: " format "\n"), mod.name, arg1,arg2)

#else

/* define macros to do nothing */
#define debug_print(mod, format, arg)

#define debug_on(mod)

#define debug_off(mod)

#endif

#endif /* ERR_H */
