/* 
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#include "cmt.h"

CMTStatus myCallback(CMTControl * control, CMTItem * event, void * arg);

int main(int argc, char ** argv)
{
  CMTItem * msg, * event = NULL;
  CMTStatus status;
  int socket, datasocket;
  int sent;
  CMTControl * connect;
  char * buffer = "some weird text that I feel like passing to server";
  
  connect = CMT_ControlConnect(myCallback, event);
  
  msg = CMT_ConstructMessage(10);

  msg->type = (int)CMTClientMessage;
  sprintf((char *)msg->data, "first msg!");

  status = CMT_SendMessage(connect, msg, event);
  if (status != SECSuccess)
    perror("CMT_SendMessage");
  
  CMT_FreeEvent(event);
  event = NULL;

  sprintf((char *)msg->data, "second msg");
  status = CMT_SendMessage(connect, msg, event);
  if (status != SECSuccess)
    perror("CMT_SendMessage");

  datasocket = CMT_DataConnect(connect, NULL);
  if (datasocket < 0) 
    perror("CMT_DataConnect");
  
  sent = write(datasocket, (void *)buffer, strlen(buffer));
  sent = write(datasocket, (void *)buffer, strlen(buffer));

  close(datasocket);
  
  msg->type = (int)CMTClientMessage;
  sprintf((char *)msg->data, "third msg!");
  status = CMT_SendMessage(connect, msg, event);
  if (status != SECSuccess)
    perror("CMT_SendMessage"); 
  
  status = CMT_CloseControlConnection(connect);
  if (status != SECSuccess)
    perror("CMT_CloseControl");
  
  CMT_FreeMessage(msg);
  CMT_FreeEvent(event);
}

CMTStatus myCallback(CMTControl * control, CMTItem * event, void * arg)
{
  if (event) 
    printf("Event received is : type %d, data %s\n", event->type, event->data);
  else printf("No event!\n");
  if (arg) 
    printf("Arg is %s\n", (char *)arg);
  else printf("No arg!\n");


  return SECSuccess;
}
