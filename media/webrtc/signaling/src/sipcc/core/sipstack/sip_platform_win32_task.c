/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_ipc.h"
#include "cpr_errno.h"
#include "cpr_socket.h"
#include "cpr_in.h"
#include "cpr_rand.h"
#include "cpr_string.h"
#include "cpr_threads.h"
#include "ccsip_core.h"
#include "ccsip_task.h"
#include "sip_platform_task.h"
#include "ccsip_platform_udp.h"
#include "sip_common_transport.h"
#include "sip_interface_regmgr.h"
#include "phntask.h"
#include "phone_debug.h"
#include "util_string.h"
#include "ccsip_platform_tcp.h"
#include "ccsip_task.h"

/*---------------------------------------------------------
 *
 * Definitions
 *
 */

/* The maximum number of messages parsed from the message queue at one time */
#define MAX_SIP_MESSAGES 8

/* The maximum number of connections allowed */
#define MAX_SIP_CONNECTIONS (64 - 2)

/* The socket select waiting time out values */
#define SIP_SELECT_NORMAL_TIMEOUT   25000    /* normal select timeout in usec*/
#define SIP_SELECT_QUICK_TIMEOUT        0    /* quick select timeout in usec */

/*---------------------------------------------------------
 *
 * Local Variables
 *
 */
fd_set read_fds;
fd_set write_fds;
static cpr_socket_t listen_socket = INVALID_SOCKET;
uint32_t nfds = 0;
sip_connection_t sip_conn;


/*---------------------------------------------------------
 *
 * Global Variables
 *
 */
extern sipGlobal_t sip;


/*---------------------------------------------------------
 *
 * Function declarations
 *
 */
//static void write_to_socket(cpr_socket_t s);
//static int read_socket(cpr_socket_t s);


/*---------------------------------------------------------
 *
 * Functions
 *
 */

/**
 *
 * sip_platform_task_init
 *
 * Initialize the SIP Task
 *
 * Parameters:   None
 *
 * Return Value: None
 *
 */
static void
sip_platform_task_init (void)
{
    uint16_t i;

    for (i = 0; i < MAX_SIP_CONNECTIONS; i++) {
        sip_conn.read[i] = INVALID_SOCKET;
        sip_conn.write[i] = INVALID_SOCKET;
    }

    /*
     * Initialize cprSelect call parameters
     */
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    return;
}


/**
 *
 * sip_platform_task_loop
 *
 * Run the SIP task
 *
 * Parameters: arg - SIP message queue
 *
 * Return Value: None
 *
 */
