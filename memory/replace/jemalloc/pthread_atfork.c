/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

int pthread_atfork(void (*prefork)(void),
                   void (*postfork_parent)(void),
                   void (*postfork_child)(void))
{
  return 0;
}
