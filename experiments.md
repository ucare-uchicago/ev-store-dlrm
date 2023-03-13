# Experiments:
1.  Exp1: "[Run DLRM on SERVER GPU=RTX 6000](#run-dlrm-on-server-gpurtx-6000-)"
2.  Exp2: "[Generate Workload For Java CacheBench](#generate-workload-for-java-cachebench)"
3.  Exp3: "[Run EVStore-C1 + Simple Approximate Embedding](#run-evstore-c1--simple-approximate-embedding)"
4.  Exp4: "[EV-Store Pipeline :: DLRM + C1 + C2](#ev-store-pipeline--dlrm--c1--c2)"
5.  Exp5: "[FINAL EV-Store Pipeline :: DLRM + C1 + C2 + C3](#final-ev-store-pipeline--dlrm--c1--c2--c3)"


------------------------------------------------------------------
## **Run DLRM on SERVER GPU=RTX 6000**
------------------------------------------------------------------

> We recommend you to use the Chameleon Cloud GPU P100 or GPU RTX6000 node<br>

### 1. Create Reservation on Chameleon Cloud

> Reserve a physical host through the lease menu in the Chameleon Cloud website. Make sure to lease it for maximum 7 days.<br>

### 2. Launching an Instance

> Use CC-Ubuntu20.04-CUDA for the operating system image and please read the chameleon instructions for more details on how to launch an instance. <br>
    
### 3. Allocate floating IPs

> Allocate a floating IP address to the lease so that we can connect it through the Internet via SSH.

### 4. Preparation [Login as "cc"]

- Use cc user
    ```bash
    ssh cc@<IP_address>     # use floating IP address
    ```
- Check if it uses SSD
    ```bash
    lsblk -o name,rota      # 0 means SSD
    ```
- Setup disk <br> 
    Check if there is already mounted disk
    ```bash
    df -H
        # output example:
        # sda      8:0    0 223.6G  0 disk
    ```
    Check SSD or Disk
    ```bash
    lsblk -d -o name,rota
    cat /sys/block/sda/queue/rotational
    ```
- Setup user 
    ```bash
    sudo adduser --disabled-password --gecos "" evstoreuser 
    sudo usermod -aG sudo evstoreuser
    sudo su 
    cp -r /home/cc/.ssh /home/evstoreuser
    chmod 700  /home/evstoreuser/.ssh
    chmod 644  /home/evstoreuser/.ssh/authorized_keys
    chown evstoreuser  /home/evstoreuser/.ssh
    chown evstoreuser  /home/evstoreuser/.ssh/authorized_keys
    echo "evstoreuser ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers.d/90-cloud-init-users
    exit
    exit
    ```

### 5. Setup zsh [Login on "user"]

```bash
ssh evstoreuser@<IP_address>
```
```bash
sudo su
apt-get update
apt-get install zsh -y
chsh -s /bin/zsh root

# Break the Copy here ====
```
```bash
exit
sudo chsh -s /bin/zsh evstoreuser
which zsh
echo $SHELL

sudo apt-get install wget git vim zsh -y

# Break the Copy here ====
```
```bash
printf 'Y' | sh -c "$(wget -O- https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)"

/bin/cp ~/.oh-my-zsh/templates/zshrc.zsh-template ~/.zshrc
sudo sed -i 's|home/evstoreuser:/bin/bash|home/evstoreuser:/bin/zsh|g' /etc/passwd
sudo sed -i 's|ZSH_THEME="robbyrussell"|ZSH_THEME="risto"|g' ~/.zshrc
zsh
exit
exit
```
### 6. Install nvidia-smi [OPTIONAL]

```bash
ssh evstoreuser@<IP_address> 
```
- Install ubuntu-drivers
    ```bash
    sudo apt-get update
    sudo apt-get install ubuntu-drivers-common -y

    ubuntu-drivers devices
        # output example
        # model    : GP100GL [Tesla P100 PCIe 16GB]
        # driver   : nvidia-driver-460 - distro non-free recommended

    sudo ubuntu-drivers autoinstall           
    sudo reboot # 5 mins
    ```

### 7. Install CUDA toolkit [OPTIONAL]

```bash
sudo apt list -a nvidia-cuda-toolkit
sudo apt install nvidia-cuda-toolkit -y 

    # If you want to remove the CUDA toolkit
        # sudo apt-get remove --purge nvidia* -y
        # sudo apt-get remove --purge *cuda* -y

# check version 
nvcc --version

# Check GPU counts
nvidia-smi
nvidia-smi --query-gpu=name --format=csv,noheader | wc -l
```

### 8. Install Dependencies Conda

- Install anaconda3 [Run Once]
    ```bash
    sudo mkdir -p /mnt/extra
    sudo chown evstoreuser -R /mnt
    ```
    ```bash
    cd /mnt/extra
    wget https://repo.anaconda.com/archive/Anaconda3-5.3.0-Linux-x86_64.sh . --no-check-certificate
    bash Anaconda3-5.3.0-Linux-x86_64.sh -b -p $HOME/anaconda3

    # zsh activate conda 
    echo "export PATH=\"~/anaconda3/bin:\$PATH\"" >> ~/.zshrc
    echo "export GITHUB=/mnt/extra/" >> ~/.zshrc

    cd ~/anaconda3/bin && ./conda update conda -y
    ./conda init zsh 
    exit
    ```
