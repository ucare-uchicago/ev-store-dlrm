#!/bin/zsh
cd /mnt/extra/
mkdir -p cham_obj_stor
# sudo umount -l cham_obj_stor
cc-cloudfuse unmount cham_obj_stor
cc-cloudfuse mount cham_obj_stor
cd /mnt/extra/cham_obj_stor/ev-store-dataset/$1
echo "==================================================="
echo "Copy the script below; run one by one!"
echo ""
for item_to_copy in *; do
    echo "echo \"Copying $item_to_copy \""
    echo "cd /mnt/extra/; cc-cloudfuse unmount cham_obj_stor; cc-cloudfuse mount cham_obj_stor"
    echo "cd " `pwd`
    echo "mkdir -p /mnt/extra/ev-store-dlrm/$1"
    echo "# BREAK THE COPY HERE ======= "
    echo "cp -R -u /mnt/extra/cham_obj_stor/ev-store-dataset/$1/$item_to_copy   /mnt/extra/ev-store-dlrm/$1"
    echo "cd /mnt/extra/\n"
done
# cd compressed4git-binary
# ls