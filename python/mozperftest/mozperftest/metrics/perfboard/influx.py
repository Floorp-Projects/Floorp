# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import datetime
import statistics
from collections import defaultdict

from mozperftest import utils
from mozperftest.layers import Layer
from mozperftest.metrics.common import COMMON_ARGS, filtered_metrics
from mozperftest.utils import get_tc_secret, install_package


class Influx(Layer):
    """Sends the metrics to an InfluxDB server"""

    name = "perfboard"
    activated = False
    arguments = COMMON_ARGS
    arguments.update(
        {
            "dashboard": {
                "type": str,
                "default": None,
                "help": "Name of the dashboard - defaults to the script"
                " `component` metadata. When not set, falls back to"
                " `perftest`",
            },
            "influx-host": {
                "type": str,
                "default": "perfboard.dev.mozaws.net",
            },
            "influx-user": {
                "type": str,
                "default": "admin",
            },
            "influx-port": {
                "type": int,
                "default": 8086,
            },
            "influx-password": {
                "type": str,
                "default": None,
            },
            "influx-db": {
                "type": str,
                "default": "perf",
            },
            "grafana-host": {
                "type": str,
                "default": "perfboard.dev.mozaws.net",
            },
            "grafana-key": {
                "type": str,
                "default": None,
            },
            "grafana-port": {
                "type": int,
                "default": 3000,
            },
        }
    )

    def _setup(self):
        venv = self.mach_cmd.virtualenv_manager
        try:
            from influxdb import InfluxDBClient
        except ImportError:
            install_package(venv, "influxdb", ignore_failure=False)
            from influxdb import InfluxDBClient

        try:
            from mozperftest.metrics.perfboard.grafana import Grafana
        except ImportError:
            install_package(venv, "grafana_api", ignore_failure=False)
            from mozperftest.metrics.perfboard.grafana import Grafana

        if utils.ON_TRY:
            secret = get_tc_secret()
            i_host = secret["influx_host"]
            i_port = secret["influx_port"]
            i_user = secret["influx_user"]
            i_password = secret["influx_password"]
            i_dbname = secret["influx_db"]
            g_key = secret["grafana_key"]
            g_host = secret["grafana_host"]
            g_port = secret["grafana_port"]
        else:
            i_host = self.get_arg("influx-host")
            i_port = self.get_arg("influx-port")
            i_user = self.get_arg("influx-user")
            i_password = self.get_arg("influx-password")
            if i_password is None:
                raise Exception("You need to set --perfboard-influx-password")
            i_dbname = self.get_arg("influx-db")
            g_key = self.get_arg("grafana-key")
            if g_key is None:
                raise Exception("You need to set --perfboard-grafana-key")
            g_host = self.get_arg("grafana-host")
            g_port = self.get_arg("grafana-port")

        self.client = InfluxDBClient(i_host, i_port, i_user, i_password, i_dbname)
        # this will error out if the server is unreachable
        self.client.ping()
        self.grafana = Grafana(self, g_key, g_host, g_port)

    def _build_point(self, name, component, values, date):
        value = statistics.mean(values)
        return {
            "measurement": name,
            "tags": {
                "component": component,
            },
            "time": date,
            "fields": {"Float_value": float(value)},
        }

    def run(self, metadata):
        when = datetime.datetime.utcnow()
        date = when.isoformat()
        metrics = self.get_arg("metrics")

        # Get filtered metrics
        results = filtered_metrics(
            metadata,
            self.get_arg("output"),
            self.get_arg("prefix"),
            metrics=metrics,
            transformer=self.get_arg("transformer"),
            split_by=self.get_arg("split-by"),
            simplify_names=self.get_arg("simplify-names"),
            simplify_exclude=self.get_arg("simplify-exclude"),
        )

        if not results:
            self.warning("No results left after filtering")
            return metadata

        # there's one thing we don't do yet is getting a timestamp
        # for each measure that is happening in browsertime or xpcshell
        # if we had it, we could send all 13/25 samples, each one with
        # their timestamp, to InfluxDB, and let Grafana handle the
        # mean() or median() part.
        #
        # Until we have this, here we convert the series to
        # a single value and timestamp
        self._setup()
        component = self.get_arg("dashboard")
        if component is None:
            component = metadata.script.get("component", "perftest")

        data = defaultdict(list)
        for name, res in results.items():
            for line in res:
                if "subtest" not in line:
                    continue
                metric_name = line["subtest"]
                short_name = metric_name.split(".")[-1]
                short_name = short_name.lower()
                if metrics and not any(
                    [m.lower().startswith(short_name.lower()) for m in metrics]
                ):
                    continue
                values = [v["value"] for v in line["data"]]
                data[short_name].extend(values)

        if not data:
            self.warning("No results left after filtering")
            return data

        points = []
        for metric_name, values in data.items():
            try:
                point = self._build_point(metric_name, component, values, date)
            except TypeError:
                continue
            points.append(point)

        self.info("Sending data to InfluxDB")
        self.client.write_points(points)

        # making sure we expose it in Grafana
        test_name = self.get_arg("tests")[0]
        test_name = test_name.split("/")[-1]
        for metric_name in data:
            self.grafana.add_panel(component, test_name, metric_name)

        return metadata
