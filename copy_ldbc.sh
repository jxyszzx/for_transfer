for machine in `cat host_list_15`
do
    echo "Copy to "$machine
    scp ./plato/data/graph/ldbc_nodes.txt $machine:/home/admin/longbin/plato/data/graph/
    scp ./plato/data/graph/to_run.txt $machine:/home/admin/longbin/plato/data/graph/
done
