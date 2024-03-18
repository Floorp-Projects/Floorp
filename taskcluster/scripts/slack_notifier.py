#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
This module provides functionalities for sending notifications to Slack channels, specifically designed for use in automated testing and release processes. It includes capabilities for sending both success and error notifications with customizable message templates. The module leverages Taskcluster for notification services and integrates with Slack's API to deliver real-time updates.

Key Features:
- SLACK_SUCCESS_MESSAGE_TEMPLATE: A predefined template for formatting success messages to be sent to Slack. This template includes placeholders for dynamic content such as product version and release details.
- SLACK_ERROR_MESSAGE_TEMPLATE: A template for error messages, used to notify about failures or issues in automated processes, particularly with TestRail API interactions.
- send_slack_notification: A function that sends a Slack notification based on a provided template and value dictionary. It handles the construction of the message payload and interfaces with Taskcluster's Slack notification service.
- get_taskcluster_options: Retrieves configuration options for Taskcluster based on the current runtime environment, ensuring appropriate setup for notification delivery.
- send_error_notification: A higher-level function that formats and sends error notifications to a specified Slack channel.
- send_success_notification: Similarly, this function sends success notifications to a specified Slack channel, using the success message template.

Usage:
The module is intended to be integrated into automated testing and release workflows, where Slack notifications are required to report the status of various processes, such as test executions or release milestones.

Required Values for Notifications:

These values are required when calling the `send_success_notification` and `send_slack_notification` functions.
They must be passed as an object with the following keys and their respective values.

Required Keys and Expected Values:
- RELEASE_TYPE: <string> Release Type or Stage (e.g., Alpha, Beta, RC).
- RELEASE_VERSION: <string> Release Version from versions.txt (e.g., '124.0b5').
- SHIPPING_PRODUCT: <string> Release Tag Name (e.g., fennec, focus).
- TESTRAIL_PROJECT_ID: <int> Project ID for TestRail Project (e.g., Fenix Browser).
- TESTRAIL_PRODUCT_TYPE: <string> Name for the official release product (e.g., Firefox, not fennec).

These values are used as arguments for `success_values` and `values` when calling the respective functions.

Example Usage:

success_values = {
    "RELEASE_TYPE": "Beta",
    "RELEASE_VERSION": "124.0b5",
    "SHIPPING_PRODUCT": "fennec",
    "TESTRAIL_PROJECT_ID": 59, # Fenix Browser
    "TESTRAIL_PRODUCT_TYPE": "Firefox"
}

send_success_notification(success_values, 'channel_id', taskcluster_options)

values = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()),
        "error_message": error_message,
}

send_error_notification(values, 'channel_id', taskcluster_options)
"""

import json
import os
import time
import traceback
from string import Template

import taskcluster

SLACK_SUCCESS_MESSAGE_TEMPLATE = Template(
    """
[
    {
        "type": "header",
        "text": {
                "type": "plain_text",
                "text": "New Release: :firefox: $SHIPPING_PRODUCT-v$RELEASE_VERSION :star:"
        }
    },
    {
        "type": "divider"
    },
    {
        "type": "section",
        "text": {
            "type": "mrkdwn",
            "text": "*Testrail Release*: $TESTRAIL_PRODUCT_TYPE $RELEASE_TYPE $RELEASE_VERSION <https://testrail.stage.mozaws.net/index.php?/projects/overview/$TESTRAIL_PROJECT_ID|Milestone> has been created:testrail:"
        }
    },
    {
        "type": "section",
        "text": {
            "type": "mrkdwn",
            "text": "*UI Automated Tests*:"
        }
    },
    {
        "type": "section",
        "text": {
            "type": "mrkdwn",
            "text": " :white_check_mark: Automated smoke test - Google Pixel 3(Android 11)"
        }
    },
    {
        "type": "section",
        "text": {
            "type": "mrkdwn",
            "text": ":white_check_mark: Automated smoke test - Google Pixel 2(Android 9)"
        }
    },
    {
        "type": "divider"
    },
    {
        "type": "context",
        "elements": [
            {
                "type": "mrkdwn",
                "text": ":testops-notify: created by <https://mozilla-hub.atlassian.net/wiki/spaces/MTE/overview|Mobile Test Engineering>"
            }
        ]
    }
]
"""
)

SLACK_ERROR_MESSAGE_TEMPLATE = Template(
    """
[
    {
        "type": "section",
        "text": {
            "type": "mrkdwn",
            "text": "Failed to call TestRail API at $timestamp with error: $error_message"
        }
    }
]
"""
)


def send_slack_notification(template, values, channel_id, options):
    """
    Sends a Slack notification based on the provided template and values.

    :param template: Template object for the Slack message.
    :param values: Dictionary containing values to substitute in the template.
    :param channel_id: Slack channel ID to send the message to.
    :param options: Taskcluster options for the notification service.
    """
    slack_message = json.loads(template.safe_substitute(**values))
    # workaround for https://github.com/taskcluster/taskcluster/issues/6801
    duplicate_message_workaround = str(int(time.time()))
    payload = {
        "channelId": channel_id,
        "text": duplicate_message_workaround,
        "blocks": slack_message,
    }

    try:
        response = taskcluster.Notify(options).slack(payload)
        print("Response from API:", response)
    except Exception as e:
        print(f"Error sending Slack message: {e}")
        traceback.print_exc()

        if hasattr(e, "response"):
            print("Response content:", e.response.text)


def get_taskcluster_options():
    """
    Retrieves the Taskcluster setup options according to the current environment.

    :return: A dictionary of Taskcluster options.
    """
    options = taskcluster.optionsFromEnvironment()
    proxy_url = os.environ.get("TASKCLUSTER_PROXY_URL")

    if proxy_url is not None:
        # Always use proxy url when available
        options["rootUrl"] = proxy_url

    if "rootUrl" not in options:
        # Always have a value in root url
        options["rootUrl"] = "https://community-tc.services.mozilla.com"

    return options


def send_error_notification(error_message, channel_id, options):
    values = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()),
        "error_message": error_message,
    }
    send_slack_notification(SLACK_ERROR_MESSAGE_TEMPLATE, values, channel_id, options)


def send_success_notification(success_values, channel_id, options):
    send_slack_notification(
        SLACK_SUCCESS_MESSAGE_TEMPLATE, success_values, channel_id, options
    )
