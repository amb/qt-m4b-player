#!/usr/bin/env python3
"""Record an apt-based Linux build environment; never install or hold packages."""

import argparse
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import tempfile

MODULES = ("Qt6Core", "Qt6Gui", "Qt6Qml", "Qt6Quick", "Qt6QuickControls2", "Qt6Sql",
           "Qt6Concurrent", "Qt6Test", "mpv", "libavformat", "libavcodec", "libavutil", "libavfilter")
PACKAGES = ("build-essential", "cmake", "ninja-build", "g++", "pkg-config", "python3",
            "qt6-base-dev", "qt6-declarative-dev", "qt6-declarative-dev-tools",
            "libmpv-dev", "libavformat-dev", "libavcodec-dev", "libavutil-dev", "libavfilter-dev",
            "qml6-module-qtquick-controls", "qml6-module-qtquick-dialogs",
            "qml6-module-qtquick-layouts", "qml6-module-qtqml-workerscript", "libqt6sql6-sqlite", "libqt6svg6")
QML_MODULES = ("QtQuick", "QtQuick/Controls", "QtQuick/Controls/Basic", "QtQuick/Controls/Basic/impl",
               "QtQuick/Dialogs", "QtQuick/Dialogs/quickimpl", "QtQuick/Layouts", "QtQuick/Window",
               "QtQuick/Templates", "QtQml", "QtQml/Models", "QtQml/WorkerScript")


def run(*args, **kwargs):
    return subprocess.check_output(args, text=True, **kwargs).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    qt_libraries = Path(run("pkg-config", "--variable=libdir", "Qt6Core"))
    qml_root = Path(run("qmake6", "-query", "QT_INSTALL_QML"))
    plugins = list((qt_libraries / "qt6/plugins/sqldrivers").glob("libqsqlite.so"))
    plugins.extend((qt_libraries / "qt6/plugins/imageformats").glob("libqsvg.so"))
    for module in QML_MODULES:
        plugins.extend((qml_root / module).glob("*.so"))
    libraries = set()
    for artifact in [binary, *plugins]:
        output = run("ldd", str(artifact))
        if "not found" in output:
            raise SystemExit(f"Unresolved shared library for {artifact}:\n{output}")
        libraries.update(Path(path).resolve() for path in re.findall(r"(?:=>\s+)?(/\S+)\s+\(", output))
    libraries.update(p.resolve() for p in plugins)
    owners_output = subprocess.run(["dpkg-query", "-S", *map(str, sorted(libraries))],
                                   capture_output=True, text=True, check=False)
    owners = {}
    for line in owners_output.stdout.splitlines():
        package, separator, path = line.partition(": /")
        if separator:
            owners[str(Path("/" + path).resolve())] = package
    package_names = sorted(set(PACKAGES) | set(owners.values()))
    versions = run("dpkg-query", "-W", "-f=${binary:Package}\t${Version}\n", *package_names)
    packages = dict(line.split("\t", 1) for line in versions.splitlines())
    with tempfile.TemporaryDirectory(prefix="qt-m4b-dependencies-") as data_dir:
        environment = dict(os.environ, QT_QPA_PLATFORM="offscreen")
        runtime = run(str(binary), "--dependency-versions", "--data-dir", data_dir, env=environment)
    report = {
        "schema_version": 1,
        "policy": "apt environment record, not a lockfile",
        "os_release": Path("/etc/os-release").read_text().strip(),
        "architecture": platform.machine(),
        "tools": {tool: run(tool, "--version").splitlines()[0] for tool in ("cmake", "ninja", "c++", "pkg-config")},
        "pkg_config": {module: run("pkg-config", "--modversion", module) for module in MODULES},
        "runtime": runtime.splitlines(),
        "apt_packages": dict(sorted(packages.items())),
        "shared_libraries": {p.name: owners.get(str(p), "unowned") for p in sorted(libraries)},
        "inventoried_plugins": sorted(p.relative_to(qt_libraries).as_posix() for p in plugins),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(f"Recorded {len(libraries)} libraries and {len(packages)} packages in {args.output}")


if __name__ == "__main__":
    main()
