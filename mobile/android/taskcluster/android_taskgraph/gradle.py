# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


def get_gradle_project(task):
    attributes = task.get("attributes", {})
    gradle_project = attributes.get("component")
    if not gradle_project:
        shipping_product = attributes.get("shipping-product", "")
        if shipping_product:
            return shipping_product

        # TODO: Use only shipping-product attribute instead
        treeherder_group = attributes.get("treeherder-group", "")
        if "focus" in treeherder_group:
            gradle_project = "focus"
        elif "fenix" in treeherder_group:
            gradle_project = "fenix"
        else:
            gradle_project = None
    return gradle_project
