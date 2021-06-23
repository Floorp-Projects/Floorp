# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import json

from marionette_driver.transport import Command, Response

from marionette_harness import MarionetteTestCase


get_current_url = ("getCurrentUrl", None)
execute_script = ("executeScript", {"script": "return 42"})


class TestMessageSequencing(MarionetteTestCase):
    @property
    def last_id(self):
        return self.marionette.client.last_id

    @last_id.setter
    def last_id(self, new_id):
        self.marionette.client.last_id = new_id

    def send(self, name, params):
        self.last_id = self.last_id + 1
        cmd = Command(self.last_id, name, params)
        self.marionette.client.send(cmd)
        return self.last_id


class MessageTestCase(MarionetteTestCase):
    def assert_attr(self, obj, attr):
        self.assertTrue(
            hasattr(obj, attr), "object does not have attribute {}".format(attr)
        )


class TestCommand(MessageTestCase):
    def create(self, msgid="msgid", name="name", params="params"):
        return Command(msgid, name, params)

    def test_initialise(self):
        cmd = self.create()
        self.assert_attr(cmd, "id")
        self.assert_attr(cmd, "name")
        self.assert_attr(cmd, "params")
        self.assertEqual("msgid", cmd.id)
        self.assertEqual("name", cmd.name)
        self.assertEqual("params", cmd.params)

    def test_stringify(self):
        cmd = self.create()
        string = str(cmd)
        self.assertIn("Command", string)
        self.assertIn("id=msgid", string)
        self.assertIn("name=name", string)
        self.assertIn("params=params", string)

    def test_to_msg(self):
        cmd = self.create()
        msg = json.loads(cmd.to_msg())
        self.assertEquals(msg[0], Command.TYPE)
        self.assertEquals(msg[1], "msgid")
        self.assertEquals(msg[2], "name")
        self.assertEquals(msg[3], "params")

    def test_from_msg(self):
        msg = [Command.TYPE, "msgid", "name", "params"]
        cmd = Command.from_msg(msg)
        self.assertEquals(msg[1], cmd.id)
        self.assertEquals(msg[2], cmd.name)
        self.assertEquals(msg[3], cmd.params)


class TestResponse(MessageTestCase):
    def create(self, msgid="msgid", error="error", result="result"):
        return Response(msgid, error, result)

    def test_initialise(self):
        resp = self.create()
        self.assert_attr(resp, "id")
        self.assert_attr(resp, "error")
        self.assert_attr(resp, "result")
        self.assertEqual("msgid", resp.id)
        self.assertEqual("error", resp.error)
        self.assertEqual("result", resp.result)

    def test_stringify(self):
        resp = self.create()
        string = str(resp)
        self.assertIn("Response", string)
        self.assertIn("id=msgid", string)
        self.assertIn("error=error", string)
        self.assertIn("result=result", string)

    def test_to_msg(self):
        resp = self.create()
        msg = json.loads(resp.to_msg())
        self.assertEquals(msg[0], Response.TYPE)
        self.assertEquals(msg[1], "msgid")
        self.assertEquals(msg[2], "error")
        self.assertEquals(msg[3], "result")

    def test_from_msg(self):
        msg = [Response.TYPE, "msgid", "error", "result"]
        resp = Response.from_msg(msg)
        self.assertEquals(msg[1], resp.id)
        self.assertEquals(msg[2], resp.error)
        self.assertEquals(msg[3], resp.result)
