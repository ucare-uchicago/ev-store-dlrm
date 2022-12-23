#!/bin/bash
if [ "$#" -eq 0 ]; then
    echo "ERROR: Need 1 argument:<object_dir to download>"
    echo "       Example: ./script/wget_evstore_dataset.sh input/compressed4git-criteo_kaggle_5mil/compressed4git-list-files.txt"
    exit
fi

# obj_path="input/compressed4git-criteo_kaggle_5mil/compressed4git-list-files.txt"
base_url="https://chi.uc.chameleoncloud.org:7480/swift/v1/AUTH_64de01b64f854410a6aead305682bd62/ev-store-dataset"
obj_path=$1
outdir=$(dirname "$obj_path")
filename=$(basename "$obj_path")

# echo $root_dir
echo "Adding $filename to $outdir"
wget "$base_url/$obj_path"
mkdir -p $outdir
mv $filename $outdir

