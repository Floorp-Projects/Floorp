# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.


import os

from mach.decorators import Command, CommandArgument
from mozpack.copier import Jarrer
from mozpack.files import FileFinder


@Command("tps-build", category="testing", description="Build TPS add-on.")
@CommandArgument("--dest", default=None, help="Where to write add-on.")
def build(command_context, dest):
    src = os.path.join(
        command_context.topsrcdir, "services", "sync", "tps", "extensions", "tps"
    )
    dest = os.path.join(
        dest or os.path.join(command_context.topobjdir, "services", "sync"),
        "tps.xpi",
    )

    if not os.path.exists(os.path.dirname(dest)):
        os.makedirs(os.path.dirname(dest))

    if os.path.isfile(dest):
        os.unlink(dest)

    jarrer = Jarrer()
    for p, f in FileFinder(src).find("*"):
        jarrer.add(p, f)
    jarrer.copy(dest)

    print("Built TPS add-on as %s" % dest)
