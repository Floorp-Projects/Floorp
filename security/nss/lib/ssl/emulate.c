/*
 * Functions that emulate PR_AcceptRead and PR_TransmitFile for SSL sockets.
 * Each Layered NSPR protocol (like SSL) must unfortunately contain its 
 * own implementation of these functions.  This code was taken from NSPR.
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nspr.h"

#if defined( XP_UNIX ) || defined( XP_BEOS )
#include <fcntl.h>
#endif
#if defined(WIN32)
#include <windef.h>
#include <winbase.h>
#endif
#include <string.h>

#define AMASK 7	/* mask for alignment of PRNetAddr */

/*
 * _PR_EmulateAcceptRead
 *
 *  Accept an incoming connection on sd, set *nd to point to the
 *  newly accepted socket, read 'amount' bytes from the accepted
 *  socket.
 *
 *  buf is a buffer of length = amount + (2 * sizeof(PRNetAddr)) + 32
 *  *raddr points to the PRNetAddr of the accepted connection upon
 *  return
 *
 *  return number of bytes read or -1 on error
 *
 */
PRInt32 
ssl_EmulateAcceptRead(	PRFileDesc *   sd, 
			PRFileDesc **  nd,
			PRNetAddr **   raddr, 
			void *         buf, 
			PRInt32        amount, 
			PRIntervalTime timeout)
{
    PRFileDesc *   newsockfd;
    PRInt32        rv;
    PRNetAddr      remote;

    if (!(newsockfd = PR_Accept(sd, &remote, PR_INTERVAL_NO_TIMEOUT))) {
	return -1;
    }

    rv = PR_Recv(newsockfd, buf, amount, 0, timeout);
    if (rv >= 0) {
	ptrdiff_t pNetAddr = (((ptrdiff_t)buf) + amount + AMASK) & ~AMASK;

	*nd = newsockfd;
	*raddr = (PRNetAddr *)pNetAddr;
	memcpy((void *)pNetAddr, &remote, sizeof(PRNetAddr));
	return rv;
    }

    PR_Close(newsockfd);
    return -1;
}


#if !defined( XP_UNIX ) && !defined( WIN32 ) && !defined( XP_BEOS )
/*
 * _PR_EmulateTransmitFile
 *
 *  Send file fd across socket sd. If headers is non-NULL, 'hlen'
 *  bytes of headers is sent before sending the file.
 *
 *  PR_TRANSMITFILE_CLOSE_SOCKET flag - close socket after sending file
 *
 *  return number of bytes sent or -1 on error
 *
 */
#define _TRANSMITFILE_BUFSIZE	(16 * 1024)

PRInt32 
ssl_EmulateTransmitFile(    PRFileDesc *        sd, 
			    PRFileDesc *        fd,
			    const void *        headers, 
			    PRInt32             hlen, 
			    PRTransmitFileFlags flags,
			    PRIntervalTime      timeout)
{
    char *  buf 	= NULL;
    PRInt32 count 	= 0;
    PRInt32 rlen;
    PRInt32 rv;

    buf = PR_MALLOC(_TRANSMITFILE_BUFSIZE);
    if (buf == NULL) {
	PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
	return -1;
    }

    /*
     * send headers, first
     */
    while (hlen) {
	rv =  PR_Send(sd, headers, hlen, 0, timeout);
	if (rv < 0) {
	    /* PR_Send() has invoked PR_SetError(). */
	    rv = -1;
	    goto done;
	} 
	count += rv;
	headers = (const void*) ((const char*)headers + rv);
	hlen -= rv;
    }
    /*
     * send file, next
     */
    while ((rlen = PR_Read(fd, buf, _TRANSMITFILE_BUFSIZE)) > 0) {
	while (rlen) {
	    char *bufptr = buf;

	    rv =  PR_Send(sd, bufptr, rlen,0,PR_INTERVAL_NO_TIMEOUT);
	    if (rv < 0) {
		/* PR_Send() has invoked PR_SetError(). */
		rv = -1;
		goto done;
	    } 
	    count += rv;
	    bufptr = ((char*)bufptr + rv);
	    rlen -= rv;
	}
    }
    if (rlen == 0) {
	/*
	 * end-of-file
	 */
	if (flags & PR_TRANSMITFILE_CLOSE_SOCKET)
	    PR_Close(sd);
	rv = count;
    } else {
	PR_ASSERT(rlen < 0);
	/* PR_Read() has invoked PR_SetError(). */
	rv = -1;
    }

done:
    if (buf)
	PR_DELETE(buf);
    return rv;
}
#else

