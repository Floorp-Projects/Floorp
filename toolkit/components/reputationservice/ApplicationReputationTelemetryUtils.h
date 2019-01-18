//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ApplicationReputationTelemetryUtils_h__
#define ApplicationReputationTelemetryUtils_h__

#include "mozilla/Telemetry.h"

/**
 * Convert network errors to telemetry labels
 * Only the following nsresults are converted, otherwise this function
 * will return "ErrorOthers".
 * NS_ERROR_ALREADY_CONNECTED
 * NS_ERROR_NOT_CONNECTED
 * NS_ERROR_CONNECTION_REFUSED
 * NS_ERROR_NET_TIMEOUT
 * NS_ERROR_OFFLINE
 * NS_ERROR_PORT_ACCESS_NOT_ALLOWED
 * NS_ERROR_NET_RESET
 * NS_ERROR_NET_INTERRUPT
 * NS_ERROR_PROXY_CONNECTION_REFUSED
 * NS_ERROR_NET_PARTIAL_TRANSFER
 * NS_ERROR_NET_INADEQUATE_SECURITY
 * NS_ERROR_UNKNOWN_HOST
 * NS_ERROR_DNS_LOOKUP_QUEUE_FULL
 * NS_ERROR_UNKNOWN_PROXY_HOST
 */
mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_SERVER_2 NSErrorToLabel(
    nsresult aRv);

/**
 * Convert http response status to telemetry labels
 */
mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_SERVER_2 HTTPStatusToLabel(
    uint32_t aStatus);

/**
 * Convert verdict type to telemetry labels
 */
mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_SERVER_VERDICT_2
VerdictToLabel(uint32_t aVerdict);

#endif  // ApplicationReputationTelemetryUtils_h__
