/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* cvextcon.c --- using external Unix programs as content-encoding filters.
 */

#include "xp.h"
#include "plstr.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"
#include "cvextcon.h"
#include "mkformat.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#ifdef __sgi
#include <bstring.h>			/* FD_ZERO uses bzero() which needs this */
								/* file for its prototype. */
#endif

typedef struct _CVG_DataObject {
  NET_StreamClass *next_stream;	/* Where the output goes */
  pid_t pid;					/* process in which the filter is running */
  int infd;						/* for reading from the process */
  int outfd;					/* for writing to the process */
  struct sigaction oldact;		/* Old SIGCHLD handler */
} CVG_DataObject;


PRIVATE int net_ExtConverterRead (CVG_DataObject *data, PRBool block_p)
{
  char input_buffer [1024];
  int bytes_read;

 AGAIN:
  while ((bytes_read = read (data->infd, input_buffer, sizeof (input_buffer)))
		 > 0)
	{
	  if (data->next_stream)
		{
		  int status = ((*data->next_stream->put_block)
						(data->next_stream,
						 input_buffer, bytes_read));
		  /* abort */
		  if (status < 0)
			return status;
		}
	}

  /* It's necessary that we block here waiting for the process to produce
	 the rest of its output before we allow the `complete' method to return.
	 We've already set the socket to be nonblocking, and there doesn't appear
	 to be any way to set it to do blocking reads again, so instead, we'll
	 use select() to block for it.  That will return when there is some input
	 available, and we'll read it, and (maybe) block again, repeating until
	 we get an EOF.

	 To implement this in a non-blocking way would require the input and
	 output sides of this to be disconnected - the output side would be as in
	 this file, but the input side would need to be a new stream type in
	 NET_ProcessNet(), at the level of http, ftp, and file streams.  
   */
  if (bytes_read == -1 && block_p &&
	  (errno == EAGAIN || errno == EWOULDBLOCK))
	{
	  fd_set rset;
	  FD_ZERO (&rset);
	  FD_SET (data->infd, &rset);
	  if (select (data->infd+1, &rset, 0, 0, 0) < 0)
		perror ("select");
	  goto AGAIN;
	}

  return 1;
}


