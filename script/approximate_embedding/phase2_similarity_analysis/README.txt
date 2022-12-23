How to run cuML_GPU:
1. Make sure your pc has nvidia gpu in it
2. Make sure your cuda version is 11++
3. Install rapids AI using conda
    # Make sure your python version in conda environment is 3.8 or 3.9 (this is the supported version at the time of writing)
    conda config --add channels rapidsai nvidia conda-forge
    conda config --set channel_priority flexible
    conda install rapids=22.04 cudatoolkit=11.2 dask-sql
    # The above command can be modified depending on what version you want to use. More info at https://rapids.ai/start.html#get-rapids