- Create conda env [Run Once]
    ```bash
    ssh evstoreuser@<IP_address>
    ```
    ```bash
    condaEnvName="ev-store-pyrocks-env"
    conda create --name $condaEnvName python=3.6 -y
    conda activate $condaEnvName
    echo "conda activate $condaEnvName" >> ~/.zshrc

    which -a pip | grep $condaEnvName
    
    export condaPip=`which -a pip | grep $condaEnvName`
    
    $condaPip --version

    $condaPip install future
    $condaPip install numpy
    $condaPip install pandas
    $condaPip install scikit-learn
    $condaPip install onnx
    $condaPip install torchviz
    $condaPip install mpi
    $condaPip install torch
    $condaPip install tqdm
    $condaPip install pydot

    cd /mnt/extra
    git clone https://github.com/mlperf/logging.git mlperf-logging
    $condaPip install -e mlperf-logging
    $condaPip install tensorboard

    $condaPip install matplotlib
    $condaPip install jupyter
    $condaPip install jupyterlab
    $condaPip install jupyterthemes
    $condaPip install ipykernel

    # install ipykernel [on "ev-store-pyrocks-env"]
    conda install ipykernel -y

    # Add conda env to jupyter
    python -m ipykernel install --user --name=$condaEnvName
        
    # install jupyterthemes
    pip install jupyterthemes

    # Set Theme
    jt -l
    jt -t chesterish

    # Launch Jupyter Lab [Good for light usage or result monitoring, OPTIONAL]
    cd /mnt/extra
    conda deactivate
    conda deactivate
    conda env list
    conda activate ev-store-pyrocks-env
    jupyter nbextension enable --py --sys-prefix widgetsnbextension
    
    nohup jupyter lab &
    sleep 5
    cat nohup.out | grep token
    ```
    Run at **LOCAL**
    ```bash
    # Port Forwarding
    # Start editing the notebook 
    ssh -L 9999:localhost:8888  evstoreuser@<IP_address>
    # Open http://localhost:9999/
    ```
- We recommend VSCode Remote for editing the code
### 9. Install Dependencies RocksDB

> https://pyrocksdb.readthedocs.io/en/v0.4/installation.html
> https://github.com/facebook/rocksdb/blob/master/INSTALL.md

```bash
sudo chown evstoreuser -R /mnt

cd /mnt/extra
sudo apt-get install build-essential -y
sudo apt-get install libsnappy-dev zlib1g-dev libbz2-dev libgflags-dev -y
git clone https://github.com/facebook/rocksdb.git
cd /mnt/extra/rocksdb
make shared_lib  # take 30 mins 
    # make clean

# installs the shared library in /mnt/extra/lib/ and the header files in /mnt/extra/include/rocksdb/:
sudo make install-shared INSTALL_PATH=/usr
```

### 10. Install pyrocksdb

At Rocksdb client
```bash
cd ~
sudo apt-get update
sudo apt-get install cmake python3-virtualenv python-dev -y

git clone https://github.com/twmht/python-rocksdb.git --recursive -b pybind11
cd python-rocksdb
python setup.py install

python -c 'import pyrocksdb; print("It works!");'
```

### 11. Install sqlite

```bash
cd /mnt/extra/
mkdir sqlite
wget https://www.sqlite.org/2022/sqlite-autoconf-3370200.tar.gz
tar xvzf sqlite-autoconf-3370200.tar.gz
cd sqlite-autoconf-3370200
./configure --prefix=/usr/local
make
sudo make install

# check the version 
sqlite3 --version | grep 2022
```

### 12. Clone EV-Store-DLRM Github
```bash
sudo chown evstoreuser -R /mnt
mkdir -p /mnt/extra/
cd /mnt/extra/
git clone https://github.com/ucare-uchicago/ev-store-dlrm.git
```
### 13. Get EVStore Dataset

> Download via wget HTTP public link: https://chi.uc.chameleoncloud.org:7480/swift/v1/AUTH_64de01b64f854410a6aead305682bd62/ev-store-dataset/ 

```bash
    # get list of all files in ev-store-dataset 
    cd /mnt/extra/ev-store-dlrm/
    rm -rf index.html*
    wget https://chi.uc.chameleoncloud.org:7480/swift/v1/AUTH_64de01b64f854410a6aead305682bd62/ev-store-dataset/ 

    declare -a arr=("input/compressed4git-criteo_kaggle_all_mmap" "input/compressed4git-criteo_kaggle_all" "stored_model/criteo_kaggle_all")
    for directory in "${arr[@]}"
    do
        echo "Working on $directory"
        # get list of files to download 
        cat index.html | grep $directory > file_to_download.txt

        # iterate the content of the file to download 
        while read filepath; do
            # download file via wget 
            ./script/wget_evstore_dataset.sh  $filepath
        done < file_to_download.txt
    done
```
### 14. Set-Up/Uncompress Sample/Workload Data