#define TRANSMITFILE_MMAP_CHUNK (256 * 1024)

/*
 * _PR_UnixTransmitFile
 *
 *  Send file fd across socket sd. If headers is non-NULL, 'hlen'
 *  bytes of headers is sent before sending the file.
 *
 *  PR_TRANSMITFILE_CLOSE_SOCKET flag - close socket after sending file
 *
 *  return number of bytes sent or -1 on error
 *
 */

PRInt32 
ssl_EmulateTransmitFile(    PRFileDesc *        sd, 
			    PRFileDesc *        fd,
			    const void *        headers, 
			    PRInt32             hlen, 
			    PRTransmitFileFlags flags,
			    PRIntervalTime      timeout)
{
    void *            addr = NULL;
    PRFileMap *       mapHandle = NULL;
    PRInt32           count     = 0;
    PRInt32           index     = 0;
    PRInt32           len	= 0;
    PRInt32           rv;
    struct PRFileInfo info;
    struct PRIOVec    iov[2];

    /* Get file size */
    if (PR_SUCCESS != PR_GetOpenFileInfo(fd, &info)) {
	count = -1;
	goto done;
    }
    if (hlen) {
	iov[index].iov_base = (char *) headers;
	iov[index].iov_len  = hlen;
	index++;
    }
    if (info.size > 0) {
	mapHandle = PR_CreateFileMap(fd, info.size, PR_PROT_READONLY);
	if (mapHandle == NULL) {
	    count = -1;
	    goto done;
	}
	/*
	 * If the file is large, mmap and send the file in chunks so as
	 * to not consume too much virtual address space
	 */
	len = PR_MIN(info.size , TRANSMITFILE_MMAP_CHUNK );
	/*
	 * Map in (part of) file. Take care of zero-length files.
	 */
	if (len) {
	    addr = PR_MemMap(mapHandle, 0, len);
	    if (addr == NULL) {
		count = -1;
		goto done;
	    }
	}
	iov[index].iov_base = (char*)addr;
	iov[index].iov_len = len;
	index++;
    }
    if (!index)
    	goto done;
    rv = PR_Writev(sd, iov, index, timeout);
    if (len) {
	PR_MemUnmap(addr, len);
    }
    if (rv >= 0) {
	PR_ASSERT(rv == hlen + len);
	info.size -= len;
	count     += rv;
    } else {
	count = -1;
	goto done;
    }
    /*
     * send remaining bytes of the file, if any
     */
    len = PR_MIN(info.size , TRANSMITFILE_MMAP_CHUNK );
    while (len > 0) {
	/*
	 * Map in (part of) file
	 */
	PR_ASSERT((count - hlen) % TRANSMITFILE_MMAP_CHUNK == 0);
	addr = PR_MemMap(mapHandle, count - hlen, len);
	if (addr == NULL) {
	    count = -1;
	    goto done;
	}
	rv =  PR_Send(sd, addr, len, 0, timeout);
	PR_MemUnmap(addr, len);
	if (rv >= 0) {
	    PR_ASSERT(rv == len);
	    info.size -= rv;
	    count     += rv;
	    len = PR_MIN(info.size , TRANSMITFILE_MMAP_CHUNK );
	} else {
	    count = -1;
	    goto done;
	}
    }
done:
    if ((count >= 0) && (flags & PR_TRANSMITFILE_CLOSE_SOCKET))
	PR_Close(sd);
    if (mapHandle != NULL)
    	PR_CloseFileMap(mapHandle);
    return count;
}
#endif  /* XP_UNIX || WIN32 || XP_BEOS */




#if !defined( XP_UNIX ) && !defined( WIN32 ) && !defined( XP_BEOS )
/*
 * _PR_EmulateSendFile
 *
 *  Send file sfd->fd across socket sd. The header and trailer buffers
 *  specified in the 'sfd' argument are sent before and after the file,
 *  respectively.
 *
 *  PR_TRANSMITFILE_CLOSE_SOCKET flag - close socket after sending file
 *
 *  return number of bytes sent or -1 on error
 *
 */

