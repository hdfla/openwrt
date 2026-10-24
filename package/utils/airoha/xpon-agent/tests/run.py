#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Run controller and migration tests using a host ucode executable."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--ucode", default="ucode")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix="xpon-tests-") as tmp:
    work = Path(tmp)
    source = (root / "files/usr/share/rpcd/ucode/xpon.agent").read_text()
    returned = "return { 'xpon.agent': xpon_methods, 'omci.agent': methods };"
    assert source.count(returned) == 1
    source = source.replace(returned, "let controller = { 'xpon.agent': xpon_methods, 'omci.agent': methods };")
    source += "\n" + (root / "tests/backend.uc").read_text()
    test = work / "backend.uc"
    test.write_text(source)
    subprocess.run([args.ucode, "-D", "UBUS_STATUS_INVALID_ARGUMENT=2",
                    "-D", "UBUS_STATUS_UNKNOWN_ERROR=9", "-L",
                    str(root / "tests/mocks"), str(test)], check=True)

    cli = root / "files/usr/sbin/xponctl"
    for argv, method, expected in [
        (["--netdev", "pon0", "status"], "status", {"netdev": "pon0"}),
        (["link-set", "2", "1"], "link_set", {"netdev": "", "id": 2, "enabled": 1}),
        (["--json", "--device", "5", "omci", "agent", "enable"], "omci_agent_set", {"device": 5, "enabled": 1}),
        (["--omci", "--device", "5", "--json", "agent", "disable"], "omci_agent_set", {"device": 5, "enabled": 0}),
    ]:
        result = subprocess.check_output([args.ucode, "-L", str(root / "tests/mocks"),
                                          str(cli), *argv], text=True)
        reply = json.loads(result)
        assert reply == {"object": "xpon.agent", "method": method, "args": expected}, argv
        print(f"PASS: CLI {' '.join(argv)}")

    config = work / "etc/config"
    defaults = work / "usr/share/xpon"
    config.mkdir(parents=True)
    defaults.mkdir(parents=True)
    (defaults / "xpon.config").write_text("default configuration\n")
    migrate = (root / "files/etc/uci-defaults/90-xpon-config").read_text()
    migrate = migrate.replace("/etc/config", str(config))
    migrate = migrate.replace("/usr/share/xpon", str(defaults))
    script = work / "migrate.sh"
    script.write_text(migrate)
    for name, legacy, current, expected in [
        ("fresh installation", None, None, "default configuration\n"),
        ("legacy migration", "serial and credentials\n", None, "serial and credentials\n"),
        ("existing xPON preservation", "legacy\n", "custom\n", "custom\n"),
    ]:
        for file, content in [(config / "omci", legacy), (config / "xpon", current)]:
            file.unlink(missing_ok=True)
            if content is not None:
                file.write_text(content)
        subprocess.run(["sh", str(script)], check=True)
        assert (config / "xpon").read_text() == expected, name
        if legacy is not None:
            assert (config / "omci").read_text() == legacy, name
        if current is None:
            assert os.stat(config / "xpon").st_mode & 0o777 == 0o600, name
        print(f"PASS: {name}")
print("Controller and migration tests passed")