- Uncompress ev-table and input-data 
    * Uncompress Preprocessed Input Data 
        ```bash
        cd /mnt/extra/ev-store-dlrm/
        ./script/uncompress_folder_for_github.sh input/compressed4git-criteo_kaggle_all
        ./script/uncompress_folder_for_github.sh input/compressed4git-criteo_kaggle_all_mmap
        mv input/un-compressed4git-criteo_kaggle_all        input/criteo_kaggle_all
        mv input/un-compressed4git-criteo_kaggle_all_mmap   input/criteo_kaggle_all_mmap
        
        # Remove old compressed folders 
        rm -rf input/compressed4git*
        ```
    * Uncompress Stored Model 
        ```bash
        
        cd /mnt/extra/ev-store-dlrm/

        declare -a arrInputData=("criteo_kaggle_all")
        for input_data in "${arrInputData[@]}"
        do
            echo "$input_data"
            ./script/uncompress_folder_for_github.sh stored_model/$input_data/compressed4git-epoch-00
            ./script/uncompress_folder_for_github.sh stored_model/$input_data/compressed4git-ev-table
            ./script/uncompress_folder_for_github.sh stored_model/$input_data/compressed4git-binary
        done
        ```
        - Rename folder
            ```bash 
            for input_data in "${arrInputData[@]}"
            do
                echo "$input_data"
                cd stored_model/$input_data/
                mv un-compressed4git-epoch-00 epoch-00
                mv un-compressed4git-ev-table epoch-00/ev-table
                mv un-compressed4git-binary epoch-00/ev-table/binary
            done
            ```

        - Uncompress the extra 4bit, 8bit, and 16bit ev-tables 
            ```bash
            cd /mnt/extra/ev-store-dlrm/stored_model/criteo_kaggle_all/
            mv ev-table-*   epoch-00

            # uncompress 4bit ev-table
                cd /mnt/extra/ev-store-dlrm/
                ./script/uncompress_folder_for_github.sh stored_model/criteo_kaggle_all/epoch-00/ev-table-4/compressed4git-binary       binary
                ./script/uncompress_folder_for_github.sh stored_model/criteo_kaggle_all/epoch-00/ev-table-4/compressed4git-raw_u_4bit   raw_u_4bit
                ./script/uncompress_folder_for_github.sh stored_model/criteo_kaggle_all/epoch-00/ev-table-4/compressed4git-raw_u_4bit_to_float raw_u_4bit_to_float
                ./script/uncompress_folder_for_github.sh stored_model/criteo_kaggle_all/epoch-00/ev-table-4/compressed4git-raw_u_4bit_to_u_char raw_u_4bit_to_u_char
                
                rm -rf stored_model/criteo_kaggle_all/epoch-00/ev-table-4/compressed4git-*

            # uncompress 8bit ev-table
                cd /mnt/extra/ev-store-dlrm/
                ./script/uncompress_folder_for_github.sh stored_model/criteo_kaggle_all/epoch-00/ev-table-8/compressed4git-binary           binary
                ./script/uncompress_folder_for_github.sh stored_model/criteo_kaggle_all/epoch-00/ev-table-8/compressed4git-u_char           u_char
                ./script/uncompress_folder_for_github.sh stored_model/criteo_kaggle_all/epoch-00/ev-table-8/compressed4git-u_char_to_float  u_char_to_float
                
                rm -rf stored_model/criteo_kaggle_all/epoch-00/ev-table-8/compressed4git-*

            # uncompress 16bit ev-table
                cd /mnt/extra/ev-store-dlrm/
                ./script/uncompress_folder_for_github.sh stored_model/criteo_kaggle_all/epoch-00/ev-table-16/compressed4git-binary            binary
                ./script/uncompress_folder_for_github.sh stored_model/criteo_kaggle_all/epoch-00/ev-table-16/compressed4git-u_short           u_short
                ./script/uncompress_folder_for_github.sh stored_model/criteo_kaggle_all/epoch-00/ev-table-16/compressed4git-u_short_to_float  u_short_to_float
                
                rm -rf stored_model/criteo_kaggle_all/epoch-00/ev-table-16/compressed4git-*
            ```
    * Remove the compressed folder
        ```bash
        rm -rf /mnt/extra/ev-store-dlrm/input/compressed4git* 
        rm -rf /mnt/extra/ev-store-dlrm/stored_model/criteo_tb_day2/compressed4git* 
        rm -rf /mnt/extra/ev-store-dlrm/stored_model/criteo_kaggle_all/compressed4git* 

            # /dev/sda3       210G  166G   36G  83% /           # Output of "df -h"
        ```
- Create SYMBOLIC Link 
    > The "_mmap" will use the same stored_model as non _mmap
    ```bash
    cd /mnt/extra/ev-store-dlrm/stored_model/
    ln -s criteo_kaggle_all criteo_kaggle_all_mmap
### 15. DLRM Training + Generate EV-Table [OPTIONAL]
> This is for generating the ev-table dataset, we already have it in this public link (https://chi.uc.chameleoncloud.org:7480/swift/v1/AUTH_64de01b64f854410a6aead305682bd62/ev-store-dataset/ ) provided in this step [13 Get EvStore Dataset](#13-get-evstore-dataset) <br>

```bash
    cd /mnt/extra/ev-store-dlrm/
    input_data="criteo_kaggle_all"

    # use mmap (will generate the reordered.npz)
        ./bench/dlrm_s_criteo_kaggle.sh "--save-model=model.pth --ntest-per-epoch=1 --nepochs=3 --mlperf-logging --input-data=./input/$input_data --use-gpu --use-memory-map=True"

    # use default 
        nohup ./bench/dlrm_s_criteo_kaggle.sh "--save-model=model.pth --ntest-per-epoch=2 --nepochs=1 --mlperf-logging --input-data=./input/$input_data --use-gpu" > logs/train-$input_data.txt &  

    tail -f logs/train-$input_data.txt