PRInt32 
ssl_EmulateSendFile(PRFileDesc *sd, PRSendFileData *sfd,
                    PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    char *       buf = NULL;
    const void * buffer;
    PRInt32      rv;
    PRInt32      count = 0;
    PRInt32      rlen;
    PRInt32      buflen;
    PRInt32      sendbytes;
    PRInt32      readbytes;

#define _SENDFILE_BUFSIZE   (16 * 1024)

    buf = (char*)PR_MALLOC(_SENDFILE_BUFSIZE);
    if (buf == NULL) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return -1;
    }

    /*
     * send header, first
     */
    buflen = sfd->hlen;
    buffer = sfd->header;
    while (buflen) {
        rv =  PR_Send(sd, buffer, buflen, 0, timeout);
        if (rv < 0) {
            /* PR_Send() has invoked PR_SetError(). */
            rv = -1;
            goto done;
        } else {
            count += rv;
            buffer = (const void*) ((const char*)buffer + rv);
            buflen -= rv;
        }
    }
    /*
     * send file, next
     */

    if (PR_Seek(sfd->fd, sfd->file_offset, PR_SEEK_SET) < 0) {
        rv = -1;
        goto done;
    }
    sendbytes = sfd->file_nbytes;
    if (sendbytes == 0) {
        /* send entire file */
        while ((rlen = PR_Read(sfd->fd, buf, _SENDFILE_BUFSIZE)) > 0) {
            while (rlen) {
                char *bufptr = buf;

                rv =  PR_Send(sd, bufptr, rlen, 0, timeout);
                if (rv < 0) {
                    /* PR_Send() has invoked PR_SetError(). */
                    rv = -1;
                    goto done;
                } else {
                    count += rv;
                    bufptr = ((char*)bufptr + rv);
                    rlen -= rv;
                }
            }
        }
        if (rlen < 0) {
            /* PR_Read() has invoked PR_SetError(). */
            rv = -1;
            goto done;
        }
    } else {
        readbytes = PR_MIN(sendbytes, _SENDFILE_BUFSIZE);
        while (readbytes && ((rlen = PR_Read(sfd->fd, buf, readbytes)) > 0)) {
            while (rlen) {
                char *bufptr = buf;

                rv =  PR_Send(sd, bufptr, rlen, 0, timeout);
                if (rv < 0) {
                    /* PR_Send() has invoked PR_SetError(). */
                    rv = -1;
                    goto done;
                } else {
                    count += rv;
                    sendbytes -= rv;
                    bufptr = ((char*)bufptr + rv);
                    rlen -= rv;
                }
            }
	    readbytes = PR_MIN(sendbytes, _SENDFILE_BUFSIZE);
        }
        if (rlen < 0) {
            /* PR_Read() has invoked PR_SetError(). */
            rv = -1;
            goto done;
        } else if (sendbytes != 0) {
            /*
             * there are fewer bytes in file to send than specified
             */
	    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            rv = -1;
            goto done;
        }
    }
    /*
     * send trailer, last
     */
    buflen = sfd->tlen;
    buffer = sfd->trailer;
    while (buflen) {
        rv =  PR_Send(sd, buffer, buflen, 0, timeout);
        if (rv < 0) {
            /* PR_Send() has invoked PR_SetError(). */
            rv = -1;
            goto done;
        } else {
            count += rv;
            buffer = (const void*) ((const char*)buffer + rv);
            buflen -= rv;
        }
    }
    rv = count;

done:
    if (buf)
        PR_DELETE(buf);
    if ((rv >= 0) && (flags & PR_TRANSMITFILE_CLOSE_SOCKET))
	PR_Close(sd);
    return rv;
}

#else /* UNIX, NT, and BEOS handled below */

/*
 * _PR_UnixSendFile
 *
 *    Send file sfd->fd across socket sd. If header/trailer are specified
 *    they are sent before and after the file, respectively.
 *
 *    PR_TRANSMITFILE_CLOSE_SOCKET flag - close socket after sending file
 *
 *    return number of bytes sent or -1 on error
 *
 */
#define SENDFILE_MMAP_CHUNK    (256 * 1024)

