#!/usr/bin/env bash
set -euo pipefail

scriptdir=$(cd $(dirname $0) && pwd)
ninja=$scriptdir/../../usecases/ninja/ninja-pin/build/install/

function branch {
(
  git clone https://github.com/ninja-build/ninja.git branch || true
  cd branch
  git checkout 2ca4c711f77d9cb95e3698de70d07823f4baa256

  ./configure.py

  # execute pin
  /build/pin-3.16-98275-ge0db48c31-gcc-linux/pin -t $scriptdir/../plugin/cond_branch.so -- $ninja/bin/ninja -n -v > log
  $scriptdir/../uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err
  $scriptdir/../measure_uniform.py -d './uniform.log' > measure.log 2> measure.err
)
}

function pair {
  git clone https://github.com/ninja-build/ninja.git pair || true
  cd pair
  git checkout 2ca4c711f77d9cb95e3698de70d07823f4baa256

  ./configure.py

  # execute pin
  /build/pin-3.16-98275-ge0db48c31-gcc-linux/pin -t $scriptdir/../plugin/pairs.so -- $ninja/bin/ninja -n -v > log

  $scriptdir/../uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err
  $scriptdir/../measure_pairs.py -d './uniform.log' -m 3 > measure.log 2> measure.err
}

$1