```
- Reduce EV-Table precision 
    ```bash
    cd /mnt/extra/ev-store-dlrm/

    for i in $(seq 22 26)
    do 
        echo "\nReduce the precision of ev-table $i"
        # ./script/reduce_precision.py -file stored_model/criteo_kaggle_all/epoch-00/ev-table/ev-table-$i.csv -read_as fp32 -new_precision 32
        #./script/reduce_precision.py -file stored_model/criteo_kaggle_all/epoch-00/ev-table/ev-table-$i.csv -read_as fp32 -new_precision 16
        # ./script/reduce_precision.py -file stored_model/criteo_kaggle_all/epoch-00/ev-table/ev-table-$i.csv -read_as fp32 -new_precision 8
        ./script/reduce_precision.py -file stored_model/criteo_kaggle_all/epoch-00/ev-table/ev-table-$i.csv -read_as fp32 -new_precision 4posit
    done

    for i in $(seq 18 26)
    do 
        echo "\nConvert to binary of ev-table $i"
        #./script/convert_ev_to_binary.py -file stored_model/criteo_kaggle_all/epoch-00/ev-table-16/u_short/ev-table-$i.csv -read_as u_short
        #./script/convert_ev_to_binary.py -file stored_model/criteo_kaggle_all/epoch-00/ev-table-8/u_char/ev-table-$i.csv -read_as u_char
        ./script/convert_ev_to_binary.py -file stored_model/criteo_kaggle_all/epoch-00/ev-table-4/raw_u_4bit_to_u_char/ev-table-$i.csv -read_as u_char
    done
    ```
### 16. Prepare limit RAM/Memory 
- Install Dependencies
    ```bash
    sudo apt-get update
    sudo apt-get install -y cgroup-lite cgroup-tools cgroupfs-mount libcgroup1
    sudo apt-get install -y cgroup-bin

    sudo apt-get install stress
    ```
- Check current BOOT_IMAGE
    ```bash
    cat /proc/cmdline

    # To find current Ram Size
        free -h
    ```
- Edit BOOT_IMAGE at grub [**run once**] [Use **10G** for inference ONLY]
    ```bash
    sudo cp /etc/default/grub /etc/default/grub-backup
    cat /etc/default/grub | grep GRUB_CMDLINE_LINUX=

    sudo sed -i 's|GRUB_CMDLINE_LINUX=\"\"|GRUB_CMDLINE_LINUX=\"mem=15g cgroup_enable=memory swapaccount=1\"|g' /etc/default/grub
        # You can adjust the mem size, make sure it's enough
    cat /etc/default/grub | grep GRUB_CMDLINE_LINUX=

    # make the grub config file definitive 
    sudo update-grub

    ```
- Restart 
    ```bash
    sudo reboot &
    ```
- Check if grub is successful
    ```bash
    cat /proc/cmdline
    # Pay attention to the mem size
    ```

<br />

------------------------------------------------------------------
## **Run EVStore-C1 on SERVER GPU=RTX 6000**
------------------------------------------------------------------

### 0. Preparation and Install GNUPlot

> Follow: "[Run DLRM on SERVER GPU=RTX 6000](#run-dlrm-on-server-gpurtx-6000-)"

```bash
# Install gnuplot
sudo apt update
sudo apt install -y gnuplot
```
### 1. Insert the data to DB [DON'T LIMIT THE RAM]

```bash
cd /mnt/extra/ev-store-dlrm/
input_data="criteo_kaggle_all"
```

```bash
    # Rocksdb
    ./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --percent-data-for-inference=0.0001  --cache-size=1500 --ev-precs=32 --use-gpu=True --use-evstore=True --use-emb-cache=True --overwrite-db=True --emb-stor=rocksdb" 

    # sqlite
    ./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --percent-data-for-inference=0.0001  --cache-size=1500 --ev-precs=32 --use-gpu=True --use-evstore=True --use-emb-cache=True --overwrite-db=True --emb-stor=sqlite" 

    # If not enough memory allocated, change the RAM/memory limit in grub
```

### 2. Update RAM Capacity
> You can limit the RAM as low as you want, but if the RAM is too low then the DLRM cannot run at all. 
    
```bash

sudo vim /etc/default/grub
# Update the mem=XXg to mem=15g
# GRUB_CMDLINE_LINUX="mem=XXg cgroup_enable=memory swapaccount=1"
sudo update-grub
sudo reboot &

# cat /etc/default/grub | grep mem
# check if it is 15g
```

### 3. Run the EVStore-C1 (GPU + Memory Map)
>This is for running the EVStore-C1 and don't forget to limit the RAM to demonstrate that we can use small amount of memory

```bash
cd /mnt/extra/ev-store-dlrm/
input_data="criteo_kaggle_all"
```

```bash
# Take 15 mins 
./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --percent-data-for-inference=0.001  --cache-size=64000 --ev-precs=32 --use-gpu=True --use-evstore=True --use-emb-cache=False --emb-stor=filepy --use-memory-map=True --extra-mem-load=0 --overwrite-db=False" 
```

```bash
# Take 2 mins 
./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --percent-data-for-inference=0.0001  --cache-size=64000 --ev-precs=32 --use-gpu=True --use-evstore=True --use-emb-cache=False --emb-stor=filepy --use-memory-map=True --extra-mem-load=0 --overwrite-db=False" 
```
#### **Clean page-cache **
> Run this while using direct emb storage 
> This will make sure that the embedding is not being cached by the operating system

```bash
cd /mnt/extra/ev-store-dlrm/script
sudo ./free_page_cache.sh 0.25
```
#### **Test Benchmark**
> make sure that you see the same number (approximately takes 73 secs)

```bash
cd /mnt/extra/ev-store-dlrm/
input_data="criteo_kaggle_all"
./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --percent-data-for-inference=0.001  --cache-size=64000 --ev-precs=32 --use-gpu=True --use-evstore=True --use-emb-cache=True --emb-stor=rocksdb --use-memory-map=True --extra-mem-load=9000 --overwrite-db=False" 
```
```
    # recall 0.3979, precision 0.6270, f1 0.4868, ap 0.5798, auc 0.8056, best auc 0.8056, accuracy 80.068 %, best accuracy 0.000 %
        EVLFU Cache size = 64000
        Perfect hit C1 = 30783
