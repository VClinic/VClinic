#!/bin/bash
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

