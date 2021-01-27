# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/
from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import CONCAT, COPY, REPLACE
from fluent.migrate.helpers import TERM_REFERENCE, VARIABLE_REFERENCE, transforms_from


def migrate(ctx):
    """Bug 1510797 - Migrate about:webrtc to Fluent, part {index}"""
    target = "toolkit/toolkit/about/aboutWebrtc.ftl"
    reference = "toolkit/toolkit/about/aboutWebrtc.ftl"
    ctx.add_transforms(
        target,
        reference,
        transforms_from(
            """
about-webrtc-document-title = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "document_title") }
about-webrtc-save-page-dialog-title = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "save_page_dialog_title") }
about-webrtc-aec-logging-msg-label = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "aec_logging_msg_label") }
about-webrtc-aec-logging-off-state-label = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "aec_logging_off_state_label") }
about-webrtc-aec-logging-on-state-label = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "aec_logging_on_state_label") }
about-webrtc-aec-logging-on-state-msg = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "aec_logging_on_state_msg") }
about-webrtc-peerconnection-id-label = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "peer_connection_id_label") }:
about-webrtc-sdp-heading = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "sdp_heading") }
about-webrtc-local-sdp-heading = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "local_sdp_heading") }
about-webrtc-local-sdp-heading-offer = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "local_sdp_heading") } ({ COPY("toolkit/chrome/global/aboutWebrtc.properties", "offer") })
about-webrtc-local-sdp-heading-answer = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "local_sdp_heading") } ({ COPY("toolkit/chrome/global/aboutWebrtc.properties", "answer") })
about-webrtc-remote-sdp-heading = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "remote_sdp_heading") }
about-webrtc-remote-sdp-heading-offer = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "remote_sdp_heading") } ({ COPY("toolkit/chrome/global/aboutWebrtc.properties", "offer") })
about-webrtc-remote-sdp-heading-answer = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "remote_sdp_heading") } ({ COPY("toolkit/chrome/global/aboutWebrtc.properties", "answer") })
about-webrtc-sdp-history-heading = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "sdp_history_heading") }
about-webrtc-sdp-parsing-errors-heading = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "sdp_parsing_errors_heading") }
about-webrtc-rtp-stats-heading = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "rtp_stats_heading") }
about-webrtc-ice-state = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "ice_state") }
about-webrtc-ice-stats-heading = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "ice_stats_heading") }
about-webrtc-ice-restart-count-label = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "ice_restart_count_label") }:
about-webrtc-ice-rollback-count-label = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "ice_rollback_count_label") }:
about-webrtc-ice-pair-bytes-sent = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "ice_pair_bytes_sent") }:
about-webrtc-ice-pair-bytes-received = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "ice_pair_bytes_received") }:
about-webrtc-ice-component-id = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "ice_component_id") }
about-webrtc-type-local = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "typeLocal") }
about-webrtc-type-remote = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "typeRemote") }
about-webrtc-nominated = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "nominated") }
about-webrtc-selected = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "selected") }
about-webrtc-save-page-label = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "save_page_label") }
about-webrtc-debug-mode-msg-label = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "debug_mode_msg_label") }
about-webrtc-debug-mode-off-state-label = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "debug_mode_off_state_label") }
about-webrtc-debug-mode-on-state-label = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "debug_mode_on_state_label") }
about-webrtc-stats-heading = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "stats_heading") }
about-webrtc-stats-clear = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "stats_clear") }
about-webrtc-log-heading = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "log_heading") }
about-webrtc-log-clear = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "log_clear") }
about-webrtc-connection-open = [ { $browser-id } | { $id } ] { $url } { $now }
about-webrtc-connection-closed = [ { $browser-id } | { $id } ] { $url } ({ COPY("toolkit/chrome/global/aboutWebrtc.properties", "connection_closed") }) { $now }
about-webrtc-local-candidate = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "local_candidate") }
about-webrtc-remote-candidate = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "remote_candidate") }
about-webrtc-raw-candidates-heading = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "raw_candidates_heading") }
about-webrtc-raw-local-candidate = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "raw_local_candidate") }
about-webrtc-raw-remote-candidate = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "raw_remote_candidate") }
about-webrtc-raw-cand-show-msg = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "raw_cand_show_msg") }
    .title = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "fold_show_hint") }
about-webrtc-raw-cand-hide-msg = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "raw_cand_hide_msg") }
    .title = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "fold_hide_hint") }
about-webrtc-priority = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "priority") }
about-webrtc-log-show-msg = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "log_show_msg") }
    .title = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "fold_show_hint") }
about-webrtc-log-hide-msg = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "log_hide_msg") }
    .title = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "fold_hide_hint") }
about-webrtc-fold-show-msg = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "fold_show_msg") }
    .title = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "fold_show_hint") }
about-webrtc-fold-hide-msg = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "fold_hide_msg") }
    .title = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "fold_hide_hint") }
about-webrtc-decoder-label = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "decoder_label") }
about-webrtc-encoder-label = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "encoder_label") }
about-webrtc-jitter-label = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "jitter_label") } { $jitter }
about-webrtc-show-tab-label = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "show_tab_label") }
about-webrtc-width-px = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "width_px") }
about-webrtc-height-px = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "height_px") }
about-webrtc-consecutive-frames = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "consecutive_frames") }
about-webrtc-time-elapsed = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "time_elapsed") }
about-webrtc-estimated-framerate = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "estimated_framerate") }
about-webrtc-rotation-degrees = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "rotation_degrees") }
about-webrtc-first-frame-timestamp = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "first_frame_timestamp") }
about-webrtc-last-frame-timestamp = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "last_frame_timestamp") }
about-webrtc-local-receive-ssrc = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "local_receive_ssrc") }
about-webrtc-remote-send-ssrc = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "remote_send_ssrc") }
about-webrtc-configuration-element-provided = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "configuration_element_provided") }
about-webrtc-configuration-element-not-provided = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "configuration_element_not_provided") }
about-webrtc-custom-webrtc-configuration-heading = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "custom_webrtc_configuration_heading") }
about-webrtc-bandwidth-stats-heading = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "bandwidth_stats_heading") }
about-webrtc-track-identifier = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "track_identifier") }
about-webrtc-send-bandwidth-bytes-sec = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "send_bandwidth_bytes_sec") }
about-webrtc-receive-bandwidth-bytes-sec = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "receive_bandwidth_bytes_sec") }
about-webrtc-max-padding-bytes-sec = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "max_padding_bytes_sec") }
about-webrtc-pacer-delay-ms = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "pacer_delay_ms") }
about-webrtc-round-trip-time-ms = { COPY("toolkit/chrome/global/aboutWebrtc.properties", "round_trip_time_ms") }
"""
        ),
    )

    ctx.add_transforms(
        target,
        reference,
        [
            FTL.Message(
                id=FTL.Identifier("about-webrtc-save-page-msg"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutWebrtc.properties",
                    "save_page_msg",
                    {
                        "%1$S": VARIABLE_REFERENCE("path"),
                    },
                    normalize_printf=True,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("about-webrtc-debug-mode-off-state-msg"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutWebrtc.properties",
                    "debug_mode_off_state_msg",
                    {
                        "%1$S": VARIABLE_REFERENCE("path"),
                    },
                    normalize_printf=True,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("about-webrtc-debug-mode-on-state-msg"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutWebrtc.properties",
                    "debug_mode_on_state_msg",
                    {
                        "%1$S": VARIABLE_REFERENCE("path"),
                    },
                    normalize_printf=True,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("about-webrtc-aec-logging-off-state-msg"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutWebrtc.properties",
                    "aec_logging_off_state_msg",
                    {
                        "%1$S": VARIABLE_REFERENCE("path"),
                    },
                    normalize_printf=True,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("about-webrtc-trickle-caption-msg"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutWebrtc.properties",
                    "trickle_caption_msg2",
                    {
                        "%1$S": COPY(
                            "toolkit/chrome/global/aboutWebrtc.properties",
                            "trickle_highlight_color_name2",
                        )
                    },
                    normalize_printf=True,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("about-webrtc-sdp-set-at-timestamp-local"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutWebrtc.properties",
                    "sdp_set_at_timestamp",
                    {
                        "%1$S": COPY(
                            "toolkit/chrome/global/aboutWebrtc.properties",
                            "local_sdp_heading",
                        ),
                        "%2$S": FTL.FunctionReference(
                            id=FTL.Identifier("NUMBER"),
                            arguments=FTL.CallArguments(
                                positional=[
                                    FTL.VariableReference(
                                        id=FTL.Identifier("timestamp")
                                    )
                                ],
                                named=[
                                    FTL.NamedArgument(
                                        name=FTL.Identifier("useGrouping"),
                                        value=FTL.StringLiteral("false"),
                                    )
                                ],
                            ),
                        ),
                    },
                    normalize_printf=True,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("about-webrtc-sdp-set-at-timestamp-remote"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutWebrtc.properties",
                    "sdp_set_at_timestamp",
                    {
                        "%1$S": COPY(
                            "toolkit/chrome/global/aboutWebrtc.properties",
                            "remote_sdp_heading",
                        ),
                        "%2$S": FTL.FunctionReference(
                            id=FTL.Identifier("NUMBER"),
                            arguments=FTL.CallArguments(
                                positional=[
                                    FTL.VariableReference(
                                        id=FTL.Identifier("timestamp")
                                    )
                                ],
                                named=[
                                    FTL.NamedArgument(
                                        name=FTL.Identifier("useGrouping"),
                                        value=FTL.StringLiteral("false"),
                                    )
                                ],
                            ),
                        ),
                    },
                    normalize_printf=True,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("about-webrtc-sdp-set-timestamp"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutWebrtc.properties",
                    "sdp_set_timestamp",
                    {
                        "%1$S": FTL.FunctionReference(
                            id=FTL.Identifier("NUMBER"),
                            arguments=FTL.CallArguments(
                                positional=[
                                    FTL.VariableReference(
                                        id=FTL.Identifier("timestamp")
                                    )
                                ],
                                named=[
                                    FTL.NamedArgument(
                                        name=FTL.Identifier("useGrouping"),
                                        value=FTL.StringLiteral("false"),
                                    )
                                ],
                            ),
                        ),
                        "%2$S": VARIABLE_REFERENCE("relative-timestamp"),
                    },
                    normalize_printf=True,
                ),
            ),
        ],
    )