```

### 3. Experiment Arguments 
    
```bash    
./bench/dlrm_s_criteo_kaggle_C1.sh
    --load-model=model.pth
    --input-data=./input/$input_data
    --ev-path=stored_model/$input_data/epoch-00/ev-table 
        # If we don't use the ev-path, then the DLRM will use the original tensor
    --mlperf-logging 
    --use-gpu=True 
    --get-cdf-lat=True
        # This will collect the latency per request for CDF data  
        --cdf-output-dir="/logs/xxxx/" ==>> to write the CDF latency data 
    --get-cdf-lat=False
        # This will not collect the latency per request
    --approx-emb-threshold=25
        # By default it's -1, which means it is off. 
        # threshold=25 means that when the aggHit is 25, it will randomize the last emb value
    --inference-only=False 
        # This will do training and testing (inference) 
        # It only works on dlrm_s_criteo_kaggle.sh
        # Implication: --mlperf-logging will be False
    --inference-only=True               
        --percent-data-for-inference=0.0001  ==>> How much data will be used for inference test
        --ev-lookup-only=False 
            # This will run the MLP inference 
        --ev-lookup-only=True 
            # This will not run the MLP inference; thus only does the ev lookup!
        --use-evstore=False 
            # This will run the original dlrm tensor and mlp model 
        --use-evstore=True 
            # Implication: --ev-path must exist
            --emb-stor=filepy ==>> ["filepy", "mmapfilepy", "dummy", "rocksdb", "sqlite"]
            --use-emb-cache=False 
                # The embedding will be read from the "emb-stor"
            --ev-precs=32  
            --use-emb-cache=True 
                --cache-algo=evlfu ==>> ["evlfu", "lru", "lfu"]
                --cache-warmup=True
                --cache-warmup=False
                    # This will not do warm-up     
                --cache-size=1500 
                --use-memory-map=True 
                    # The input_data will be using *_mmap
                --extra-mem-load=0 
                --overwrite-db=False
```

### 4. Benchmarking DLRM tensor [NO MEMORY LIMIT]
> This is for comparing with using the DLRM tensor as the original embedding layer and using the embedding from the embedding path
```bash
cd /mnt/extra/ev-store-dlrm/
input_data="criteo_kaggle_all"
```
#### **A. Use DLRM tensor (original embedding layer)**
```bash
    declare -a arr=("False" "True")
    for item in "${arr[@]}"
    do
        output_dir="logs/inference=0.0022/use-evstore=False/ev-lookup-only=$item/"
        echo "Writing output at $output_dir"
        mkdir -p $output_dir
        ./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --inference-only=True --mlperf-logging --use-gpu=True  --use-memory-map=True --percent-data-for-inference=0.0022 --use-evstore=False --ev-lookup-only=$item" > $output_dir/tensor.txt
    done
```

#### **B. Replace the embedding layer (embedding path is specified by the ev-path argument)**
```bash
    declare -a arr=("False" "True")
    for item in "${arr[@]}"
    do
        output_dir="logs/inference=0.0022/use-evstore=False/ev-lookup-only=$item/replaced-embedding/"
        echo "Writing output at $output_dir"
        mkdir -p $output_dir
        ./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --use-gpu=True  --use-memory-map=True --percent-data-for-inference=0.0022 --use-evstore=False --ev-lookup-only=$item" > $output_dir/tensor.txt
    done
