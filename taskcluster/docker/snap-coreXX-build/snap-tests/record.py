# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import os
import time

import dbus

session_bus = dbus.SessionBus()
session_bus.call_blocking(
    "org.gnome.Shell.Screencast",
    "/org/gnome/Shell/Screencast",
    "org.gnome.Shell.Screencast",
    "Screencast",
    signature="sa{sv}",
    args=[
        os.path.join(os.environ.get("ARTIFACT_DIR", ""), "video_%d_%t.webm"),
        {"draw-cursor": True, "framerate": 35},
    ],
)

while True:
    time.sleep(30)