PRIVATE int net_ExtConverterWrite (NET_StreamClass *stream,
								   CONST char* output_buffer,
								   int32 output_length)
{
  CVG_DataObject *data = (CVG_DataObject *) stream->data_object;  

  while (output_length > 0)
    {
	  int bytes_written = 0;

	  /* write as much as possible (until done, or the pipe is full.)
	   */
	  while (output_length > 0 &&
			 (bytes_written = write (data->outfd, output_buffer,
									 output_length))
			 > 0)
		{
		  output_buffer += bytes_written;
		  output_length -= bytes_written;
		}

	  if (bytes_written == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
		{
		  perror ("write");
		  return -1;
		}

	  /* Now read as much as possible (until done, or the pipe is drained.)
	   */
	  {
		int status = net_ExtConverterRead (data, PR_FALSE);
		/* abort */
		if (status < 0)
		  return status;
	  }

	  /* Now go around the loop again, if we weren't able to write all of
		 the output buffer at once (because the pipe filled up.)  Now that
		 we've read the available data from the pipe, we will presumably
		 be able to write to it again.
	   */
	}
  return 1;
}

PRIVATE int net_ExtConverterWriteReady (NET_StreamClass *stream)
{	
  /* #### I'm not sure what the right thing to do here is. --jwz */
#if 1
  return (MAX_WRITE_READY);
#else
  CVG_DataObject *data = (CVG_DataObject *) stream->data_object;
  if(data->next_stream)
    return ((*data->next_stream->is_write_ready)
			(data->next_stream));
  else
    return (MAX_WRITE_READY);
#endif
}


PRIVATE void
net_KillConverterProcess (CVG_DataObject *data)
{
  pid_t wait_status;
  /* It may not actually be necessary to kill the process here if we have
	 exited normally, since in that case it has already closed its stdout;
	 but it can't hurt.

	 After it dies, we have to wait() for it, or it becomes a zombie.
	 I'm not entirely sure that the perror() is correct - it could be that
	 0 is a legitimate value from waitpid() if the process had already
	 exited before we kill()ed it, but I'm not sure.
   */

  kill (data->pid, SIGINT);
  wait_status = waitpid (data->pid, 0, 0);
#ifdef DEBUG_dp
  fprintf(stderr, "Restoring sigchild handler for pid %d.\n", data->pid);
#endif
  /* Reset SIGCHLD signal hander before returning */
  sigaction(SIGCHLD, &data->oldact, NULL);

  if (wait_status != data->pid)
	perror ("waitpid");
}

PRIVATE void net_ExtConverterComplete (NET_StreamClass *stream)
{
  CVG_DataObject *data = (CVG_DataObject *) stream->data_object;  

  /* Send an EOF to the stdin of the subprocess; then wait for the rest
	 of its output to show up on its stdout; then close stdout, and kill
	 the process. */
  close (data->outfd);
  net_ExtConverterRead (data, PR_TRUE);
  close (data->infd);
  net_KillConverterProcess (data);

  /* complete the next stream */
  if (data->next_stream)
    {
      (*data->next_stream->complete) (data->next_stream);
      free (data->next_stream);
    }
  free (data);
}

PRIVATE void net_ExtConverterAbort (NET_StreamClass *stream, int status)
{
  CVG_DataObject *data = (CVG_DataObject *) stream->data_object;  

  /* Close the streams and kill the process, discarding any output still
	 in the pipe. */
  close (data->outfd);
  close (data->infd);
  net_KillConverterProcess (data);

  /* abort the next stream */
  if (data->next_stream)
    {
      (*data->next_stream->abort) (data->next_stream, status);
      free (data->next_stream);
    }
  free (data);
}


PUBLIC NET_StreamClass * 
NET_ExtConverterConverter (int          format_out,
                           void        *data_obj,
                           URL_Struct  *URL_s,
                           MWContext   *window_id)
{
    CVG_DataObject* obj;
    CV_ExtConverterStruct * ext_con_obj = (CV_ExtConverterStruct *) data_obj;
    NET_StreamClass* stream;
	struct sigaction newact;
    
    TRACEMSG(("Setting up display stream. Have URL: %s\n", URL_s->address));

    stream = PR_NEW(NET_StreamClass);
    if(stream == NULL) 
            return(NULL);

	memset(stream, 0, sizeof(NET_StreamClass));

    obj = PR_NEW(CVG_DataObject);
    if (obj == NULL) 
            return(NULL);
	memset(obj, 0, sizeof(CVG_DataObject));

    stream->name           = "Filter Stream";
    stream->complete       = (MKStreamCompleteFunc) net_ExtConverterComplete;
    stream->abort          = (MKStreamAbortFunc) net_ExtConverterAbort;
    stream->put_block      = (MKStreamWriteFunc) net_ExtConverterWrite;
    stream->is_write_ready = (MKStreamWriteReadyFunc) net_ExtConverterWriteReady;
    stream->data_object    = obj;  /* document info object */
    stream->window_id      = window_id;

	/* Open the next stream.
	   First, swap in the content-encoding or content-type of the document
	   as we are about to convert it, to look up the proper next converter
	   (and to avoid looping.)  But once we have set up the stream, put the
	   original encoding back, so that the URL_Struct is not permanently
	   altered - foo.html.gz must still have content-encoding x-gzip even
	   though it has been decoded for display.
	 */
	{
	  char *old, *new;
	  if (ext_con_obj->is_encoding_converter)
		{
		  old = URL_s->content_encoding;
		  new = PL_strdup (ext_con_obj->new_format);
		  if (!new) return (NULL);
		  URL_s->content_encoding = new;
		}
	  else
		{
		  old = URL_s->content_type;
		  new = PL_strdup (ext_con_obj->new_format);
		  if (!new) return (NULL);
		  URL_s->content_type = new;
		}

	  obj->next_stream = NET_StreamBuilder (format_out, URL_s, window_id);

	  if (ext_con_obj->is_encoding_converter)
		{
		  PR_Free (URL_s->content_encoding);
		  URL_s->content_encoding = old;
		}
	  else
		{
		  PR_Free (URL_s->content_type);
		  URL_s->content_type = old;
		}
	}

	if (!obj->next_stream)
	  return (NULL);

	/* Open two pipes, one for writing to a subprocess, and one for reading
	   from it (for a total of four file descriptors, I/O for us, and O/I
	   for the kid.)
	 */
	{
	  int infds [2];
	  int outfds[2];
	  pid_t forked;

	  if (pipe (infds))
		{
		  perror ("creating input pipe");
		  free (stream);
		  free (obj);
		  return 0;
		}
	  if (pipe (outfds))
		{
		  perror ("creating output pipe");
		  free (stream);
		  free (obj);
		  return 0;
		}
	  obj->infd  = infds  [0];
	  obj->outfd = outfds [1];

	  /* Set our side of the pipes to be nonblocking.  (It's important not
		 to set the other side of the pipes to be nonblocking - that
		 decision must be left up to the process on the other end. */

#if defined(O_NONBLOCK)
# define NONBLOCK_FLAG O_NONBLOCK
#elif defined(O_NDELAY)
# define NONBLOCK_FLAG O_NDELAY
#else
	  ERROR!! neither O_NONBLOCK nor O_NDELAY are defined.
#endif
	  fcntl (obj->infd,  F_SETFL, NONBLOCK_FLAG);
	  fcntl (obj->outfd, F_SETFL, NONBLOCK_FLAG);
#undef NONBLOCK_FLAG

	  obj->pid = 0;

#ifdef DEBUG_dp
  fprintf(stderr, "Ignoring sigchild.\n");
#endif
	  /* Setup signals so that SIGCHLD is ignored as we want to do waitpid
	   * when the helperapp ends
	   */
	  newact.sa_handler = SIG_DFL;
	  newact.sa_flags = 0;
	  sigfillset(&newact.sa_mask);
	  sigaction (SIGCHLD, &newact, &obj->oldact);

	  switch (forked = fork ())
		{
		case -1:
		  perror ("fork");
		  close (outfds[0]);
		  close (outfds[1]);
		  close (infds [0]);
		  close (infds [1]);
		  free (stream);
		  free (obj);
		  /* Reset SIGCHLD signal hander before returning */
		  sigaction(SIGCHLD, &obj->oldact, NULL);
		  return 0;
		case 0:
		  {
			/* This is the new process.  exec() the filter here.
			   We do this with sh to get tokenization and pipelines
			   and all that junk.
			 */
			char *av[10];
			int ac = 0;

			av [ac++] = "/bin/sh";
			av [ac++] = "-c";
			av [ac++] = ext_con_obj->command;
			av [ac++] = 0;

			dup2 (outfds[0], 0);		/* stdin */
			dup2 (infds [1], 1);		/* stdout */
		/*	dup2 (infds [1], 2);		 * stderr */

			/* We have copied the two pipes to stdin/stdout.
			   We no longer need the other file descriptors hanging around.
			   (Actually I think we need to close these, or the other side
			   of the pipe doesn't see an eof when we close stdout...)
			 */
			close (outfds[0]);
			close (outfds[1]);
			close (infds [0]);
			close (infds [1]);

			execv (av[0], av);
			/* exec() should never return. */
			perror ("execv");
			exit (1);	/* This only exits a child fork.  */
			break;
		  }
		default:
		  /* This is the "old" process (subproc pid is in `forked'.) */
		  obj->pid = forked;

		  /* These are the file descriptors we created for the benefit
			 of the child process - we don't need them in the parent. */
		  close (outfds[0]);
		  close (infds [1]);

		  break;
		}
	}

    TRACEMSG(("Returning stream from NET_ExtConverterConverter\n"));

    return stream;
}
