#!/bin/bash
LIST_FILES=compressed4git-list-files.txt

if [ "$#" -eq 0 ]; then
    echo "ERROR: Need 1 arguments: <input_dir>"
    echo "       Example: ./script/uncompress_folder_for_github.sh  stored_model/criteo_kaggle_5mil/epoch-00/compressed4git-ev-table output-folder-name"
    exit
fi

echo "==========================================="
echo "Uncompress the files inside folder = $1"
echo "    will NOT uncompress any inner folder!"
echo "==========================================="
dirname=`basename $1`
parentdir="$(dirname "$1")"
# create the new folder to hold the compressed files
new_folder=$parentdir/"un-$dirname"
rm -rf $new_folder; mkdir $new_folder

cd $1
cat $LIST_FILES | while read file 
do
  echo "UN-Compressing file = $file "
  # join the parts
  cat $file.tar.gz.part* > $file.tar.gz.joined
  
  # uncompress
  tar -zxvf $file.tar.gz.joined
  # move to the uncompressed folder
  mv $file ../"un-$dirname"/

  # remove the joined tar.gz files
  rm -rf $file.tar.gz.joined
done

if [ -z "$2" ]
then
      echo "$2 is empty; the output dir won't be renamed!"
else
      cd ../
      # pwd
      echo "... rename the output dir to = $2"
      rm -rf $2
      mv "un-$dirname" $2
      new_folder=$parentdir/$2
fi

echo "Done, folder is ready for Github push: $new_folder"
echo "      remove old folder: rm -rf $1"