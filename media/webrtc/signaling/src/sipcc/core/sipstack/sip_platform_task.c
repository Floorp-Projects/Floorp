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
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
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
#include "phntask.h"
#include "phone_debug.h"
#include "util_string.h"
#include "ccsip_platform_tcp.h"
#include "ccsip_task.h"
#include "sip_socket_api.h"

/*---------------------------------------------------------
 *
 * Definitions
 *
 */

/* The maximum number of messages parsed from the message queue at one time */
#define MAX_SIP_MESSAGES 8

/* The maximum number of connections allowed */
#define MAX_SIP_CONNECTIONS (64 - 2)

/* SIP Message queue waiting thread and the main thread IPC names */
#ifdef __ANDROID__
#define SIP_MSG_IPC_PATH  "/data/data/com.cisco.telephony.provider/"
#else
#define SIP_MSG_IPC_PATH  "/tmp/"
#endif
#define SIP_MSG_SERV_NAME "SIP-Main"
#define SIP_MSG_CLNT_NAME "SIP-MsgQ"

#define SIP_PAUSE_WAIT_IPC_LISTEN_READY_TIME   50  /* 50ms. */
#define SIP_MAX_WAIT_FOR_IPC_LISTEN_READY    1200  /* 50 * 1200 = 1 minutes */


/*---------------------------------------------------------
 *
 * Local Variables
 *
 */
fd_set read_fds;
fd_set write_fds;
static cpr_socket_t listen_socket = INVALID_SOCKET;
static cpr_socket_t sip_ipc_serv_socket = INVALID_SOCKET;
static cpr_socket_t sip_ipc_clnt_socket = INVALID_SOCKET;
static boolean main_thread_ready = FALSE;
uint32_t nfds = 0;
sip_connection_t sip_conn;

/*
 * Internal message structure between main thread and 
 * message queue waiting thread.
 */
typedef struct sip_int_msg_t_ {
    void            *msg;
    phn_syshdr_t    *syshdr;   
} sip_int_msg_t;

/* Internal message queue (array) */
static sip_int_msg_t sip_int_msgq_buf[MAX_SIP_MESSAGES] = {{0,0},{0,0}};

/* Main thread and message queue waiting thread IPC names */
static const char *sip_IPC_serv_name = SIP_MSG_IPC_PATH SIP_MSG_SERV_NAME;
static const char *sip_IPC_clnt_name = SIP_MSG_IPC_PATH SIP_MSG_CLNT_NAME;
static cpr_sockaddr_un_t sip_serv_sock_addr; 
static cpr_sockaddr_un_t sip_clnt_sock_addr;


/*---------------------------------------------------------
 *
 * Global Variables
 *
 */
extern sipGlobal_t sip;
extern boolean  sip_reg_all_failed;


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
 *  sip_create_IPC_sock creates and bind the socket for IPC.
 *
 *  @param[in]name - pointer to the const. character for the
 *                  IPC address (name) to be bound when the
 *                  IPC socket is successfully created.
 *
 *  @return cpr_socket_t - Returns a valid CPR's socket if
 *                   the socket is created sucessafully otherwise
 *                   returns INVALID_SOCKET.
 *  @pre    (name != NULL)
 */
static cpr_socket_t sip_create_IPC_sock (const char *name)
{
    const char *fname = "sip_create_IPC_sock";
    cpr_socket_t sock;
    cpr_sockaddr_un_t addr;

    /* Create socket */
    sock = cprSocket(AF_LOCAL, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"cprSocket() returned error"
                          " errno=%d\n", fname, cpr_errno);
        return (INVALID_SOCKET);
    }

    /* Bind to the local socket */
    cpr_set_sockun_addr(&addr, name);

    /* make sure file doesn't already exist */
    unlink( (char *)addr.sun_path);
   
    /* do the bind */
    if (cprBind(sock, (cpr_sockaddr_t *)&addr,
                cpr_sun_len(addr)) == CPR_FAILURE) {
        (void) sipSocketClose(sock, FALSE);
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"cprBind() failed"
                          " errno=%d\n", fname, cpr_errno);
        return (INVALID_SOCKET);
    }
    return (sock);
}

