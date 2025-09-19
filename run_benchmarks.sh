#!/bin/bash
#set -e
IS_ARM=0
if test ! -z "$(lscpu | grep aarch64)"; then
  IS_ARM=1
fi
echo IS_ARM=$IS_ARM

ROOT_DIR=`pwd`
BENCH_DIR=run/NPB3.4.2/NPB3.4-OMP
cd $BENCH_DIR

BENCH="bt cg ep ft is lu mg sp ua"

echo "Collecting for runtime and memory overheads"
#date=`date +%y%m%d%H%M%S`
mkdir -p run-VClinic/ && cd run-VClinic/

DRRUN="$ROOT_DIR/build/bin64/drrun"
if [ "$IS_ARM" = "1" ]; then
  DRRUN="$ROOT_DIR/build/bin64/drrun -unsafe_build_ldstex"
fi
PROFILE="python2 $ROOT_DIR/mem_overhead.py"
toy_tools_relax="vprofile_mem_and_reg vprofile_mem_and_reg_read vprofile_memory vprofile_memory_read"
toy_tools_strict="vprofile_mem_and_reg_sd vprofile_mem_and_reg_read_sd vprofile_memory_sd vprofile_memory_read_sd"
client_tools="loadspy zerospy deadspy redspy"

TNUM=14
if [ "$IS_ARM" = "1" ]; then
  TNUM=32
fi

echo thread number=$TNUM

for b in $BENCH
do
  echo "Running $b ..."
  EXE="$BENCH_DIR/bin/$b.C.x"
  numactl --cpubind=0 $PROFILE ori-$TNUM- $EXE >> ori.stdout.log
  for tool in $toy_tools_relax
  do
    echo "Running $b with toy tool $tool ..."
    numactl --cpubind=0 $PROFILE vclinic-$TNUM- $DRRUN -max_bb_instrs 40 -t $tool -- $EXE >> toy.stdout.log
  done

  for tool in $toy_tools_strict
  do
    echo "Running $b with toy tool $tool ..."
    numactl --cpubind=0 $PROFILE vclinic-$TNUM- $DRRUN -max_bb_instrs 40 -t $tool -- $EXE >> toy.stdout.log
  done

  mkdir -p $b && cd $b
  for tool in $client_tools
  do
    echo "Running $b with client tool $tool ..."
    if [ "$tool" = "redspy" ]; then
      numactl --cpubind=0 $PROFILE ../vclinic-$TNUM- $DRRUN -max_bb_instrs 40 -t $tool -- $EXE > $tool.stdout.log
    else
      numactl --cpubind=0 $PROFILE ../vclinic-$TNUM- $DRRUN -t $tool -- $EXE > $tool.stdout.log
    fi
  done
  cd ..
done

echo "Finish collecting VClinic data! The collected data is located in `pwd`"
