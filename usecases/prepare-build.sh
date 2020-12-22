#!/usr/bin/env bash
set -euo pipefail -o functrace

datadir=$(git rev-parse --show-toplevel)/data

echo "=== installing Qt 5.9.5"
echo "  /opt/Qt5.9.5"
echo '  Check "Qt" > "Qt 5.9.5" > "Desktop gcc 64-bit"'
$datadir/qt-opensource-linux-x64-5.9.5.run

apt-get install -y qt5-default
qtchooser -install qt595 /opt/Qt5.9.5/5.9.5/gcc_64/bin/qmake