/**
 *  The function is a thread loop that waits on SIP message queue
 *  visible for the external components (sip_msgq). The thread is used
 *  to avoid having the main task loop from having to set a small time
 *  waiting on select() to poll inter-thread messages. The small waiting
 *  time on select() to poll internal messages queue increases the
 *  unnecessary of waking up when system is idle. On the platform that
 *  power conservative is critical such as handheld phone, waking up
 *  periodically is not good for battery life.
 *
 *  This thread splits the message queue waiting function from the
 *  main thread waiting on select(). This thread simply listens on the
 *  internal message queue and signal to the main thread via IPC socket
 *  or local socket trigger. Therefore the main thread can uniformly
 *  process internal message event and the network event via select().
 *  The small internal poll time on select() can be
 *  avoided.
 *
 *  @param[in] arg - pointer to SIP main thread's message queue.
 *
 *  @return None.
 * 
 *  @pre     (arg != NULL) 
 */
void sip_platform_task_msgqwait (void *arg)
{
    const char    *fname = "sip_platform_task_msgqwait";
    cprMsgQueue_t *msgq = (cprMsgQueue_t *)arg;
    unsigned int  wait_main_thread = 0;
    phn_syshdr_t  *syshdr;
    void          *msg;
    uint8_t       num_messages = 0;
    uint8_t       response = 0;

    if (msgq == NULL) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"task msgq is null, exiting\n", fname);
        return;
    }

    if (platThreadInit("SIP IPCQ task") != 0) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"failed to attach thread to JVM\n", fname);
        return;
    }

    /*
     * Wait for SIP main thread ready for IPC connection.
     */
    while (!main_thread_ready) {
        /* Pause for other threads to run while waiting */
        cprSleep(SIP_PAUSE_WAIT_IPC_LISTEN_READY_TIME);

        wait_main_thread++;
        if (wait_main_thread > SIP_MAX_WAIT_FOR_IPC_LISTEN_READY) {
            /* Exceed the number of wait time */
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"timeout waiting for listening IPC"
                              " socket ready, exiting\n", fname);
            return;
        }
    }

    /*
     * Adjust relative priority of SIP thread.
     */
#ifndef WIN32
    (void) cprAdjustRelativeThreadPriority(SIP_THREAD_RELATIVE_PRIORITY);
#else
    /* Use default priority */
    (void) cprAdjustRelativeThreadPriority(0);
#endif
    
    /*
     * The main thread is ready. set global client socket address
     * so that the server can send back response. 
     */
    cpr_set_sockun_addr(&sip_clnt_sock_addr, sip_IPC_clnt_name);

    sip_ipc_clnt_socket = sip_create_IPC_sock(sip_IPC_clnt_name);

    if (sip_ipc_clnt_socket == INVALID_SOCKET) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sip_create_IPC_sock() failed,"
                          "  exiting\n", fname);
        return;
    }

    while (TRUE) {
        msg = cprGetMessage(msgq, TRUE, (void **) &syshdr);
        while (msg != NULL) {
            /*
             * There is a message to be forwarded to the main SIP
             * thread for processing.
             */
            sip_int_msgq_buf[num_messages].msg    = msg;
            sip_int_msgq_buf[num_messages].syshdr = syshdr;
            num_messages++;
            if (num_messages == MAX_SIP_MESSAGES) {
                /*
                 * Limit the number of messages passed to the main SIP
                 * thread to MAX_SIP_MESSAGES since SIP main thread only
                 * process messages up to MAX_SIP_MESSAGES at a time.
                 */
                break;
            }
            /*
             * Check to see if there is more message on the queue
             * before sending IPC message trigger to the main SIP
             * thread. This is to minimize the overhead of the
             * the main SIP thread in processing select().
             */
            msg = cprGetMessage(msgq, 0, (void **) &syshdr);
        }

        if (num_messages) {
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"%d msg available on msgq\n", DEB_F_PREFIX_ARGS(SIP_MSG_QUE, fname), num_messages);
            /*
             * There are some number of messages sent to the main thread,
             * trigger the main SIP thread via IPC to process the message.
             */
            if (cprSendTo(sip_ipc_clnt_socket, (void *)&num_messages,
                          sizeof(num_messages), 0,
                          (cpr_sockaddr_t *)&sip_serv_sock_addr, 
                          cpr_sun_len(sip_serv_sock_addr)) < 0) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"send IPC failed errno=%d\n", fname, cpr_errno);
            }

            /*
             * Wait for main thread to signal us to get more message.
             */
            if (cprRecvFrom(sip_ipc_clnt_socket, &response, 
                            sizeof(response), 0, NULL, NULL) < 0) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"read IPC failed:"
                                  " errno=%d\n", fname, cpr_errno);
            }
            num_messages = 0;
        }
    }
}

