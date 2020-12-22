#!/usr/bin/env bash
set -euo pipefail -o functrace

. ../common.conf

build_dir=$PWD/radare2
diablo_dir=$PWD/diablo
sequence_file=$diablo_dir/seq/libr_util/outputs/profiles/sequencing_data.libr_util.so
profile_file=$diablo_dir/sp/libr_util/outputs/profiles/profiling_data.libr_util.so
annotation_file=$diablo_dir/annotations.json
liveness_dir=$diablo_dir/liveness
meta_dir=$PWD/meta-api
remote_dir=radare2
install_dir=$PWD/install

function init {
  git clone https://github.com/radare/radare2.git
  (
    cd radare2
    git checkout tags/3.8.0
    patch -p1 -i ../source.patch
  )
}

function build {
  (
    cd radare2

    ./configure --host=arm-diablo-linux-gnueabihf --prefix=${install_dir}/radare2 --with-compiler=armhf
    CFLAGS="${CROSS_CXXFLAGS}" LDFLAGS="${CROSS_LDFLAGS} -Wl,--no-relax" CC=arm-diablo-linux-gnueabihf-gcc make
    make install
  )
}

function build_pin {
  (
    cd radare2-pin

    ./configure --prefix=${PWD}/install
    make
    make install
  )
}

function build_test {
  (
    cd radare2_test

    patch -p1 -i ../microbenchmark.patch

    ./configure --host=arm-diablo-linux-gnueabihf --prefix=${install_dir}/radare2 --with-compiler=armhf
    CFLAGS="${CROSS_CXXFLAGS}" LDFLAGS="${CROSS_LDFLAGS} -Wl,--no-relax" CC=arm-diablo-linux-gnueabihf-gcc make
  )
}

function install_on_board {
  rsync -pav ${install_dir}/radare2 sabre:
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

function run {
  $run_script -b $build_dir/libr/util/libr_util.so -c $scriptdir/test.conf -r sabre2
}

# for i in fs io core parse egg bin main syscall lang search magic asm anal crypto cons hash debug flag reg util config bp socket; do ./setup.sh annotate_profile_libr $i; done

# fs, io, core, parse, egg, bin, main, syscall, lang, search, magic
# asm, anal, crypto, cons, hash, debug, flag, reg, util, config, bp, socket
function diablo_sp_libr {
  libname=libr_$1
  libpath=$build_dir/libr/$1/$libname.so

  echo "Generate self-profiling binary for '$libname'"
  (
    mkdir -p diablo/sp/$libname || true
    cd diablo/sp/$libname
    $DIABLO_SP_BINARY -S -O $(dirname $libpath) -L $(dirname $libpath) $(basename $libpath) -o $(basename $libpath) -os 1 -SP none --nospfork -lp ${CROSS_SYSROOT_LIVENESS}:$liveness_dir > log
  )
}

function diablo_sp {
  diablo_sp_libr util
}

function run_sp {
  $run_script -b $diablo_dir/sp/libr_util/libr_util.so -c $scriptdir/test.conf -r sabre2
}

function diablo_seq_libr {
  libname=libr_$1
  libpath=$build_dir/libr/$1/$libname.so

  echo "Generate self-profiling binary for '$libname'"
  (
    mkdir -p diablo/seq/$libname || true
    cd diablo/seq/$libname
    $DIABLO_SP_BINARY -S -O $(dirname $libpath) -L $(dirname $libpath) $(basename $libpath) -o $(basename $libpath) -os 1 -SP none --nospfork -spseq -lp ${CROSS_SYSROOT_LIVENESS}:$liveness_dir > log
  )
}

function diablo_seq {
  diablo_seq_libr util
}

function run_seq {
  $run_script -b $diablo_dir/seq/libr_util/libr_util.so -c $scriptdir/test.conf -r sabre2
}

function diablo_obf_libr {
  libname=libr_$1
  libpath=$build_dir/libr/$1/$libname.so
  profile_file=$diablo_dir/profiles/profiling_data.$(basename $libpath).merged
  sequence_file=$diablo_dir/profiles/sequencing_data.$(basename $libpath).merged

  echo "Generating obfuscated binary for '$libname'"
  (
    mkdir -p diablo/obf/$libname || true
    cd diablo/obf/$libname
    $DIABLO_BINARY -S -O $(dirname $libpath):$build_dir -L $(dirname $libpath):$build_dir $(basename $libpath) -o $(basename $libpath) -os 0 --rawprofiles --blocksequencefile $sequence_file --blockprofilefile $profile_file --annotation-file $annotation_file --randomseed 0 --origin-tracking on --origin-tracking-final off --final-dots on -c 1000000 -mav 0 -madbg off -maver off -matwoway on -lp ${CROSS_SYSROOT_LIVENESS}:$liveness_dir -aop_xml $meta_dir/api.xml -Ost on > log
  )
}

function diablo_obf {
  diablo_obf_libr util
}

function run_obf {
  $run_script -b $diablo_dir/obf/libr_util/libr_util.so -c $scriptdir/test.conf -r sabre2
}

# fs, io, core, parse, egg, bin, main, syscall, lang, search, magic
# asm, anal, crypto, cons, hash, debug, flag, reg, util, config, bp, socket
function annotate_profile_libr {
  libname=libr_$1
  mapfile=$build_dir/libr/$1/$libname.so.map

  echo "Annotate map file with execution profile for '$libname'"
  (
    cd diablo/profiles/
    $annotate_script $mapfile profiling_data.$libname.so > profiling_data.$libname.so.annot
  )
}

command=$1
shift
$command $@
