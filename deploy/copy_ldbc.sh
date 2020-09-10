for machine in `cat host_list_7`
do
    echo "Copy to "$machine
    scp ./plato/data/graph/ldbc_node.txt $machine:/home/admin/longbin/plato/data/graph/
done

# [Usage]
# ./copy_all.sh /apsarapangu/disk1/ldbc-data/social_network_person.300 /apsarapangu/disk1/ldbc-data/
# ./copy_all.sh /home/admin/longbin/plato /home/admin/longbin/


scp ./plato/data/graph/ldbc_nodes.txt 100.81.128.147:/home/admin/longbin/plato/data/graph/