#!/usr/bin/env bash
set -euo pipefail

scriptdir=$(cd $(dirname $0) && pwd)
installdir=$scriptdir/../../usecases/keepassxc/keepassxc-pin/build/install/
datadir=$installdir/../../tests/data

# plugin=cond_branch.so
plugin=pairs.so

counter=0
function command {
  ((counter+=1))

  (
    echo "command $counter"

    rm -r $counter || true

    mkdir $counter || true
    cd $counter

    cp ../NewDatabase.kdbx .
    /build/pin-3.16-98275-ge0db48c31-gcc-linux/pin -t $scriptdir/../plugin/$plugin -- $installdir/bin/keepassxc-cli "$@" < $scriptdir/input > log
    cp NewDatabase.kdbx ..
  )
}

function run {
(
  cp $datadir/NewDatabase.kdbx .

  #1
  command add -u newuser --url https://example.com -g -L 20 NewDatabase.kdbx /newuser-entry
  #2 0
  command show -s NewDatabase.kdbx /newuser-entry
  #3
  command add -q -u newuser -g -L 20 NewDatabase.kdbx /newentry-quiet
  #4 1
  command show -s NewDatabase.kdbx /newentry-quiet
  #5
  command add -u newuser2 --url https://example.net -p NewDatabase.kdbx /newuser-entry2
  #6 2
  command show -s NewDatabase.kdbx /newuser-entry2
  #7
  command add -u newuser3 -g -L 34 NewDatabase.kdbx /newuser-entry3
  #8 3
  command show -s NewDatabase.kdbx /newuser-entry3
  #9
  command mkdir NewDatabase.kdbx /new_group
  #10 4
  command mkdir NewDatabase.kdbx /new_group/newer_group
  #11
  command analyze --hibp $datadir/hibp.txt NewDatabase.kdbx
  #12 5
  command clip NewDatabase.kdbx "Sample Entry"
  #13
  command db-create testCreate1.kdbx
  #14 6
  command db-create testCreate2.kdbx -k keyfile.txt
  #15
  command db-create testCreate3.kdbx -k keyfile.txt
  #16 7
  command db-create testCreate4.kdbx -t 500
  #17
  command rmdir NewDatabase.kdbx "/General"
  #18 8
  command rmdir NewDatabase.kdbx "/Recycle Bin/General"
  #19
  command rm -q NewDatabase.kdbx "/Sample Entry"
  #20 9
  command locate NewDatabase.kdbx "Sample Entry"
  #21
  command ls NewDatabase.kdbx

  # cp $installdir/../../tests/data/* .
)
}

function branch {
  plugin=cond_branch.so

  mkdir -p branch || true
  cd branch

  run

  for d in $(ls -d */); do
    echo $d
    (cd $d ; $scriptdir/../uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err)
  done

  $scriptdir/../measure_uniform.py -d '*/uniform.log' > measure.log 2> measure.err
}

function pairs {
  plugin=pairs.so

  mkdir -p pair || true
  cd pair

  run

  for d in $(ls -d */); do
    echo $d
    (cd $d ; $scriptdir/../uniformize_pin_output.py -d './data_*.log' -s './sections_*.log' > uniform.log 2> uniform.err)
  done

  # many inputs, so need to limit the number of combinations.
  odd=()
  for i in $(seq 1 2 21); do
    odd+=(-d ./$i/uniform.log)
  done
  $scriptdir/../measure_pairs.py ${odd[@]} -m 3 > measure-odd.log 2> measure-odd.err

  even=()
  for i in $(seq 2 2 21); do
    even+=(-d ./$i/uniform.log)
  done
  $scriptdir/../measure_pairs.py ${even[@]} -m 3 > measure-even.log 2> measure-even.err
}

$1
