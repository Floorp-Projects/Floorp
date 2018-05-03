from . import serve

def test_make_hosts_file():
    c = serve.Config(browser_host="foo.bar", alternate_hosts={"alt": "foo2.bar"})
    hosts = serve.make_hosts_file(c, "192.168.42.42")
    lines = hosts.split("\n")
    assert set(lines) == {"",
                          "0.0.0.0\tnonexistent-origin.foo.bar",
                          "0.0.0.0\tnonexistent-origin.foo2.bar",
                          "192.168.42.42\tfoo.bar",
                          "192.168.42.42\tfoo2.bar",
                          "192.168.42.42\twww.foo.bar",
                          "192.168.42.42\twww.foo2.bar",
                          "192.168.42.42\twww1.foo.bar",
                          "192.168.42.42\twww1.foo2.bar",
                          "192.168.42.42\twww2.foo.bar",
                          "192.168.42.42\twww2.foo2.bar",
                          "192.168.42.42\txn--lve-6lad.foo.bar",
                          "192.168.42.42\txn--lve-6lad.foo2.bar",
                          "192.168.42.42\txn--n8j6ds53lwwkrqhv28a.foo.bar",
                          "192.168.42.42\txn--n8j6ds53lwwkrqhv28a.foo2.bar"}
    assert lines[-1] == ""
