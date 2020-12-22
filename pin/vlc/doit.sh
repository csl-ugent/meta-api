#!/usr/bin/env bash
set -euo pipefail

scriptdir=$(cd $(dirname $0) && pwd)

plugin=pairs.so

function pin {
  $scriptdir/../docker/build/pin-3.16-98275-ge0db48c31-gcc-linux/pin -t $scriptdir/../plugin/$plugin -- vlc > stdout.log 2> stderr.log
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

    # pin

    $scriptdir/../uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err
    $scriptdir/../measure_pairs.py -d 'uniform.log' > measure.log 2> measure.err

  )
}

$1
