#!/bin/bash

set -ex

CUR_DIR=$(realpath $(dirname $0))
ROOT_DIR=$(realpath $CUR_DIR/..)
cd $ROOT_DIR

MAIN="$ROOT_DIR/bazel-bin/example/n-hop" # process name
WNUM=4
WCORES=4

INPUT=${INPUT:="$ROOT_DIR/data/graph/nodes.csv"}
# INPUT=${INPUT:="$ROOT_DIR/data/graph/v100_e2150_ua_c3.csv"}
SUBNODE=${SUBNODE:="$ROOT_DIR/data/graph/nodes.txt"}
OUTPUT=${OUTPUT:="/tmp/n-hop"}
IS_DIRECTED=${IS_DIRECTED:=false}
N=${N:=5}
K=${K:=4}
NUM=${NUM:=2000}

# param
PARAMS+=" --threads ${WCORES}"
PARAMS+=" --input ${INPUT} --output ${OUTPUT} --is_directed=${IS_DIRECTED}"
PARAMS+=" --n ${N} --k ${K} --num ${NUM} --subnode ${SUBNODE}"

# mpich
MPIRUN_CMD=${MPIRUN_CMD:="$ROOT_DIR/3rd/mpich/bin/mpiexec.hydra"}

# test
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$ROOT_DIR/3rd/hadoop2/lib

# output dir
mkdir -p $OUTPUT

# run
${MPIRUN_CMD} -n ${WNUM} ${MAIN} ${PARAMS}

echo ">>>>>>>>>>>output>>>>>>>>>>>>>>"
ls -lh $OUTPUT
