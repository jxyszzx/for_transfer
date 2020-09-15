#!/bin/bash

set -ex

CUR_DIR=$(realpath $(dirname $0))
ROOT_DIR=$(realpath $CUR_DIR/..)
cd $ROOT_DIR

MAIN="$ROOT_DIR/bazel-bin/example/n-hop" # process name
WNUM=16
WCORES=4

INPUT=${INPUT:="/apsarapangu/disk1/ldbc-data/social_network_person.300/person_knows_person.csv"}
# INPUT=${INPUT:="$ROOT_DIR/data/graph/v100_e2150_ua_c3.csv"}
SUBNODE=${SUBNODE:="$ROOT_DIR/data/graph/ldbc_nodes.txt"}
TORUN=${TORUN:="$ROOT_DIR/data/graph/to_run.txt"}
OUTPUT=${OUTPUT:="/tmp/n-hop"}
IS_DIRECTED=${IS_DIRECTED:=false}
N=${N:=5}
K=${K:=5}
NUM=${NUM:=10}
SUB_ROUND=${SUB_ROUND:=5}
#EPS=${EPS:=0.0001}
#DAMPING=${DAMPING:=0.85}
#ITERATIONS=${ITERATIONS:=100}

export HADOOP_HOME=${APP_HADOOP_HOME:='/apsarapangu/disk1/hadoop/hadoop-2.8.4'}
export HADOOP_CONF_DIR="${HADOOP_HOME}/etc/hadoop"
export CLASSPATH=${HADOOP_HOME}/etc/hadoop:`find ${HADOOP_HOME}/share/hadoop/ | awk '{path=path":"$0}END{print path}'`
export LD_LIBRARY_PATH="${HADOOP_HOME}/lib/native":${LD_LIBRARY_PATH}

# param
PARAMS+=" --threads ${WCORES}"
PARAMS+=" --input ${INPUT} --output ${OUTPUT} --is_directed=${IS_DIRECTED}"
PARAMS+=" --n ${N} --k ${K} --num ${NUM} --subnode ${SUBNODE} --sub_round ${SUB_ROUND} --to_run ${TORUN}"
#PARAMS+=" --iterations ${ITERATIONS} --eps ${EPS} --damping ${DAMPING}"

# mpich
MPIRUN_CMD=${MPIRUN_CMD:="$ROOT_DIR/3rd/mpich/bin/mpiexec.hydra"}

# test
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$ROOT_DIR/3rd/hadoop2/lib

# output dir
mkdir -p $OUTPUT

# run
#${MPIRUN_CMD} -n ${WNUM} ${MAIN} ${PARAMS}
${MPIRUN_CMD} -n ${WNUM} -hostfile $ROOT_DIR/../host_list_16 /usr/bin/time -o $ROOT_DIR/../log_time -v ${MAIN} ${PARAMS}

echo ">>>>>>>>>>>output>>>>>>>>>>>>>>"
ls -lh $OUTPUT
