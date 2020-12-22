#!/usr/bin/env bash
set -euo pipefail -o functrace

. ../common.conf

liveness_dir=$PWD/diablo/liveness
meta_dir=$PWD/meta-api
diablo_dir=$PWD/diablo
build_dir=$PWD/ninja/build
sequence_file=$PWD/diablo/profiles/sequencing_data.ninja
profile_file=$PWD/diablo/profiles/profiling_data.ninja
annotation_file=$PWD/diablo/annotations.json
remote_dir=ninja
install_dir=$PWD/install

function init {
  git clone https://github.com/ninja-build/ninja.git
  (
    cd ninja
    git checkout 2ca4c711f77d9cb95e3698de70d07823f4baa256
    patch -p1 -i $scriptdir/source.patch
  )
}

function build {
  (
    mkdir -p $build_dir || true
    cd $build_dir

    cmake -DCMAKE_INSTALL_PREFIX=${install_dir}/ninja -DCMAKE_TOOLCHAIN_FILE=${CROSS_ROOT}/_helpers/cmake.cross -DCMAKE_BUILD_TYPE=Debug ..

    make -j16
    make install
  )
}

function build_test {
  (
    cd ninja_test

    patch -p1 -i ../microbenchmark.patch

    mkdir -p build || true
    cd build

    cmake -DCMAKE_INSTALL_PREFIX=${install_dir}/ninja -DCMAKE_TOOLCHAIN_FILE=${CROSS_ROOT}/_helpers/cmake.cross -DCMAKE_BUILD_TYPE=Debug ..

    make -j16
  )
}

function build_pin {
  (
    cd ninja-pin

    mkdir -p build || true
    cd build

    cmake -DCMAKE_INSTALL_PREFIX=${PWD}/install ..

    make -j16
    make install
  )
}

function install_on_board {
  rsync -pav ${install_dir}/ninja sabre:
}

function profiles_from_board {
  (
    mkdir -p $diablo_dir/profiles || true
    cd $diablo_dir/profiles
    scp sabre:$remote_dir/work/ninja/*_data.ninja .

    for i in $(ls sequencing_data* | grep -v '.part' | grep -v '.merged'); do
      /opt/diablo/bin/scripts/profiles/merge-sequences.py --input $i --output $i.merged
    done

    for i in $(ls profiling_data* | grep -v '.part' | grep -v '.merged'); do
      /opt/diablo/bin/scripts/profiles/merge-profiles.py --input $i --output $i.merged
    done
  )
}

function diablo_sp {
  libname=ninja
  libpath=$build_dir/$libname

  echo "Generate self-profiling binary for '$libname'"
  (
    mkdir -p diablo/sp/$libname || true
    cd diablo/sp/$libname
    $DIABLO_SP_BINARY -S -O $(dirname $libpath):$build_dir:$build_dir/src -L $(dirname $libpath):$build_dir $(basename $libpath) -o $(basename $libpath) -os 1 -SP none --nospfork -lp ${CROSS_SYSROOT_LIVENESS}:$liveness_dir > log
  )
}

function run_sp {
  $run_script -b $scriptdir/diablo/sp/ninja/ninja -c $scriptdir/test.conf -r sabre2
}

function diablo_seq {
  libname=ninja
  libpath=$build_dir/$libname

  echo "Generate self-sequencing binary for '$libname'"
  (
    mkdir -p diablo/seq/$libname || true
    cd diablo/seq/$libname
    $DIABLO_SP_BINARY -S -O $(dirname $libpath):$build_dir:$build_dir/src -L $(dirname $libpath):$build_dir $(basename $libpath) -o $(basename $libpath) -os 1 -SP none --nospfork -spseq -lp ${CROSS_SYSROOT_LIVENESS}:$liveness_dir > log
  )
}

function run_seq {
  $run_script -b $scriptdir/diablo/seq/ninja/ninja -c $scriptdir/test.conf -r sabre2
}

function diablo_obf {
  libname=ninja
  libpath=$build_dir/ninja
  profile_file=$diablo_dir/profiles/profiling_data.$(basename $libpath).merged
  sequence_file=$diablo_dir/profiles/sequencing_data.$(basename $libpath).merged

  echo "Generating obfuscated binary for '$libname'"
  (
    mkdir -p diablo/obf/$libname || true
    cd diablo/obf/$libname
    $DIABLO_BINARY -S -O $(dirname $libpath):$build_dir -L $(dirname $libpath):$build_dir $(basename $libpath) -o $(basename $libpath) -os 0 --rawprofiles --blockprofilefile $profile_file --blocksequencefile $sequence_file --annotation-file $annotation_file --randomseed 0 --origin-tracking on --origin-tracking-final off --final-dots on -c 1000000 -mav 0 -madbg off -maver off -matwoway off -lp ${CROSS_SYSROOT_LIVENESS}:$liveness_dir -aop_xml $meta_dir/api.xml > log
  )
}

function run_obf {
  $run_script -b $scriptdir/diablo/obf/ninja/ninja -c $scriptdir/test.conf -r sabre2
}

function annotate_profile {
  libname=ninja
  libfile=$build_dir/ninja
  mapfile=$libfile.map

  echo "Annotate map file with execution profile for '$libname'"
  (
    cd diablo/profiles
    $annotate_script $mapfile $diablo_dir/profiles/profiling_data.$libname > profiling_data.$libname.annot
  )
}

command=$1
shift
$command $@
