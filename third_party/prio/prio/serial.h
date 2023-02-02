/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <mprio.h>

SECStatus serial_write_packet_client(msgpack_packer* pk,
                                     const_PrioPacketClient p,
                                     const_PrioConfig cfg);

SECStatus serial_read_packet_client(msgpack_unpacker* upk, PrioPacketClient p,
                                    const_PrioConfig cfg);

#endif /* __SERIAL_H__ */
