#!/bin/bash
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.
#
#WARNING: must have compiled PyTorch and caffe2

CURR_DIR=`pwd`

# check if the command contains "cpp_algo_socket"
if [[ $1 == *"cpp_algo_socket"* ]]; then
    # using socket interface
    echo "The CPP caching layer is started by the script below ..."
    echo "Will use SOCKET as the interface"

    cd /mnt/extra/ev-store-dlrm/mixed_precs_caching
    g++ -O3 evlfu_4.cpp evlfu_8.cpp evlfu_16.cpp evlfu_32.cpp aprx_embedding.cpp cache_manager.cpp -pthread; ./a.out &
else
    # Each experiment might have different cacheSize, thus we recompile it
    echo "Compile the C++ shared library ... "
    echo "Will use Ctypes as the interface"
    cd /mnt/extra/ev-store-dlrm/mixed_precs_caching
    g++ -shared -o libcachemanager.so -fPIC -O3 evlfu_4.cpp evlfu_8.cpp evlfu_16.cpp evlfu_32.cpp aprx_embedding.cpp cache_manager.cpp -pthread; mv *.so lib/
    echo "C++ shared library (*.so) is updated!"

    # check if this DLRM deployment wants to use specific libcachemanager naming [To enbale multi DLRM deployment]
    if [ -z "$2" ]; then
        echo "No need to rename the .so"
    else
        echo "COPY lib/libcachemanager.so -> lib/$2" # will be use by Ctypes!
        cp lib/libcachemanager.so lib/$2
    fi
fi

cd $CURR_DIR
#check if extra argument is passed to the test
if [ -z "$1" ]; then
    dlrm_extra_option=""
else
    dlrm_extra_option=$1
fi
# echo $dlrm_extra_option

dlrm_pt_bin="python3 dlrm_s_pytorch_C1_C2.py"
# dlrm_c2_bin="python3 dlrm_s_caffe2.py"

echo "run pytorch C1_C2 ..."
# WARNING: the following parameters will be set based on the data set
# --arch-embedding-size=... (sparse feature sizes)
# --arch-mlp-bot=... (the input to the first layer of bottom mlp)
$dlrm_pt_bin --arch-sparse-feature-size=36 --arch-mlp-bot="13-512-256-64-36" --arch-mlp-top="512-256-1" --data-generation=dataset --data-set=kaggle --loss-function=bce --round-targets=True --learning-rate=0.1 --mini-batch-size=128 --print-freq=1024 --print-time --test-mini-batch-size=1 --test-num-workers=0 $dlrm_extra_option 2>&1 

# echo "run caffe2 ..."
# WARNING: the following parameters will be set based on the data set
# --arch-embedding-size=... (sparse feature sizes)
# --arch-mlp-bot=... (the input to the first layer of bottom mlp)
# $dlrm_c2_bin --arch-sparse-feature-size=16 --arch-mlp-bot="13-512-256-64-16" --arch-mlp-top="512-256-1" --data-generation=dataset --data-set=kaggle --raw-data-file=./input/train.txt --processed-data-file=./input/kaggleAdDisplayChallenge_processed.npz --loss-function=bce --round-targets=True --learning-rate=0.1 --mini-batch-size=128 --print-freq=1024 --print-time $dlrm_extra_option 2>&1 | tee run_kaggle_c2.log

echo "finished!"
