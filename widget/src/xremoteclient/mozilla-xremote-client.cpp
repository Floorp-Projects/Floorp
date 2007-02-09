/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2001 the Initial Developer. All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <plgetopt.h>
#ifdef MOZ_WIDGET_PHOTON
#include "PhRemoteClient.h"
#else
#include "XRemoteClient.h"
#endif

static void print_usage(void);

int main(int argc, char **argv)
{
  nsresult rv;
  XRemoteClient client;
  char *browser = 0;
  char *profile = 0;
  char *username = 0;
  char *command = 0;

  if (argc < 2) {
    print_usage();
    return 4;
  }

  PLOptStatus os;
  PLOptState *opt = PL_CreateOptState(argc, argv, "ha:u:p:");
  while (PL_OPT_EOL != (os = PL_GetNextOpt(opt))) {
    if (PL_OPT_BAD == os) {
      print_usage();
      return 4;
    }

    switch (opt->option) {
    case 'h':
      print_usage();
      return 4;
      break;
    case 'u':
      username = strdup(opt->value);
      break;
    case 'a':
      browser = strdup(opt->value);
      break;
    case 'p':
      profile = strdup(opt->value);
      break;
    case 0:
      command = strdup(opt->value);      
    default:
      break;
    }
  }

  rv = client.Init();
  // failed to connect to the X server
  if (NS_FAILED(rv))
    return 1;

  // send the command - it doesn't get any easier than this
  PRBool success = PR_FALSE;
  char *error = 0;
  rv = client.SendCommand(browser, username, profile, command, nsnull,
                          &error, &success);

  // failed to send command
  if (NS_FAILED(rv)) {
    fprintf(stderr, "%s: Error: Failed to send command: ", argv[0]);
    if (error) {
      fprintf(stderr, "%s\n", error);
      free(error);
    }
    else {
      fprintf(stderr, "No error string reported..\n");
    }

    return 3;
  }

  // no running window found
  if (!success) {
    fprintf(stderr, "%s: Error: Failed to find a running server.\n", argv[0]);
    return 2;
  }

  // else, everything is fine.
  return 0;
}

/* static */
void print_usage(void) {
  fprintf(stderr, "Usage: mozilla-xremote-client [-a firefox|thunderbird|mozilla|any]\n");
  fprintf(stderr, "                              [-u <username>]\n");
  fprintf(stderr, "                              [-p <profile>] COMMAND\n");
}
