#!/usr/bin/env bash
set -euo pipefail -o functrace

. ../common.conf

liveness_dir=$PWD/diablo/liveness
meta_dir=$PWD/meta-api
diablo_dir=$PWD/diablo
build_dir=$PWD/keepassxc/build
build_test_dir=$PWD/keepassxc_test/build
sequence_file=$PWD/diablo/profiles/sequencing_data.keepassxc-cli
profile_file=$PWD/diablo/profiles/profiling_data.keepassxc-cli
annotation_file=$PWD/diablo/annotations.json
remote_dir=keepassxc
install_dir=$PWD/install

function init {
  git clone https://github.com/keepassxreboot/keepassxc.git
  (
    cd keepassxc
    git checkout b9daed20558103e3c8e799f3e0575522bf823df6
    patch -p1 -i ../source.patch
  )
}

function build {
  (
    mkdir -p $build_dir || true
    cd $build_dir

    LDFLAGS="${CROSS_LDFLAGS}" CXXFLAGS="${CROSS_CXXFLAGS}" cmake -DCMAKE_TOOLCHAIN_FILE=${CROSS_ROOT}/_helpers/cmake.cross -DCMAKE_INSTALL_PREFIX=${install_dir}/keepassxc ..
    for i in $(grep -rl lrelease); do sed -i "s+${CROSS_SYSROOT}/usr/lib/qt5/bin/lrelease+/usr/bin/lrelease+g" $i; done
    for i in $(grep -rl qt5/bin/moc); do sed -i "s+${CROSS_SYSROOT}/usr/lib/qt5/bin/moc+/usr/bin/moc+g" $i; done
    for i in $(grep -rl qt5/bin/uic); do sed -i "s+${CROSS_SYSROOT}/usr/lib/qt5/bin/uic+/usr/bin/uic+g" $i; done

    make -j16
    make install
  )
}

function build_test {
  (
    mkdir -p $build_test_dir || true
    cd $build_test_dir

    patch -p1 -i ../../microbenchmark.patch

    LDFLAGS="${CROSS_LDFLAGS}" CXXFLAGS="${CROSS_CXXFLAGS}" cmake -DCMAKE_TOOLCHAIN_FILE=${CROSS_ROOT}/_helpers/cmake.cross -DCMAKE_INSTALL_PREFIX=${install_dir}/keepassxc ..
    for i in $(grep -rl lrelease); do sed -i "s+${CROSS_SYSROOT}/usr/lib/qt5/bin/lrelease+/usr/bin/lrelease+g" $i; done
    for i in $(grep -rl qt5/bin/moc); do sed -i "s+${CROSS_SYSROOT}/usr/lib/qt5/bin/moc+/usr/bin/moc+g" $i; done
    for i in $(grep -rl qt5/bin/uic); do sed -i "s+${CROSS_SYSROOT}/usr/lib/qt5/bin/uic+/usr/bin/uic+g" $i; done

    make -j16
  )
}

function build_pin {
  git clone https://github.com/keepassxreboot/keepassxc.git keepassxc-pin || true

  (
    cd keepassxc-pin
    git checkout b9daed20558103e3c8e799f3e0575522bf823df6

    mkdir -p build || true
    cd build

    cmake -DCMAKE_INSTALL_PREFIX=${PWD}/install ..

    make -j16
    make install
  )
}

function install_on_board {
  rsync -pav ${install_dir}/keepassxc sabre:
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
  libname=keepassxc-cli
  libpath=$build_dir/src/cli/$libname

  echo "Generate self-profiling binary for '$libname'"
  (
    mkdir -p diablo/sp/$libname || true
    cd diablo/sp/$libname
    $DIABLO_SP_BINARY -S -O $(dirname $libpath):$build_dir -L $(dirname $libpath):$build_dir $(basename $libpath) -o $(basename $libpath) -os 1 -SP none --nospfork -lp ${CROSS_SYSROOT_LIVENESS}:$liveness_dir > log
  )
}

function run_sp {
  $run_script -b $scriptdir/diablo/sp/keepassxc-cli/keepassxc-cli -c $scriptdir/test.conf -r sabre2
}

function diablo_seq {
  libname=keepassxc-cli
  libpath=$build_dir/src/cli/$libname

  echo "Generate self-profiling binary for '$libname'"
  (
    mkdir -p diablo/seq/$libname || true
    cd diablo/seq/$libname
    $DIABLO_SP_BINARY -S -O $(dirname $libpath):$build_dir -L $(dirname $libpath):$build_dir $(basename $libpath) -o $(basename $libpath) -os 1 -SP none --nospfork -spseq -lp ${CROSS_SYSROOT_LIVENESS}:$liveness_dir > log
  )
}

function run_seq {
  $run_script -b $scriptdir/diablo/seq/keepassxc-cli/keepassxc-cli -c $scriptdir/test.conf -r sabre2
}

function diablo_obf {
  libname=keepassxc-cli
  libpath=$build_dir/src/cli/$libname
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
  $run_script -b $scriptdir/diablo/obf/keepassxc-cli/keepassxc-cli -c $scriptdir/test.conf -r sabre2
}

function annotate_profile {
  libname=keepassxc-cli
  libfile=$build_dir/src/cli/$libname
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
