#!/usr/bin/env bash
set -euo pipefail

scriptdir=$(cd $(dirname $0) && pwd)
installdir=$scriptdir/../../usecases/radare2/radare2-pin/install/

plugin=pairs.so

function pin {
  LD_LIBRARY_PATH=$installdir/lib /build/pin-3.16-98275-ge0db48c31-gcc-linux/pin -t $scriptdir/../plugin/$plugin -- $installdir/bin/r2 /bin/ls < $installdir/../../input.txt > stdout.log 2> stderr.log
}

function branch {
  plugin=cond_branch.so

  (
    mkdir -p branch || true
    cd branch

    pin

    $scriptdir/../uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err
    $scriptdir/../measure_uniform.py -d 'uniform.log' > measure.log 2> measure.err
  )
}

function pair {
  plugin=pairs.so

  (
    mkdir -p pair || true
    cd pair

    pin

    $scriptdir/../uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err
    $scriptdir/../measure_pairs.py -d 'uniform.log' -m 3 > measure.log 2> measure.err
  )
}

$1
