#!/bin/bash
set -e
ROOT_DIR=`pwd`
mkdir -p run && cd run
echo "Downloading NPB 3.4.2 benchmarks ..."
wget https://www.nas.nasa.gov/assets/npb/NPB3.4.2.tar.gz
tar xvzf NPB3.4.2.tar.gz

echo "Building NPB-OMP..."
cd NPB3.4.2/NPB3.4-OMP/
patch ./config/make.def.template -i $ROOT_DIR/scripts/bench_tool/NPB3.4-OMP.make.patch -o ./config/make.def

BENCH="bt cg ep ft is lu mg sp ua"

for b in $BENCH
do
  make CLASS=C $b
done

echo "Collecting for runtime and memory overheads"
mkdir -p run/ && cd run/

EXE=$ROOT_DIR/build/bin64/drrun
PROFILE="python2 $ROOT_DIR/mem_overhead.py"
toy_tools_relax="vprofile_mem_and_reg vprofile_mem_and_reg_read vprofile_memory vprofile_memory_read"
toy_tools_strict="vprofile_mem_and_reg_sd vprofile_mem_and_reg_read_sd vprofile_memory_sd vprofile_memory_read_sd"
client_tools="loadspy zerospy deadspy redspy"

TNUM=14

for b in $BENCH
do
  echo "Running $b ..."
  numactl --cpubind=0 $PROFILE ori-$TNUM- echo ./$b.C.x
  for tool in $toy_tools_relax
  do
    echo "Running $b with toy tool $tool ..."
    numactl --cpubind=0 $PROFILE vclinic-$TNUM- $EXE -t $tool -- echo ./$b.C.x
  done

  for tool in $toy_tools_strict
  do
    echo "Running $b with toy tool $tool ..."
    numactl --cpubind=0 $PROFILE vclinic-$TNUM- $EXE -t $tool -- echo ./$b.C.x
  done

  for tool in $client_tools
  do
    echo "Running $b with client tool $tool ..."
    numactl --cpubind=0 $PROFILE vclinic-$TNUM- $EXE -t $tool -- echo ./$b.C.x
  done
done

echo "Collecting scalability data"
scale="8 4 2 1"
for i in $scale
do
  export OMP_NUM_THREADS=$i
  for b in $BENCH
  do
    echo "Running $b with $i threads ..."
    numactl --cpubind=0 $PROFILE ori-$i- echo ./$b.C.x
    for tool in $toy_tools_relax
    do
      echo "Running $b ($i) with toy tool $tool ..."
      numactl --cpubind=0 $PROFILE vclinic-$i- $EXE -t $tool -- echo ./$b.C.x
    done
  done
  unset OMP_NUM_THREADS
done
