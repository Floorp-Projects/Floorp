/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _LOGGER_INCLUDED_H
#define _LOGGER_INCLUDED_H

#define LOG_MAX_LEN 64

/*
 * short form helper macros
 */
void log_msg(int log, ...);
void log_clear(int msg);
char *get_device_name();

#endif /* _LOGGER_INCLUDED_H */
