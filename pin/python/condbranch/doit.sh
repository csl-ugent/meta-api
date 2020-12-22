#!/usr/bin/env bash
set -ueo pipefail
set -x

scriptdir=$(cd $(dirname $0) && pwd)
prefixdir=$scriptdir/prefix
pythondir=$scriptdir/../../../usecases/python/Python-3.7.4_native/build-pin/install
pythonbin=$pythondir/bin/python3
pinbin=$scriptdir/../../docker/build/pin-3.16-98275-ge0db48c31-gcc-linux/pin
pinplugin=$scriptdir/../../plugin/cond_branch.so

export PYTHONPATH=$prefixdir/lib/python3.7/site-packages
export NLTK_DATA=$prefixdir/lib/python3.7/site-packages/nltk/tokenize/

function python_command {
  $pinbin -t $pinplugin -- $pythonbin "$@" > pin.log
}

function install_modules {
  rm -r $prefixdir || true

  clean
  python_command ../get-pip.py install --prefix=$PWD/prefix --no-cache-dir
  move_pin_output get-pip

  clean
  python_command -m pip install --prefix=$prefixdir speedtest-cli
  move_pin_output speedtest-install

  clean
  python_command -m pip install --prefix=$prefixdir homer-text
  move_pin_output homer-text-install

  clean
  python_command -m pip install --prefix=$prefixdir steganographer
  move_pin_output steganographer-install
}

function clean {
  find $pythondir -name '*.pyc' -exec rm {} \; || true
  find $prefixdir -name '*.pyc' -exec rm {} \; || true
}

function move_pin_output {
  destination=$1

  mkdir -p $destination || true
  mv pin.log $destination
  mv data_*.log $destination
  mv sections_*.log $destination
}

function run {
  clean
  python_command -m speedtest
  move_pin_output speedtest

  clean
  python_command ../homer-prepare.py
  move_pin_output homer-prepare
  clean
  python_command -m homer.homer_cmd --name moby_dick --author herman_melville --file_path ../mobydick.txt
  move_pin_output homer

  clean
  python_command -m steganographer -o ./trollface-stego.jpg -m "it works!" ../trollface.jpg
  move_pin_output steganographer
  mv trollface-stego.jpg steganographer/
}

$1