/**
 *  sip_process_int_msg - process internal IPC message from the
 *  the message queue waiting thread.
 *
 *  @param - none.
 *
 *  @return none.
 */
static void sip_process_int_msg (void)
{
    const char    *fname = "sip_process_int_msg";
    ssize_t       rcv_len;
    uint8_t       num_messages = 0;
    uint8_t       response = 0;
    sip_int_msg_t *int_msg;
    void          *msg;
    phn_syshdr_t  *syshdr;

    /* read the msg count from the IPC socket */
    rcv_len = cprRecvFrom(sip_ipc_serv_socket, &num_messages, 
                          sizeof(num_messages), 0, NULL, NULL);

    if (rcv_len < 0) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"read IPC failed:"
                          " errno=%d\n", fname, cpr_errno);
        return;
    }
     
    if (num_messages == 0) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"message queue is empty!\n", fname);
        return;
    }

    if (num_messages > MAX_SIP_MESSAGES) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"number of  messages on queue exceeds maximum %d\n", fname,
                          num_messages);
        num_messages = MAX_SIP_MESSAGES;
    }

    /* process messages */
    int_msg = &sip_int_msgq_buf[0];
    while (num_messages) {
        msg    = int_msg->msg;
        syshdr = int_msg->syshdr;
        if (msg != NULL && syshdr != NULL) {
            SIPTaskProcessListEvent(syshdr->Cmd, msg, syshdr->Usr.UsrPtr,
                syshdr->Len);
            cprReleaseSysHeader(syshdr);

            int_msg->msg    = NULL;
            int_msg->syshdr = NULL;
        }

        num_messages--;  /* one less message to work on */
        int_msg++;       /* advance to the next message */
    }

    /*
     * Signal message queue waiting thread to get more messages.
     */
    if (cprSendTo(sip_ipc_serv_socket, (void *)&response,
                  sizeof(response), 0,
                  (cpr_sockaddr_t *)&sip_clnt_sock_addr,
                  cpr_sun_len(sip_clnt_sock_addr)) < 0) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"%d sending IPC\n", fname);
    }
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
    uint16_t i;
    fd_set sip_read_fds;
    fd_set sip_write_fds;
    sip_tcp_conn_t *entry;

    sip_msgq = (cprMsgQueue_t) arg;
    if (!sip_msgq) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sip_msgq is null, exiting\n", fname);
        return;
    }
    sip.msgQueue = sip_msgq;

    sip_platform_task_init();
    /*
     * Initialize the SIP task
     */
    SIPTaskInit();

    if (platThreadInit("SIPStack Task") != 0) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"failed to attach thread to JVM\n", fname);
        return;
    }

    /*
     * Adjust relative priority of SIP thread.
     */
#ifndef WIN32
    (void) cprAdjustRelativeThreadPriority(SIP_THREAD_RELATIVE_PRIORITY);
#else
    /* Use default priority */
    (void) cprAdjustRelativeThreadPriority(0);
