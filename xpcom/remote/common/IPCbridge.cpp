/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <string.h>
#include "IPCbridge.h"

#define SERVER 1L
#define CLIENT 2L

const char * IPCBridge::PREFIX = "/tmp/.msg";
struct data_info {
    long dataId;
    size_t dataSize;
    int dataBlock;
};


// constructor for both
IPCBridge::IPCBridge( const char* serverName)
{    
    if (serverName) {
	fname = (char *)malloc(strlen(serverName) + strlen(PREFIX) + 1);
	sprintf(fname,"%s%s",PREFIX,serverName);
	initClient();
    } else {
	initServer();

    }
}

// destructor
IPCBridge::~IPCBridge()
{
    switch( this_side )
    {
        case SERVER:
            // remove msg queue
            msgctl(msg_handler, IPC_RMID, NULL);
            // remove file
            unlink(fname);
        case CLIENT:
            // free memory
            free(fname);
            break;
    }
}

unsigned int IPCBridge::Read( void **data )
{
    int status;
    void *shmem;
    struct data_info di;

    /* read message about array */
    status = msgrcv(msg_handler, &di, sizeof(struct data_info), other_side, IPC_NOWAIT);
    if( status==-1 ) return 0;

    /* attach shared memory block */
    shmem = shmat(di.dataBlock, NULL, 0);

    /* copy array to local memory */
    *data = malloc(di.dataSize);
    memcpy(*data, shmem, di.dataSize);

    /* destroy shared memory block */
    shmdt((char *)shmem);
    status = shmctl(di.dataBlock, IPC_RMID, NULL);

    /* return data */
    return di.dataSize;
}

unsigned int IPCBridge::Write( unsigned int size, void *data )
{
    int status;
    int shm_handler;
    void *shmem;
    struct data_info di;

    /* create and attach shared memory block */
    shm_handler = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);
    shmem = shmat(shm_handler, NULL, 0);

    /* copy array from local memory */
    memcpy(shmem, data, size);
    shmdt((char *)shmem);

    /* create and send message about array */
    di.dataId = this_side;
    di.dataSize = size;
    di.dataBlock = shm_handler;
    status = msgsnd(msg_handler, &di, sizeof(struct data_info), IPC_NOWAIT);
    if( status==-1 )
    {
        shmctl(shm_handler, IPC_RMID, NULL);
        return 0;
    }

    /* return */
    return size;
}

int IPCBridge::initServer( void )
{
    int status;
    int fs;

    /* create exchange file               */
    /* name is /tmp/.mgg<pid_in_hex_mode> */
    fname = get_name(getpid());
    fs = open(fname, O_RDWR | O_CREAT | O_TRUNC, 0600);
    printf("--InitServer %s\n",fname);
    /* create message queue */
    msg_handler = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    if (msg_handler < 0) {
	printf("--IPCBridge::initServer failed. status\n");
	perror(NULL);
    }
    this_side = SERVER;
    other_side = CLIENT;

    /* wrtie handler into exchange file */
    status = write(fs, &msg_handler, sizeof(int));

    close(fs);
    return status;
}

int IPCBridge::initClient( void )
{
    int status;
    int fs;

    /* create exchange filename                  */
    /* name is /tmp/.msg<parent_pid_in_hex_mode> */


    /* get message handler */
    fs = open(fname,O_RDONLY);
    status = read(fs,&msg_handler,sizeof(int));
    this_side = CLIENT;
    other_side = SERVER;

    close(fs);
    return status;
}

char *IPCBridge::get_name( pid_t pid )
{
    char * p = (char *)malloc(100);
    sprintf(p,"%s%x",PREFIX,pid);
    printf("--get_name %s\n",p);
    return p;
}