```

### 5. Benchmarking Emb. Storage
> This is for comparing between various embedding storages and with different algorithms (EvLFU and LRU)
```bash
cd /mnt/extra/ev-store-dlrm/
input_data="criteo_kaggle_all"
```
0. Let the emb. storage use the OS cache

    - Full Inference Measurement <br>
        Emb. Storage 
        ```bash
        output_dir="logs/inference=0.0022/use-evstore=True/extra-mem-load=8500/use-emb-cache=False/ev-lookup-only=False/allow-page-cache/"
        mkdir -p $output_dir
        declare -a arr=("mmapfilepy" "filepy" "rocksdb" "sqlite")
        for item in "${arr[@]}"
        do
            echo "Working on $item"
            ./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --ev-precs=32 --use-gpu=True  --use-memory-map=True --percent-data-for-inference=0.0022 --extra-mem-load=8500 --use-evstore=True --overwrite-db=False --use-emb-cache=False --cache-warmup=True --cache-size=240000 --cache-algo=evlfu --emb-stor=$item --ev-lookup-only=False" > $output_dir/$item.txt
        done
        ```

    - EV-Lookup Measurement <br>
        Emb. Storage
        ```bash
        output_dir="logs/inference=0.0022/use-evstore=True/extra-mem-load=8500/use-emb-cache=False/ev-lookup-only=True/allow-page-cache/"
        mkdir -p $output_dir
        declare -a arr=("mmapfilepy" "filepy" "rocksdb" "sqlite")
        for item in "${arr[@]}"
        do
            echo "Working on $item"
            ./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --ev-precs=32 --use-gpu=True  --use-memory-map=True --percent-data-for-inference=0.0022 --extra-mem-load=8500 --use-evstore=True --overwrite-db=False --use-emb-cache=False --cache-warmup=True --cache-size=240000 --cache-algo=evlfu --emb-stor=$item --ev-lookup-only=True" > $output_dir/$item.txt
        done
        ```

1. Embedding storage is not using OS's page cache
    On other terminal
    ```bash
    cd /mnt/extra/ev-store-dlrm/script
    sudo ./free_page_cache.sh 0.25
    ```
    - Full Inference Measurement
        ```bash
        output_dir="logs/inference=0.0022/use-evstore=True/extra-mem-load=8500/use-emb-cache=False/ev-lookup-only=False/"
        mkdir -p $output_dir
        declare -a arr=("mmapfilepy" "filepy" "rocksdb" "sqlite")
        for item in "${arr[@]}"
        do
            echo "Working on $item"
            ./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --ev-precs=32 --use-gpu=True  --use-memory-map=True --percent-data-for-inference=0.0022 --extra-mem-load=8500 --use-evstore=True --overwrite-db=False --use-emb-cache=False --cache-warmup=True --cache-size=240000 --cache-algo=evlfu --emb-stor=$item --ev-lookup-only=False" > $output_dir/$item.txt
        done
        ```

    - EV-Lookup Measurement

        ```bash
        output_dir="logs/inference=0.0022/use-evstore=True/extra-mem-load=8500/use-emb-cache=False/ev-lookup-only=True/"
        mkdir -p $output_dir
        declare -a arr=("mmapfilepy" "filepy" "rocksdb" "sqlite")
        for item in "${arr[@]}"
        do
            echo "Working on $item"
            ./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --ev-precs=32 --use-gpu=True  --use-memory-map=True --percent-data-for-inference=0.0022 --extra-mem-load=8500 --use-evstore=True --overwrite-db=False --use-emb-cache=False --cache-warmup=True --cache-size=240000 --cache-algo=evlfu --emb-stor=$item --ev-lookup-only=True" > $output_dir/$item.txt
        done
        ```

2. 100% hit EV-LFU and LRU on various emb. storage
    > The cache size is 240000 which is big enough to store all the embedding data in the memory
    > To compare between EvLFU and LRU in all embedding storages

    On other terminal
    ```bash
    cd /mnt/extra/ev-store-dlrm/script
    sudo ./free_page_cache.sh 0.25
    ```
                
    - Full Inference Measurement <br>
        Tensor
        ```bash
        declare -a arrStorage=("mmapfilepy" "filepy" "rocksdb" "sqlite")
        for storage in "${arrStorage[@]}"
        do
            echo "Working on $storage"
            output_dir="logs/inference=0.0022/use-evstore=True/extra-mem-load=8500/use-emb-cache=True/ev-lookup-only=False/cache-size=240000/emb-stor=$storage/"
            mkdir -p $output_dir
            declare -a arr=("evlfu" "lru" )
            for item in "${arr[@]}"
            do
                echo "  Working on $item"
                ./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --ev-precs=32 --use-gpu=True  --use-memory-map=True --percent-data-for-inference=0.0022 --extra-mem-load=8500 --use-evstore=True --overwrite-db=False --use-emb-cache=True --cache-warmup=True --cache-size=240000 --cache-algo=$item --emb-stor=$storage --ev-lookup-only=False" > $output_dir/$item.txt
            done
        done
        ```

    - EV-Lookup Measurement
        ```bash
        declare -a arrStorage=("mmapfilepy" "filepy" "rocksdb" "sqlite")
        for storage in "${arrStorage[@]}"
        do
            echo "Working on $storage"
            output_dir="logs/inference=0.0022/use-evstore=True/extra-mem-load=8500/use-emb-cache=True/ev-lookup-only=True/cache-size=240000/emb-stor=$storage/"
            mkdir -p $output_dir
            declare -a arr=("evlfu" "lru" )
            for item in "${arr[@]}"
            do
                echo "  Working on $item"
                ./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --ev-precs=32 --use-gpu=True  --use-memory-map=True --percent-data-for-inference=0.0022 --extra-mem-load=8500 --use-evstore=True --overwrite-db=False --use-emb-cache=True --cache-warmup=True --cache-size=240000 --cache-algo=$item --emb-stor=$storage --ev-lookup-only=True" > $output_dir/$item.txt
            done
        done
        ```

3. Get CDF latency of EV-Lookup using EV-LFU and LRU on various emb. storage
    ```bash
    cd /mnt/extra/ev-store-dlrm/
    input_data="criteo_kaggle_all"
    ```
        
    Background Task <br>
    On other terminal
    ```bash
    cd /mnt/extra/ev-store-dlrm/script
    sudo ./free_page_cache.sh 0.25
    ```

    - Full Inference Measurement
        ```bash
        declare -a arrSize=("8000" "10000" "20000" "30000" "40000" "50000" "64000" "80000" "100000" "150000" "200000" "220000" "225000" "230000" "232000" "234000" "236000" "238000" "240000"  )
        for curr_size in "${arrSize[@]}"
        do
            echo "Working on size $curr_size"
            #"sqlite"
            declare -a arrStorage=("filepy" )
            for storage in "${arrStorage[@]}"
            do
                output_dir="logs/inference=0.0022/use-evstore=True/extra-mem-load=8500/use-emb-cache=True/ev-lookup-only=False/cache-size=$curr_size/emb-stor=$storage/"
                mkdir -p $output_dir
                declare -a arrCaches=("evlfu" "lru")
                for cache_algo in "${arrCaches[@]}"
                do
                    echo "Output at = $output_dir"
                    ./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --ev-precs=32 --use-gpu=True  --use-memory-map=True --percent-data-for-inference=0.0022 --extra-mem-load=8500 --use-evstore=True --overwrite-db=False --use-emb-cache=True --cache-warmup=True --cache-size=$curr_size --cache-algo=$cache_algo --emb-stor=$storage --ev-lookup-only=False --get-cdf-lat=True --cdf-output-dir=$output_dir" > $output_dir/$cache_algo.txt
                    ./script/plot_cdf.py --input-file=$output_dir/$cache_algo-cdf.csv 
                done
                # plotting cdf of evLFU vs LRU
                gnuplot -e "input_dir='/mnt/extra/ev-store-dlrm/$output_dir'" /mnt/extra/ev-store-dlrm/script/gnuplot_cdf_evlfu_lru.plt
            done
        done
        ```
            
    - EV-Lookup Measurement
        ```bash
        cd /mnt/extra/ev-store-dlrm/
        input_data="criteo_kaggle_all"

        declare -a arrSize=("20000" "50000" "100000" "200000" "230000")
        for curr_size in "${arrSize[@]}"
        do
            echo "Working on size $curr_size"
            declare -a arrStorage=("filepy" "sqlite")
            for storage in "${arrStorage[@]}"
            do
                output_dir="logs/inference=0.0022/use-evstore=True/extra-mem-load=8500/use-emb-cache=True/ev-lookup-only=True/cache-size=$curr_size/emb-stor=$storage/"
                mkdir -p $output_dir
                declare -a arrCaches=("evlfu" "lru")
                for cache_algo in "${arrCaches[@]}"
                do
                    echo "Output at = $output_dir"
                    ./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --ev-precs=32 --use-gpu=True  --use-memory-map=True --percent-data-for-inference=0.0022 --extra-mem-load=8500 --use-evstore=True --overwrite-db=False --use-emb-cache=True --cache-warmup=True --cache-size=$curr_size --cache-algo=$cache_algo --emb-stor=$storage --ev-lookup-only=True --get-cdf-lat=True --cdf-output-dir=$output_dir" > $output_dir/$cache_algo.txt
                    ./script/plot_cdf.py --input-file=$output_dir/$cache_algo-cdf.csv 
                done
                # plotting cdf of evLFU vs LRU
                gnuplot -e "input_dir='/mnt/extra/ev-store-dlrm/$output_dir'" /mnt/extra/ev-store-dlrm/script/gnuplot_cdf_evlfu_lru.plt
            done
        done
        ```
<br>

------------------------------------------------------------------
## **Generate Workload For Java CacheBench**
------------------------------------------------------------------

### 0. Preparation
> Follow: "[Run EVStore-C1 on SERVER GPU=RTX 6000](#run-evstore-c1-on-server-gpurtx-6000-)"
>
> Make sure you can run one of the benchmarking test 

### 1. Run Tracer 
    
```bash
cd /mnt/extra/ev-store-dlrm/
input_data="criteo_kaggle_all"

