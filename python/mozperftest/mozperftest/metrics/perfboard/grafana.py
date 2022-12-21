# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import os

from grafana_api.grafana_face import GrafanaFace

HERE = os.path.dirname(__file__)


with open(os.path.join(HERE, "dashboard.json")) as f:
    template = json.loads(f.read())

with open(os.path.join(HERE, "panel.json")) as f:
    panel_template = json.loads(f.read())

with open(os.path.join(HERE, "target.json")) as f:
    target_template = json.loads(f.read())


class Grafana:
    def __init__(self, layer, key, host="perfboard.dev.mozaws.net", port=3000):
        self.client = GrafanaFace(host=host, port=port, auth=key)
        self.layer = layer

    def get_dashboard(self, title):
        existing = self.client.search.search_dashboards(tag="component")
        existing = dict(
            [(dashboard["title"].lower(), dashboard["uid"]) for dashboard in existing]
        )
        if title in existing:
            return self.client.dashboard.get_dashboard(existing[title])
        self.layer.debug(f"Creating dashboard {title}")
        d = dict(template)
        d["title"] = title.capitalize()
        res = self.client.dashboard.update_dashboard(
            dashboard={"dashboard": d, "folderId": 0, "overwrite": False}
        )

        return self.client.dashboard.get_dashboard(res["uid"])

    def _add_panel(self, dashboard, panel_title, metrics):
        found = None
        ids = []
        for panel in dashboard["dashboard"]["panels"]:
            ids.append(panel["id"])

            if panel["title"] == panel_title:
                found = panel

        ids.sort()

        need_update = False
        if found is None:
            # create the panel
            panel = panel_template
            panel["title"] = panel_title
            if ids != []:
                panel["id"] = ids[-1] + 1
            else:
                panel["id"] = 1
            self.layer.debug("Creating panel")
            dashboard["dashboard"]["panels"].append(panel)
            need_update = True
        else:
            self.layer.debug("Panel exists")
            panel = found

        # check the metrics
        existing = [target["measurement"] for target in panel["targets"]]

        for metric in metrics:
            if metric in existing:
                continue
            m = dict(target_template)
            m["measurement"] = metric
            panel["targets"].append(m)
            need_update = True

        if need_update:
            self.layer.debug("Updating dashboard")
            self.client.dashboard.update_dashboard(dashboard=dashboard)

    def add_panel(self, dashboard, panel, metrics):
        dashboard = self.get_dashboard(dashboard)
        self._add_panel(dashboard, panel, metrics)
