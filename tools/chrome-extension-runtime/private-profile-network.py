#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Opt-in environment networking for one disposable native browser profile."""
import hashlib
from pathlib import Path
import re
import ssl
from urllib.parse import urlsplit


def environment_network(environment):
    """Return native proxy preferences and public CA certificates, never secrets."""
    preferences = {"network.proxy.type": 1}
    proxies = {}
    for name in ("http", "https"):
        value = environment.get(name + "_proxy") or environment.get(name.upper() + "_PROXY")
        value = value or environment.get("all_proxy") or environment.get("ALL_PROXY")
        if not value:
            continue
        parsed = urlsplit(value)
        if parsed.username is not None or parsed.password is not None:
            raise ValueError("Environment proxy credentials require an explicit native authentication fixture")
        if parsed.scheme != "http" or not parsed.hostname or parsed.path not in ("", "/") or parsed.query or parsed.fragment:
            raise ValueError("The environment proxy must be an HTTP proxy URL")
        try:
            port = parsed.port or 80
        except ValueError as error:
            raise ValueError("The environment proxy port is invalid") from error
        prefix = "network.proxy." + ("ssl" if name == "https" else "http")
        preferences[prefix] = parsed.hostname
        preferences[prefix + "_port"] = port
        proxies[name] = {"scheme": "http", "port": port}
    if not proxies:
        preferences["network.proxy.type"] = 0
    preferences["network.proxy.share_proxy_settings"] = False
    bypass = [part.strip() for part in (environment.get("no_proxy") or environment.get("NO_PROXY") or "").split(",") if part.strip()]
    # Every suite's authored loopback fixtures stay local even without NO_PROXY.
    bypass.extend(["localhost", "127.0.0.0/8", "::1"])
    preferences["network.proxy.no_proxies_on"] = ",".join(dict.fromkeys(bypass))
    bundle = next((environment[key] for key in ("SSL_CERT_FILE", "REQUESTS_CA_BUNDLE", "CURL_CA_BUNDLE") if environment.get(key)), None)
    bundle = bundle or ssl.get_default_verify_paths().cafile
    if not bundle:
        raise ValueError("No normal public CA bundle is available for the private profile")
    encoded = Path(bundle).read_bytes()
    certificates = re.findall(rb"-----BEGIN CERTIFICATE-----\s*([A-Za-z0-9+/=\s]+?)\s*-----END CERTIFICATE-----", encoded)
    if not certificates:
        raise ValueError("The normal public CA bundle contains no PEM certificates")
    certificates = [re.sub(rb"\s", b"", certificate).decode("ascii") for certificate in certificates]
    return preferences, certificates, {
        "mode": "environment-proxy-private-ca",
        "proxies": proxies,
        "loopbackBypass": True,
        "environmentNoProxyHonored": True,
        "publicCaBundleSha256": hashlib.sha256(encoded).hexdigest(),
        "publicCaCertificates": len(certificates),
        "certificateValidationDisabled": False,
        "hostTrustModified": False,
    }
