#!/usr/bin/env python3

import sys
import argparse
import os
import pandas as pd 
import numpy as np
from os import listdir
from pathlib import Path
from array import *
from tqdm import tqdm


OUT_DIR_NAME = "embedding/"
defaultOutFile = "output"

def get_files_in_dir(baseDir, keyword):
    listPath = []
    counter = 0
    for item in listdir(baseDir):
        if (os.path.isfile(os.path.join(baseDir, item))) and (keyword in item):
            counter += 1
            listPath.append(os.path.join(baseDir, item))
    if counter == 0:
        print("ERROR: Can't find files (*"+keyword+"*) in folder: " + baseDir)
        exit(-1)
    return listPath

# save to a file
def write_to_file(df, filePath):
    df.to_csv(filePath, index=False, header=True, sep=',')
    print("===== output file : " + filePath)

def write_as_binary(df, filePath):
    # bytearray
    columns = df.columns[:-1]
    # val = df["0"].iloc[0]
    # print(val)
    # print(bin(np.float16(-117.0).view('H'))[2:].zfill(16))
    # print(bin(np.float16(val).view('H'))[2:].zfill(16))

    with open(filePath, 'wb') as f:
        # per row
        for nrow in tqdm(range(0, df.shape[0])):
            # per column [0... 1023]
            for col in columns:
                val = df[col].iloc[nrow]
                f.write(val)
    f.close()
    print("===== output file : " + filePath)

def apply_preconditions_add_x(df, add_factor):
    arr_floats = []
    for nrow in tqdm(range(0, df.shape[0])):
        arr_floats += [(float(item) + add_factor) for item in np.array(df.iloc[nrow]).tolist()]
    return arr_floats

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-file", help="File path of the raw ev data",type=str)
    parser.add_argument("-read_as", help="the ev data can be read as fp32 or fp16",type=str)
    parser.add_argument("-apply", help="the preconditioning that will be applied",type=str)
    parser.add_argument("-overwrite", help="will overwrite the input file when writing the output",action='store_true')
    args = parser.parse_args()
    if (not args.file) or (not args.apply) or (not args.read_as):
        print("ERROR: You must provide these 2 arguments: -file <the input file> -read_as <read as fp32 or fp16> -apply <the preconditioning options> ")
        exit(-1)
    else:
        # Create output directory at the PARENT dir
        inFile = args.file
        inFile_list = inFile.split('-')
        parentDir = str(Path(inFile).parent)
        outFile = inFile if (args.overwrite) else str(parentDir + "/" + defaultOutFile + "_" + inFile_list[4])
        
        print("===== Read ev data as " + args.read_as)

        if (args.read_as == "fp32"):
            df = pd.read_csv(inFile, dtype=object, delimiter=',').astype(np.float32)
        elif (args.read_as == "fp16"):
            df = pd.read_csv(inFile, dtype=object, delimiter=',').astype(np.float16)
        else:
            print("ERROR: Can't understand the read_as format : " + args.read_as)
            exit(-1)

        if 'key' not in df.columns:
            # No Key column, just use the index
            df["key"] = df.index
        df["key"] = df.key.astype(int)
        
        print(df.head())

        if (args.apply == "add1"):
            print("===== Apply add1")
            # key column is in the last, so I will apply the preconditioning except the last 
            for idx in range(0, len(df.columns)-1):
                curCol = df.columns[idx]
                df[curCol] = (df[curCol] + 1)
        elif (args.apply == "add2"):
            print("===== Apply add2")
            # key column is in the last, so I will apply the preconditioning except the last 
            for idx in range(0, len(df.columns)-1):
                curCol = df.columns[idx]
                df[curCol] = (df[curCol] + 2)
        elif (args.apply == "abs"):
            print("===== Apply abs")
            # key column is in the last, so I will apply the preconditioning except the last 
            for idx in range(0, len(df.columns)-1):
                curCol = df.columns[idx]
                df[curCol] = df[curCol].abs()
        elif (args.apply == "add_abs_med"):
            print("===== Apply add_abs_med")
            # key column is in the last, so I will apply the preconditioning except the last 
            # print(df.iloc[0][:-1])
            # print(df.iloc[0][:-1].median())
            # print(df.iloc[0][:-1].abs().median())
            # print(df.iloc[0][:-1].mean())
            abs_med = df.iloc[0][:-1].abs().median()
            for nrow in tqdm(range(0, df.shape[0])):
                for idx in range(0, len(df.columns)-1):
                    curCol = df.columns[idx]
                    df[curCol].iloc[nrow] = df[curCol].iloc[nrow] + abs_med
        elif (args.apply == "none"):
            print("===== Apply nothing")
        elif (args.apply == "add_med_abs"):
            print("===== Apply add_med_abs")
            # key column is in the last, so I will apply the preconditioning except the last 
            # print(df.iloc[0][:-1])
            # print(df.iloc[0][:-1].median())
            # print(df.iloc[0][:-1].abs().median())
            # print(df.iloc[0][:-1].mean())
            abs_med = abs(df.iloc[0][:-1].median())
            for nrow in tqdm(range(0, df.shape[0])):
                for idx in range(0, len(df.columns)-1):
                    curCol = df.columns[idx]
                    df[curCol].iloc[nrow] = df[curCol].iloc[nrow] + abs_med

        elif (args.apply == "write_binary"):
            print("===== Apply write_binary")
            write_as_binary(df, outFile + "-" + args.read_as + ".bin")
            exit(0)
        else: 
            print("ERROR: Can't understand the args.apply : " + args.apply)
            exit(-1)

        print(df.head())
        if (not args.overwrite):
            outFile += ".csv"
        write_to_file(df, outFile)
        # outDir = os.path.join(parentDir, OUT_DIR_NAME)
        # print("The output is at folder: " + outDir)
        # if not os.path.exists(outDir):
        #     os.makedirs(outDir)






