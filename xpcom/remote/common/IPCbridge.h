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
#ifndef __IPCbridge_h__
#define __IPCbridge_h__

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

class IPCBridge {

    public:
        IPCBridge(const char* serverName = NULL);
        ~IPCBridge();
        unsigned int Read( void** );
        unsigned int Write( unsigned int, void* );
    private:
        const static char * PREFIX;
        int msg_handler;
        long this_side;
        long other_side;
        char *fname;
	
        char *get_name( pid_t );
        int initServer( void );
        int initClient( void );

};

#endif /* __IPCbridge_h__ */