PRInt32 
ssl_EmulateSendFile(PRFileDesc *sd, PRSendFileData *sfd,
                    PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    void *            addr = NULL;
    PRFileMap *       mapHandle  	= NULL;
    PRInt32           count 		= 0;
    PRInt32           file_bytes;
    PRInt32           index 		= 0;
    PRInt32           len;
    PRInt32           rv;
    PRUint32          addr_offset;
    PRUint32          file_mmap_offset;
    PRUint32          mmap_len;
    PRUint32          pagesize;
    struct PRFileInfo info;
    struct PRIOVec    iov[3];

    /* Get file size */
    if (PR_SUCCESS != PR_GetOpenFileInfo(sfd->fd, &info)) {
	count = -1;
	goto done;
    }
    if (sfd->file_nbytes && 
		(info.size < (sfd->file_offset + sfd->file_nbytes))) {
        /*
         * there are fewer bytes in file to send than specified
         */
	PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        count = -1;
        goto done;
    }
    if (sfd->file_nbytes)
        file_bytes = sfd->file_nbytes;
    else
        file_bytes = info.size - sfd->file_offset;

#if defined(WIN32)
    {
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        pagesize = sysinfo.dwAllocationGranularity;
    }
#else
    pagesize = PR_GetPageSize();
#endif
    /*
     * If the file is large, mmap and send the file in chunks so as
     * to not consume too much virtual address space
     */
    if (!sfd->file_offset || !(sfd->file_offset & (pagesize - 1))) {
        /*
         * case 1: page-aligned file offset
         */
        mmap_len = PR_MIN(file_bytes, SENDFILE_MMAP_CHUNK);
        len      = mmap_len;
        file_mmap_offset = sfd->file_offset;
        addr_offset = 0;
    } else {
        /*
         * case 2: non page-aligned file offset
         */
        /* find previous page boundary */
        file_mmap_offset = (sfd->file_offset & ~(pagesize - 1));

        /* number of initial bytes to skip in mmap'd segment */
        addr_offset = sfd->file_offset - file_mmap_offset;
        PR_ASSERT(addr_offset > 0);
        mmap_len = PR_MIN(file_bytes + addr_offset, SENDFILE_MMAP_CHUNK);
        len      = mmap_len - addr_offset;
    }
    /*
     * OK I've convinced myself that length has to be possitive (file_bytes is 
     * negative or  SENDFILE_MMAP_CHUNK is less than pagesize). Just assert 
     * that this is the case so we catch problems in debug builds.
     */
    PR_ASSERT(len >= 0);

    /*
     * Map in (part of) file. Take care of zero-length files.
     */
    if (len > 0) {
	mapHandle = PR_CreateFileMap(sfd->fd, info.size, PR_PROT_READONLY);
	if (!mapHandle) {
	    count = -1;
	    goto done;
	}
	addr = PR_MemMap(mapHandle, file_mmap_offset, mmap_len);
        if (!addr) {
            count = -1;
            goto done;
        }
    }
    /*
     * send headers, first, followed by the file
     */
    if (sfd->hlen) {
        iov[index].iov_base = (char *) sfd->header;
        iov[index].iov_len  = sfd->hlen;
        index++;
    }
    if (len) {
        iov[index].iov_base = (char*)addr + addr_offset;
        iov[index].iov_len  = len;
        index++;
    }
    if ((file_bytes == len) && (sfd->tlen)) {
        /*
         * all file data is mapped in; send the trailer too
         */
        iov[index].iov_base = (char *) sfd->trailer;
        iov[index].iov_len  = sfd->tlen;
        index++;
    }
    rv = PR_Writev(sd, iov, index, timeout);
    if (len)
        PR_MemUnmap(addr, mmap_len);
    if (rv < 0) {
        count = -1;
        goto done;
    }

    PR_ASSERT(rv == sfd->hlen + len + ((len == file_bytes) ? sfd->tlen : 0));

    file_bytes -= len;
    count      += rv;
    if (!file_bytes)    /* header, file and trailer are sent */
	goto done;

    /*
     * send remaining bytes of the file, if any
     */
    len = PR_MIN(file_bytes, SENDFILE_MMAP_CHUNK);
    while (len > 0) {
	/*
	 * Map in (part of) file
	 */
	file_mmap_offset = sfd->file_offset + count - sfd->hlen;
	PR_ASSERT((file_mmap_offset % pagesize) == 0);

	addr = PR_MemMap(mapHandle, file_mmap_offset, len);
        if (!addr) {
            count = -1;
            goto done;
        }
	rv = PR_Send(sd, addr, len, 0, timeout);
	PR_MemUnmap(addr, len);
	if (rv < 0) {
	    count = -1;
	    goto done;
	}

	PR_ASSERT(rv == len);
	file_bytes -= rv;
	count      += rv;
	len = PR_MIN(file_bytes, SENDFILE_MMAP_CHUNK);
    }
    PR_ASSERT(0 == file_bytes);
    if (sfd->tlen) {
        rv =  PR_Send(sd, sfd->trailer, sfd->tlen, 0, timeout);
        if (rv >= 0) {
            PR_ASSERT(rv == sfd->tlen);
            count += rv;
        } else
            count = -1;
    }
done:
    if (mapHandle)
    	PR_CloseFileMap(mapHandle);
    if ((count >= 0) && (flags & PR_TRANSMITFILE_CLOSE_SOCKET))
	PR_Close(sd);
    return count;
}
#endif /* UNIX, NT, and BEOS */
