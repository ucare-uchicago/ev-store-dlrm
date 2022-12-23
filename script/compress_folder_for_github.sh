#!/bin/bash
LIST_FILES=compressed4git-list-files.txt
MAIN_DIR=`pwd`
SIZE=45M #MB
# remote: warning: File input/compressed4git-criteo_kaggle_5mil/criteo_kaggle_5mil.npz.tar.gz.partaa 
# is 95.00 MB; this is larger than GitHub's recommended maximum file size of 50.00 MB

if [ "$#" -ne 1 ]; then
    echo "ERROR: Need 1 arguments: <input_dir>"
    echo "       Example: ./script/compress_folder_for_github.sh  stored_model/criteo_kaggle_5mil/epoch-0/ev-table"
    exit
fi

echo "==========================================="
echo "Compressing the files inside folder = $1"
echo "    will NOT compress any inner folder!"
echo "==========================================="
dirname=`basename $1`
parentdir="$(dirname "$1")"
# create the new folder to hold the compressed files
new_folder=$parentdir/"compressed4git-$dirname"
index_file=$parentdir/"compressed4git-$dirname"/$LIST_FILES
rm -rf $new_folder; mkdir $new_folder
echo "-- this line will be removed by tail later" >  $index_file

# collect the files that will be compressed
for file in $1/*; do
    if [[ -d $file ]]; then
        echo "== $file is a directory, will not be compressed"
    else
        echo "Found a file to compress : $file"
        filename=`basename $file`
        if [[ $filename == *"tar.gz"* ]]; then
          # we can't handle the file that already compressed as tar.gz; 
          # this will complicate the script
          echo "ERROR: *tar.gz* file is FOUND!"
          echo "       please remove it with = "
          echo "           rm -rf $1/*tar.gz*"
          exit
        fi
        echo $filename >>  $parentdir/"compressed4git-$dirname"/$LIST_FILES
    fi
done

# remove the first line
tail -n +2 "$index_file"  > "$index_file.tmp" && mv "$index_file.tmp" "$index_file"

cat $index_file | while read file 
do
  cd $1 #because we want the compression to happen at the file, without any dirs
  echo "Compressing file = $file "
  # compress the file using tar
  tar -zcvf $file.tar.gz $file
  
  # split into 45 MB each
  split -b $SIZE $file.tar.gz "$file.tar.gz.part"

  # remove the plain tar.gz
  rm -rf $file.tar.gz

  cd $MAIN_DIR #because the moving is relative to where the script was executed
  mv $1/*tar.gz.part* $new_folder/
done

echo "Done, folder is ready for Github push: $new_folder"
