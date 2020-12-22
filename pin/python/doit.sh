#!/usr/bin/env bash
set -ueo pipefail
set -x

scriptdir=$(cd $(dirname $0) && pwd)
prefixdir=DOES_NOT_EXIST
pythondir=$scriptdir/../../usecases/python/Python-3.7.4_native/build-pin/install
pythonbin=$pythondir/bin/python3
pinbin=/build/pin-3.16-98275-ge0db48c31-gcc-linux/pin
plugin=cond_branch.so

function python_command {
  PYTHONPATH=$prefixdir/lib/python3.7/site-packages NLTK_DATA=$prefixdir/lib/python3.7/site-packages/nltk/tokenize/ $pinbin -t $scriptdir/../plugin/$plugin -- $pythonbin "$@" > pin.log
}

function install_modules {
  rm -r $prefixdir || true

  clean
  python_command $scriptdir/get-pip.py install --prefix=$prefixdir --no-cache-dir
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
  python_command $scriptdir/homer-prepare.py
  move_pin_output homer-prepare
  clean
  python_command -m homer.homer_cmd --name moby_dick --author herman_melville --file_path $scriptdir/mobydick.txt
  move_pin_output homer

  clean
  python_command -m steganographer -o ./trollface-stego.jpg -m "it works!" $scriptdir/trollface.jpg
  move_pin_output steganographer
  mv trollface-stego.jpg steganographer/
}

function collect_data {
(cd get-pip ; ../../../uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err)
(cd speedtest-install ; ../../../uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err)
(cd homer-text-install ; ../../../uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err)
(cd steganographer-install ; ../../../uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err)
(cd speedtest ; ../../../uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err)
(cd homer-prepare ; ../../../uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err)
(cd homer ; ../../../uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err)
(cd steganographer ; ../../../uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err)

$scriptdir/../measure_uniform.py -d '*/uniform.log' > measure.log 2> measure.err
}

function branch {
(
  mkdir -p branch || true
  cd branch

  plugin=cond_branch.so
  prefixdir=$PWD/prefix

  clean
  install_modules
  run

  collect_data
)
}

function pair {
(
  mkdir -p pair || true
  cd pair

  plugin=pairs.so
  prefixdir=$PWD/prefix

  clean
  install_modules
  run

  collect_data
)
}

$1