#endif

    /*
     * Setup IPC socket addresses for main thread (server)
     */ 
    cpr_set_sockun_addr(&sip_serv_sock_addr, sip_IPC_serv_name);

    /*
     * Create IPC between the message queue thread and this main
     * thread.
     */
    sip_ipc_serv_socket = sip_create_IPC_sock(sip_IPC_serv_name);

    if (sip_ipc_serv_socket == INVALID_SOCKET) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sip_create_IPC_sock() failed:"
                          " errno=%d\n", fname, cpr_errno);
        return;
    }

    /*
     * On Win32 platform, the random seed is stored per thread; therefore,
     * each thread needs to seed the random number.  It is recommended by
     * MS to do the following to ensure randomness across application
     * restarts.
     */
    cpr_srand((unsigned int)time(NULL));

    /*
     * Set read IPC socket
     */
    sip_platform_task_set_read_socket(sip_ipc_serv_socket);

    /*
     * Let the message queue waiting thread know that the main 
     * thread is ready.
     */
    main_thread_ready = TRUE;

    /*
     * Main Event Loop
     */
    while (TRUE) {
        /*
         * Wait on events or timeout
         */
        sip_read_fds = read_fds;

        // start off by init to zero
        FD_ZERO(&sip_write_fds);
        // now look for sockets where data has been queued up
        for (i = 0; i < MAX_CONNECTIONS; i++) {
            entry = sip_tcp_conn_tab + i;
            if (-1 != entry->fd && entry->sendQueue && sll_count(entry->sendQueue)) {
                FD_SET(entry->fd, &sip_write_fds);
            }
        }
        
        pending_operations = cprSelect((nfds + 1),
                                       &sip_read_fds,
                                       &sip_write_fds,
                                       NULL, NULL);
        if (pending_operations == SOCKET_ERROR) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"cprSelect() failed: errno=%d."
                " Recover by initiating sip restart\n",
                              fname, cpr_errno);
            /* 
             * If we have come here, then either read socket related to 
             * sip_ipc_serv_socket has got corrupted, or one of the write 
             * socket related to cucm tcp/tls connection.
             * We will recover, by first clearing all fds, then re-establishing
             * the connection with sip-msgq by listening on 
             * sip_ipc_serv_socket.
             */
            sip_platform_task_init(); /* this clear FDs */
            sip_platform_task_set_read_socket(sip_ipc_serv_socket); 
            
            /* 
             * Since all sockets fds have been cleared above, we can not anyway
             * send or receive msg from cucm. So, there is no point
             * trying to send registration cancel msg to cucm. Also, a
             * call may be active, and in that case we do not want to
             * un-register. So, by setting sip_reg_all_failed to true, we 
             * make sure that no registration cancelation attempt is made.
             */
            sip_reg_all_failed = TRUE;
            platform_reset_req(DEVICE_RESTART);
            continue;
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
             * Check IPC for internal message queue
             */
            if (FD_ISSET(sip_ipc_serv_socket, &sip_read_fds)) {
                /* read the message to flush the buffer */
                sip_process_int_msg();
                pending_operations--;
            }

            /*
             * Check all sockets for stuff to do
             */
            for (i = 0; ((i < MAX_SIP_CONNECTIONS) &&
                         (pending_operations > 0)); i++) {
                if ((sip_conn.read[i] != INVALID_SOCKET) &&
                    FD_ISSET(sip_conn.read[i], &sip_read_fds)) {
                    /*
                     * Assume tcp
                     */
                    sip_tcp_read_socket(sip_conn.read[i]);
                    pending_operations--;
                }
                if ((sip_conn.write[i] != INVALID_SOCKET) &&
                    FD_ISSET(sip_conn.write[i], &sip_write_fds)) {
                    int connid;

                    connid = sip_tcp_fd_to_connid(sip_conn.write[i]);
                    if (connid >= 0) {
                        sip_tcp_resend(connid);
                    }
                    pending_operations--;
                }
            }
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

