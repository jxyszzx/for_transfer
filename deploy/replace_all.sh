for machine in `cat hosts_16`
do
    OP="cd $2 ; sudo rm -r $1"
    echo "SSH to "$machine
    ssh $machine $OP
    scp -r $1 $machine:$2
done

# [Usage]
# ./replace_all.sh /home/admin/longbin/plato/bazel-bin/example /home/admin/longbin/plato/bazel-bin/
# ./replace_all.sh /home/admin/longbin/plato/data/graph /home/admin/longbin/plato/data/