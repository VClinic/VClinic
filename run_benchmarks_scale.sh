#!/bin/bash
set -e
IS_ARM=0
if test ! -z "$(lscpu | grep aarch64)"; then
  IS_ARM=1
fi
echo IS_ARM=$IS_ARM

ROOT_DIR=`pwd`
BENCH_DIR=run/NPB3.4.2/NPB3.4-OMP/
cd $BENCH_DIR

BENCH="bt cg ep ft is lu mg sp ua"

echo "Collecting for runtime and memory overheads"
mkdir -p run-VClinic/ && cd run-VClinic/

DRRUN="$ROOT_DIR/build/bin64/drrun"
if [ "$IS_ARM" = "1" ]; then
  DRRUN="$ROOT_DIR/build/bin64/drrun -unsafe_build_ldstex"
fi
PROFILE="python2 $ROOT_DIR/mem_overhead.py"
toy_tools_relax="vprofile_mem_and_reg vprofile_mem_and_reg_read vprofile_memory vprofile_memory_read"
toy_tools_strict="vprofile_mem_and_reg_sd vprofile_mem_and_reg_read_sd vprofile_memory_sd vprofile_memory_read_sd"
client_tools="loadspy zerospy deadspy redspy"

echo "Collecting scalability data"
scale="8 4 2 1"
if [ "$IS_ARM" = "1" ]; then
  scale="16 8 4 2 1"
fi
for i in $scale
do
  export OMP_NUM_THREADS=$i
  for b in $BENCH
  do
    EXE="../bin/$b.C.x"
    echo "Running $b with $i threads ..."
    numactl --cpubind=0 $PROFILE ori-$i- $EXE > ori.stdout.log
    for tool in $toy_tools_relax
    do
      echo "Running $b ($i) with toy tool $tool ..."
      numactl --cpubind=0 $PROFILE vclinic-$i- $DRRUN -max_bb_instrs 40 -t $tool -- $EXE > toy.stdout.log
    done
  done
  unset OMP_NUM_THREADS
done

echo "Finish collecting VClinic Scalability data! The collected data is located in `pwd`"
