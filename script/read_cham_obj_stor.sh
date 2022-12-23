#!/bin/zsh
# Auto retry!!
echo "This script won't work due to connection lost"
exit
n=0
until [ "$n" -ge 350 ]
do  
    # Re mount 
    cd /mnt/extra/
    cc-cloudfuse unmount cham_obj_stor
    sleep 3
    cc-cloudfuse mount cham_obj_stor
    cd /mnt/extra/cham_obj_stor/ev-store-dataset/
    ls
    cd $1
    ls
    for item_to_copy in */; do
        echo "Copying $item_to_copy"

        cd $item_to_copy
        ls

        break
        # Do internal retry
        m=0
        until [ "$m" -ge 350 ]
        do
            # Re mount 
            cd /mnt/extra/
            cc-cloudfuse unmount cham_obj_stor
            cc-cloudfuse mount cham_obj_stor
            cd /mnt/extra/cham_obj_stor/ev-store-dataset/
            # ls
            cd $1
            ls
            cd $item_to_copy
            ls
            mkdir -p /mnt/extra/ev-store-dlrm/$1/$item_to_copy/
            break

            sleep 2
            cp -R $item_to_copy /mnt/extra/ev-store-dlrm/$1/$item_to_copy/ && break  # substitute your command here
            # break
            n=$((n+1)) 
            sleep 3
        done
        break
    done
    # echo $1
    break
    # openstack container list
    sleep 2
    cd $1
    ls $1
    echo "Retrying ... $n"
    # cp -R -n $1 $2 && break  # substitute your command here
    # rsync -aq criteo_kaggle_all /mnt/extra/ev-store-dlrm/stored_model/ && break  # substitute your command here
    # cp -R -u -p criteo_kaggle_all /mnt/extra/ev-store-dlrm/stored_model/ && break  # substitute your command here
    n=$((n+1)) 
    sleep 3
done