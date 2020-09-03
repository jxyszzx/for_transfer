for machine in `cat host_list_34`
do
    echo "Copy to "$machine
    scp -r $1 $machine:$2
done

# [Usage]
# ./copy_all.sh /apsarapangu/disk1/ldbc-data/social_network_person.300 /apsarapangu/disk1/ldbc-data/
# ./copy_all.sh /home/admin/longbin/plato /home/admin/longbin/