void
sip_platform_task_loop (void *arg)
{
    static const char *fname = "sip_platform_task_loop";
    int pending_operations;
    struct cpr_timeval timeout;
    void *msg;
    uint32_t cmd;
    uint16_t len;
    void *usr;
    phn_syshdr_t *syshdr;
    uint16_t i;
    fd_set sip_read_fds;
//    fd_set sip_write_fds;

    sip_msgq = (cprMsgQueue_t) arg;
    if (!sip_msgq) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sip_msgq is null, exiting", fname);
        return;
    }
    sip.msgQueue = sip_msgq;

    sip_platform_task_init();
    /*
     * Initialize the SIP task
     */
    SIPTaskInit();

    if (platThreadInit("sip_platform_task_loop") != 0) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"failed to attach thread to JVM", fname);
        return;
    }

    /*
     * Adjust relative priority of SIP thread.
     */
    (void) cprAdjustRelativeThreadPriority(SIP_THREAD_RELATIVE_PRIORITY);

    /*
     * Set the SIP task timeout at 25 milliseconds
     */
    timeout.tv_sec  = 0;
    timeout.tv_usec = SIP_SELECT_NORMAL_TIMEOUT;

    /*
     * On Win32 platform, the random seed is stored per thread; therefore,
     * each thread needs to seed the random number.  It is recommended by
     * MS to do the following to ensure randomness across application
     * restarts.
     */
    cpr_srand((unsigned int)time(NULL));

    /*
     * Main Event Loop
     */
    while (TRUE) {
        /*
         * Wait on events or timeout
         */
        sip_read_fds = read_fds;

//        sip_write_fds = write_fds;
        pending_operations = cprSelect((nfds + 1),
                                       &sip_read_fds,
                                       NULL,
                                       NULL, &timeout);
        if (pending_operations == SOCKET_ERROR) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"cprSelect() failed: errno=%d",
                              fname, cpr_errno);
        } else if (pending_operations) {
            /*
             * Listen socket is set only if UDP transport has been
             * configured. So see if the select return was for read
             * on the listen socket.
             */
            if ((listen_socket != INVALID_SOCKET) &&
                (sip.taskInited == TRUE) &&
                FD_ISSET(listen_socket, &sip_read_fds)) {
                sip_platform_udp_read_socket(listen_socket);
                pending_operations--;
            }
            /*
             * Check all sockets for stuff to do
             */
            for (i = 0; ((i < MAX_SIP_CONNECTIONS) &&
                         (pending_operations != 0)); i++) {
                if ((sip_conn.read[i] != INVALID_SOCKET) &&
                    FD_ISSET(sip_conn.read[i], &sip_read_fds)) {
                    /*
                     * Assume tcp
                     */
                    sip_tcp_read_socket(sip_conn.read[i]);
                    pending_operations--;
                }
				/*
                if ((sip_conn.write[i] != INVALID_SOCKET) &&
                    FD_ISSET(sip_conn.write[i], &sip_write_fds)) {
                    int connid;

                    connid = sip_tcp_fd_to_connid(sip_conn.write[i]);
                    if (connid >= 0) {
                        sip_tcp_resend(connid);
                    }
                    pending_operations--;
                }
				*/
            }
        }

        /*
         * Process all messages on the message queue
         * (e.g. timer callbacks)
         */
        i = 0;
        while (i++ < MAX_SIP_MESSAGES) {
            msg = cprGetMessage(sip_msgq, FALSE, (void **) &syshdr);
            if (msg != NULL) {
                cmd = syshdr->Cmd;
                len = syshdr->Len;
                usr = syshdr->Usr.UsrPtr;
                SIPTaskProcessListEvent(cmd, msg, usr, len);
                cprReleaseSysHeader(syshdr);
                syshdr = NULL;
            } else {
                /* Stop checking for msgs if the queue is empty */
                if (syshdr != NULL) {
                    cprReleaseSysHeader(syshdr);
                    syshdr = NULL;
                }
                break;
            }
        }

        /*
         * If message servicing loop reaches the maximum loop limit
         * it is possible that there are more messages left on the message
         * queue. Shorten the socket select time out to come back and
         * check the message queue for the message that might be left on
         * the queue.
         */
        if (i >= MAX_SIP_MESSAGES) {
            timeout.tv_usec = SIP_SELECT_QUICK_TIMEOUT;
        } else {
            /* set normal time out value to poll msg. queue */
            timeout.tv_usec = SIP_SELECT_NORMAL_TIMEOUT;
        }
    }
}

/**
 *
 * sip_platform_task_set_listen_socket
 *
 * Mark the socket for cpr_select to be read
 *
 * Parameters:   s - the socket
 *
 * Return Value: None
 *
 */
void
sip_platform_task_set_listen_socket (cpr_socket_t s)
{
    listen_socket = s;
    sip_platform_task_set_read_socket(s);
}

/**
 *
 * sip_platform_task_set_read_socket
 *
 * Mark the socket for cpr_select to be read
 *
 * Parameters:   s - the socket
 *
 * Return Value: None
 *
 */
void
sip_platform_task_set_read_socket (cpr_socket_t s)
{
    if (s != INVALID_SOCKET) {
        FD_SET(s, &read_fds);
        nfds = MAX(nfds, (uint32_t)s);
    }
}

/**
 *
 * sip_platform_task_reset_listen_socket
 *
 * Mark the socket  as INVALID
 *
 * Parameters:   s - the socket
 *
 * Return Value: None
 *
 */
void
sip_platform_task_reset_listen_socket (cpr_socket_t s)
{
    sip_platform_task_clr_read_socket(s);
    listen_socket = INVALID_SOCKET;
}

/**
 *
 * sip_platform_task_clr_read_socket
 *
 * Mark the socket for cpr_select to be read
 *
 * Parameters:   s - the socket
 *
 * Return Value: None
 *
 */
void
sip_platform_task_clr_read_socket (cpr_socket_t s)
{
    if (s != INVALID_SOCKET) {
        FD_CLR(s, &read_fds);
    }
}

