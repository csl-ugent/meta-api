#!/usr/bin/env bash
set -euo pipefail -o functrace

version=3.7.4

. ../common.conf

liveness_dir=$PWD/diablo/liveness
meta_dir=$PWD/meta-api
diablo_dir=$PWD/diablo
build_dir=$PWD/Python-${version}/build
sequence_file=$PWD/diablo/profiles/sequencing_data.python
profile_file=$PWD/diablo/profiles/profiling_data.python
annotation_file=$PWD/diablo/annotations.json
remote_dir=python-env
install_dir=$PWD/install

function init {
  wget https://www.python.org/ftp/python/${version}/Python-${version}.tar.xz
  tar xf Python-${version}.tar.xz
  rm -rf Python-${version}_native
  cp -r Python-${version} Python-${version}_native
  (cd Python-${version} ; patch -p1 -i ../source.patch)
}

function build_native {
  (
    cd Python-${version}_native

    mkdir build-native || true
    cd build-native

    ../configure --enable-optimizations
    make -j$NJOBS altinstall
  )
}

function build_pin {
  (
    cd Python-${version}_native

    mkdir build-pin || true
    cd build-pin

    ../configure --enable-optimizations --prefix=$PWD/install
    make -j$NJOBS instal
  )
}

function build {
  (
    cd Python-${version}

    mkdir build || true
    cd build

    LDFLAGS="${CROSS_LDFLAGS}" CPPFLAGS="${CROSS_CXXFLAGS}" CFLAGS="-mfpu=vfp" ../configure --host=arm-diablo-linux-gnueabihf --build=x86_64-linux-gnu --prefix=${install_dir}/python --disable-ipv6 --enable-unicode=ucs4 ac_cv_file__dev_ptmx=no ac_cv_file__dev_ptc=no ac_cv_have_long_long_format=yes --with-openssl=${CROSS_SYSROOT}/usr

    make -j$NJOBS install
  )
}

function build_test {
  (
    cd Python-${version}_test

    patch -p1 -i ../microbenchmark.patch

    mkdir build || true
    cd build

    LDFLAGS="${CROSS_LDFLAGS}" CPPFLAGS="${CROSS_CXXFLAGS}" CFLAGS="-mfpu=vfp" ../configure --host=arm-diablo-linux-gnueabihf --build=x86_64-linux-gnu --prefix=${install_dir}/python --disable-ipv6 --enable-unicode=ucs4 ac_cv_file__dev_ptmx=no ac_cv_file__dev_ptc=no ac_cv_have_long_long_format=yes --with-openssl=${CROSS_SYSROOT}/usr

    make -j$NJOBS
  )
}

function run {
  $run_script -b $build_dir/python -c $scriptdir/test.conf -r sabre2
}

function run_measure {
  $run_script -b $build_dir/python -c $scriptdir/test.conf -r sabre2 -m 5
}

function install_on_board {
  rsync -pav ${install_dir}/python sabre:
}

function profiles_from_board {
  (
    mkdir -p $diablo_dir/profiles || true
    cd $diablo_dir/profiles
    scp sabre:$remote_dir/*_data* .

    for i in $(ls sequencing_data* | grep -v '.part' | grep -v '.merged'); do
      /opt/diablo/bin/scripts/profiles/merge-sequences.py --input $i --output $i.merged
    done

    for i in $(ls profiling_data* | grep -v '.part' | grep -v '.merged'); do
      /opt/diablo/bin/scripts/profiles/merge-profiles.py --input $i --output $i.merged
    done
  )
}

function diablo_sp {
  libname=python
  libpath=$build_dir/$libname

  echo "Generate self-profiling binary for '$libname'"
  (
    mkdir -p diablo/sp/$libname || true
    cd diablo/sp/$libname
    $DIABLO_SP_BINARY -S -O $(dirname $libpath):$build_dir -L $(dirname $libpath):$build_dir $(basename $libpath) -o $(basename $libpath) -os 1 -SP none --nospfork -lp ${CROSS_SYSROOT_LIVENESS}:$liveness_dir > log
  )
}

function run_sp {
  $run_script -b $scriptdir/diablo/sp/python/python -c $scriptdir/test.conf -r sabre2
}

function diablo_seq {
  libname=python
  libpath=$build_dir/$libname

  echo "Generate self-profiling binary for '$libname'"
  (
    mkdir -p diablo/seq/$libname || true
    cd diablo/seq/$libname
    $DIABLO_SP_BINARY -S -O $(dirname $libpath):$build_dir -L $(dirname $libpath):$build_dir $(basename $libpath) -o $(basename $libpath) -os 1 -SP none --nospfork -spseq -lp ${CROSS_SYSROOT_LIVENESS}:$liveness_dir > log
  )
}

function run_seq {
  $run_script -b $scriptdir/diablo/seq/python/python -c $scriptdir/test.conf -r sabre2
}

function diablo_obf {
  libname=python
  libpath=$build_dir/$libname
  profile_file=$diablo_dir/profiles/profiling_data.$(basename $libpath).merged
  sequence_file=$diablo_dir/profiles/sequencing_data.$(basename $libpath).merged

  echo "Generating obfuscated binary for '$libname'"
  (
    mkdir -p diablo/obf/$libname || true
    cd diablo/obf/$libname
    $DIABLO_BINARY -S -O $(dirname $libpath):$build_dir -L $(dirname $libpath):$build_dir $(basename $libpath) -o $(basename $libpath) -os 0 --rawprofiles --blocksequencefile $sequence_file --blockprofilefile $profile_file --annotation-file $annotation_file --randomseed 0 --origin-tracking on --origin-tracking-final off --final-dots on -c 1000000 -mav 0 -madbg off -maver off -matwoway on -lp ${CROSS_SYSROOT_LIVENESS}:$liveness_dir -aop_xml $meta_dir/api.xml -Ost on > log
  )
}

function run_obf {
  $run_script -b $scriptdir/diablo/obf/python/python -c $scriptdir/test.conf -r sabre2
}

function annotate_profile {
  libname=python
  mapfile=$build_dir/$libname.map

  echo "Annotate map file with execution profile for '$libname'"
  (
    cd diablo/profiles/
    $annotate_script $mapfile profiling_data.$libname > profiling_data.$libname.annot
  )
}

command=$1
shift
$command $@