# You can increase the --percent-data-for-inference if you want to get more workload 
./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --use-memory-map=True --percent-data-for-inference=0.0022 --use-evstore=True --overwrite-db=False --trace-inference-workload=True" 
```

### 2. Move traces from ev-store-dlrm repo to java cache-benchmark
Clone the cache-benchmark repo first
https://github.com/ucare-uchicago/cache-benchmark.git <br>

Run in local
```bash
cd $GITHUB/cache-benchmark/
mkdir -p inf-workload-traces/
cd $GITHUB/cache-benchmark/inf-workload-traces/
rsync -Pav evstoreuser@<IP_address>:/mnt/extra/ev-store-dlrm/logs/inf-workload-traces/ .
```

<br>

------------------------------------------------------------------
## Run EVStore-C1 + Simple Approximate Embedding
------------------------------------------------------------------

### 0. Preparation
> Follow: [Run EVStore-C1 on SERVER GPU=RTX 6000](#run-evstore-c1-on-server-gpurtx-6000-)
>
> Make sure you can run one of the benchmarking test 
    
### 1. Benchmarking Emb. Storage [LIMIT RAM TO 10G]
    
```bash    
cd /mnt/extra/ev-store-dlrm/
input_data="criteo_kaggle_all"
```
- Background Task [On other terminal]
    ```bash
    cd /mnt/extra/ev-store-dlrm/script
    sudo ./free_page_cache.sh 0.25
    ```
```bash
storage="sqlite"
curr_size="100000"
approx_emb="22"     # threshold
cache_algo="evlfu"
output_dir="logs/inference=0.0022/use-evstore=True/extra-mem-load=8500/use-emb-cache=True/ev-lookup-only=False/cache-size=$curr_size/emb-stor=$storage/approx-emb-threshold=$approx_emb/"
mkdir -p $output_dir
echo "Output at = $output_dir"
./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --ev-precs=32 --use-gpu=True  --use-memory-map=True --percent-data-for-inference=0.0022 --extra-mem-load=8500 --use-evstore=True --overwrite-db=False --use-emb-cache=True --cache-warmup=True --cache-size=$curr_size --cache-algo=$cache_algo --emb-stor=$storage --ev-lookup-only=False --get-cdf-lat=True --cdf-output-dir=$output_dir --approx-emb-threshold=$approx_emb"
./script/plot_cdf.py --input-file=$output_dir/$cache_algo-cdf.csv 
```
- Full Inference Measurement
    ```bash
    cache_algo="evlfu"
    declare -a arrSize=("100000")
    for curr_size in "${arrSize[@]}"
    do
        echo "Working on size $curr_size"
        declare -a arrStorage=("filepy" "sqlite")
        for storage in "${arrStorage[@]}"
        do
            echo "Working on storage $storage"
            declare -a arrApproxEmb=( "15" "20" "21" "22" "23" "24" "25")
            for approx_emb in "${arrApproxEmb[@]}"
            do
                echo "Working on approx_emb $approx_emb"
                output_dir="logs/inference=0.0022/use-evstore=True/extra-mem-load=8500/use-emb-cache=True/ev-lookup-only=False/cache-size=$curr_size/emb-stor=$storage/approx-emb-threshold=$approx_emb/"
                mkdir -p $output_dir

                echo "Output at = $output_dir"
                ./bench/dlrm_s_criteo_kaggle_C1.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --ev-precs=32 --use-gpu=True  --use-memory-map=True --percent-data-for-inference=0.0022 --extra-mem-load=8500 --use-evstore=True --overwrite-db=False --use-emb-cache=True --cache-warmup=True --cache-size=$curr_size --cache-algo=$cache_algo --emb-stor=$storage --ev-lookup-only=False --get-cdf-lat=True --cdf-output-dir=$output_dir --approx-emb-threshold=$approx_emb" > $output_dir/$cache_algo.txt
                ./script/plot_cdf.py --input-file=$output_dir/$cache_algo-cdf.csv 
            done
        done
    done
    ```

<br>

------------------------------------------------------------------
## EV-Store Pipeline :: DLRM + C1 + C2
------------------------------------------------------------------

### 0. Preparation
> Follow: [Run EVStore-C1 on SERVER GPU=RTX 6000](#run-evstore-c1-on-server-gpurtx-6000-)
>
> Make sure you can run one of the benchmarking test 

### 1. Clear Cache Periodically
Background Task [On other terminal]
```bash
cd /mnt/extra/ev-store-dlrm/script
sudo ./free_page_cache.sh 0.25
```

### 2. Configure the caching layers
> you may edit the variables in the mixed_precs_caching/cache_manager.cpp to setup the experiments
```c
// The ratio between C1 and C2 is 50:50
#define N_CACHING_LAYER       2       // 1 or 2 layers
#define MAIN_PRECISION        8      // 32, 16, 8, or 4
#define SECONDARY_PRECISION   4       // must be lower than the MAIN_PRECISION
#define TOTAL_SIZE            8000   // 8000 is 3% in memory
```

### 3. Run DLRM C1_C2 with Ctypes interface [LIMIT RAM TO 15 GB]

- Run DLRM C1_C2 
    ```bash
    cd /mnt/extra/ev-store-dlrm/
    input_data="criteo_kaggle_all"
    cache_algo="cpp_algo"
    output_dir="logs/inference=0.003/use-evstore=True/use-emb-cache=True/ev-lookup-only=False/cache-size=00/emb-stor=cpp_caching_layer/$cache_algo/"
    ./bench/dlrm_s_criteo_kaggle_C1_C2.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --ev-precs=00 --use-gpu=True  --use-memory-map=True --percent-data-for-inference=0.003 --use-evstore=True --overwrite-db=False --use-emb-cache=True --cache-warmup=True --cache-size=00 --cache-algo=$cache_algo --emb-stor=cpp_caching_layer --ev-lookup-only=False --get-cdf-lat=True --cdf-output-dir=$output_dir" 
    ```
- Plotting CDF of evLFU vs LRU
    ```bash
    gnuplot -e "input_dir='/mnt/extra/ev-store-dlrm/$output_dir'" /mnt/extra/ev-store-dlrm/script/gnuplot_cdf_evlfu_lru.plt

    output_dir="logs/inference=0.003/use-evstore=True/extra-mem-load=8500/use-emb-cache=True/ev-lookup-only=False/cache-size=00/emb-stor=cpp_caching_layer/test/"
    gnuplot -e "input_dir='/mnt/extra/ev-store-dlrm/$output_dir'" /mnt/extra/ev-store-dlrm/script/gnuplot_cdf_direct_io.plt
    ```
------------------------------------------------------------------
## FINAL EV-Store Pipeline :: DLRM + C1 + C2 + C3
------------------------------------------------------------------

> This is an implementation of 8bit + 4bit caching, just a prototype

### 0. Preparation
> Follow: [EV-Store Pipeline :: DLRM + C1 + C2](#ev-store-pipeline--dlrm--c1--c2--)
>
> up to [3. Run DLRM C1_C2 with Ctypes interface [LIMIT RAM TO 15 GB]](#3-run-dlrm-c1_c2-with-ctypes-interface-limit-ram-to-15-gb)

### 1. Upload Approximate EV Data [Run in Local]
Run in Local
```bash
[where do we get the alternative keys in github?, just use from Dimas folder?
https://drive.google.com/drive/folders/13PnJx1pq-5nMk80HoUYySm8x6OiQ-I3a?usp=share_link]
rsync  -a --ignore-existing /Users/daniar/Documents/EV-Store/alternative-keys 192.5.87.198:/mnt/extra/ev-store-dlrm/stored_model/criteo_kaggle_all/
```

### 2. RUN DLRM C1_C2_C3

- Configure the caching manager at "cache_manager.cpp <br>
- You can modify it manually or use the script ./script/modify_param.py <br>
    ```c
    #define N_CACHING_LAYER       3       // 1 , 2 layers, or 3 layers (C1, C2, APRX_EV)
    #define MAIN_PRECISION        8      // 32, 16, 8, or 4
    #define SECONDARY_PRECISION   4       // must be lower than the MAIN_PRECISION
    #define TOTAL_SIZE            15085   // 8000 is 3% in memory
    #define SIZE_PROPORTION       "30 63 7"  // 30 50 20 | 35 55 10 |The size of C1 : C2 : C3. The total must be 100
    ```
- Execute DLRM inference 

    ```bash
    cd /mnt/extra/ev-store-dlrm/
    input_data="criteo_kaggle_all"
    cache_algo="cpp_algo"
    # size_proportion="50-50-0"
    # params="N_CACHING_LAYER=2 MAIN_PRECISION=8 SECONDARY_PRECISION=4 TOTAL_SIZE=75425 SIZE_PROPORTION=$size_proportion"
    output_dir="logs/inference=0.003/use-evstore=True/use-emb-cache=True/ev-lookup-only=False/cache-size=75425/emb-stor=cpp_caching_layer/$cache_algo/GPU-3/$size_proportion/1000n-euclid003-newrank"
    mkdir -p $output_dir
    # ./script/modify_param.py -file /mnt/extra/ev-store-dlrm/mixed_precs_caching/cache_manager.cpp -params $params
    ./bench/dlrm_s_criteo_kaggle_C1_C2_C3.sh "--load-model=model.pth --input-data=./input/$input_data --ev-path=stored_model/$input_data/epoch-00/ev-table --inference-only=True --mlperf-logging --ev-precs=00 --use-gpu=True  --use-memory-map=True --percent-data-for-inference=0.003 --use-evstore=True --overwrite-db=False --use-emb-cache=True --cache-warmup=True --cache-size=00 --cache-algo=$cache_algo --emb-stor=cpp_caching_layer --ev-lookup-only=False --get-cdf-lat=True --cdf-output-dir=$output_dir" > $output_dir/evlfu.txt
    ```

### 3. Download all the logs 
```bash
log_dir="log_alltog_eval_v100_1"
mkdir <custom_path_in_local>
rsync -Pav evstoreuser@<IP_address>:/mnt/extra/ev-store-dlrm/logs/ <custom_path_in_local>
```
### 4. Graph the CDF 
```bash
# go to script/gnuplot_graph/
# modify the file path

gnuplot script/cdf_2_line.plt
```
<br>
