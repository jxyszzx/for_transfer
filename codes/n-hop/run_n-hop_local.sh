#!/bin/bash

set -ex

CUR_DIR=$(realpath $(dirname $0))
ROOT_DIR=$(realpath $CUR_DIR/..)
cd $ROOT_DIR

MAIN="$ROOT_DIR/bazel-bin/example/n-hop" # process name
WNUM=8
WCORES=8

INPUT=${INPUT:="/apsarapangu/disk1/ldbc-data/social_network_person.300/person_knows_person.csv"}
SUBNODE=${SUBNODE:="$ROOT_DIR/data/graph/ldbc_nodes.txt"}
OUTPUT=${OUTPUT:="/tmp/n-hop"}
IS_DIRECTED=${IS_DIRECTED:=false}
N=${N:=5}
K=${K:=5}
NUM=${NUM:=10}
SUB_ROUND=${SUB_ROUND:=10}

# export HADOOP_HOME=${APP_HADOOP_HOME:='/apsarapangu/disk1/hadoop/hadoop-2.8.4'}
# export HADOOP_CONF_DIR="${HADOOP_HOME}/etc/hadoop"
# export CLASSPATH=${HADOOP_HOME}/etc/hadoop:`find ${HADOOP_HOME}/share/hadoop/ | awk '{path=path":"$0}END{print path}'`
# export LD_LIBRARY_PATH="${HADOOP_HOME}/lib/native":${LD_LIBRARY_PATH}

# param
PARAMS+=" --threads ${WCORES}"
PARAMS+=" --input ${INPUT} --output ${OUTPUT} --is_directed=${IS_DIRECTED}"
PARAMS+=" --n ${N} --k ${K} --num ${NUM} --subnode ${SUBNODE} --sub_round ${SUB_ROUND}"

# mpich
MPIRUN_CMD=${MPIRUN_CMD:="$ROOT_DIR/3rd/mpich/bin/mpiexec.hydra"}

# test
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$ROOT_DIR/3rd/hadoop2/lib

# output dir
mkdir -p $OUTPUT

# run
#${MPIRUN_CMD} -n ${WNUM} ${MAIN} ${PARAMS}
/usr/bin/time -v ${MPIRUN_CMD} -n ${WNUM} -hostfile $ROOT_DIR/../host_list_all ${MAIN} ${PARAMS}

echo ">>>>>>>>>>>output>>>>>>>>>>>>>>"
ls -lh $OUTPUT
