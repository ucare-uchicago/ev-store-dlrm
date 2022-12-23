#!/usr/bin/env python3
import sys
import argparse
import os
import pandas as pd 
import numpy as np
from tqdm import tqdm
from pathlib import Path
from os import listdir
import struct

OUT_BINARY_DIR_NAME = "binary/"

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

def write_uchar_per_byte(df, filePath):
    cols = df.columns
    with open(filePath, 'wb') as f:
        # iterate per string (row)
        for index, row in df.iterrows():
            # iterate per char
            for col in cols:
                # convert to unsign char MUST use struct.pack!!
                # https://stackoverflow.com/questions/58113899/creating-only-one-byte-with-struct-pack
                f.write(struct.pack('>B', row[col]))
    print("===== output file : " + filePath)

def write_ushort_per_byte(df, filePath):
    cols = df.columns
    with open(filePath, 'wb') as f:
        # iterate per string (row)
        for index, row in df.iterrows():
            # iterate per char
            for col in cols:
                # convert to unsign char MUST use struct.pack!!
                # https://stackoverflow.com/questions/58113899/creating-only-one-byte-with-struct-pack
                f.write(struct.pack('H', row[col]))
                # print(row[col])
                # print(struct.unpack('H', struct.pack('H', row[col])))
                # exit(-1)
    print("===== output file : " + filePath)

def write_as_binary(df, filePath):
    # bytearray
    columns = df.columns[:-1]
    with open(filePath, 'wb') as f:
        # per row
        for nrow in tqdm(range(0, df.shape[0])):
            # per column [0... 1023]
            for col in columns:
                val = df[col].iloc[nrow]
                f.write(val)
    f.close()
    print("===== output file : " + filePath)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-file", help="File path of the raw ev data",type=str)
    parser.add_argument("-read_as", help="the ev data can be read as fp32 or fp16",type=str)
    args = parser.parse_args()
    if (not args.file) or (not args.read_as):
        print("ERROR: You must provide these 2 arguments: -file <the input file> -read_as <read as fp32 or fp16>  ")
        exit(-1)
    else:
        # Create output directory at the PARENT dir
        filePath = args.file
        parentDir = str(Path(filePath).parent)
        fileName = os.path.basename(args.file)
        fileName = os.path.splitext(fileName)[0] # removing .csv extension
        outFile = os.path.join(parentDir, OUT_BINARY_DIR_NAME, fileName + ".bin")

        print("===== Read ev data as " + args.read_as)
        if (args.read_as == "fp32"):
            # create output folder if it doesn't exist
            Path(os.path.join(parentDir, OUT_BINARY_DIR_NAME)).mkdir(parents=True, exist_ok=True)
        
            df = pd.read_csv(filePath, dtype=object, delimiter=',').astype(np.float32)
            if 'key' not in df.columns:
                # No Key column, just use the index
                df["key"] = df.index
            df["key"] = df.key.astype(int)
            write_as_binary(df, outFile)

        elif (args.read_as == "u_short"):
            # alternative method to improve the fp16 by np.float16
            # New method: Use u_short to hold the value
            # This works for 8bit and 4bit precision
            grandParentDir = str(Path(parentDir).parent)
            # create output folder if it doesn't exist
            Path(os.path.join(grandParentDir, OUT_BINARY_DIR_NAME)).mkdir(parents=True, exist_ok=True)

            df = pd.read_csv(filePath, dtype=object, delimiter=',').astype(np.int)
            # print(df.head())
            outFile = os.path.join(grandParentDir, OUT_BINARY_DIR_NAME, fileName + ".bin")
            write_ushort_per_byte(df, outFile)
            
        elif (args.read_as == "fp16"):
            print("ERROR: read as fp16 is no longer supported! It's too slow to unpack by C++")
            exit(-1)
            # create output folder if it doesn't exist
            Path(os.path.join(parentDir, OUT_BINARY_DIR_NAME)).mkdir(parents=True, exist_ok=True)
        
            # Slow to unpack in c++!!! 
            # df = pd.read_csv(filePath, dtype=object, delimiter=',').astype(np.float16)

            if 'key' not in df.columns:
                # No Key column, just use the index
                df["key"] = df.index
            df["key"] = df.key.astype(int)
            write_as_binary(df, outFile)
        elif (args.read_as == "u_char"):
            # This works for 8bit and 4bit precision
            grandParentDir = str(Path(parentDir).parent)
            # create output folder if it doesn't exist
            Path(os.path.join(grandParentDir, OUT_BINARY_DIR_NAME)).mkdir(parents=True, exist_ok=True)
        
            df = pd.read_csv(filePath, dtype=object, delimiter=',').astype(np.int)
            outFile = os.path.join(grandParentDir, OUT_BINARY_DIR_NAME, fileName + ".bin")

            write_uchar_per_byte(df, outFile)
           
        else:
            print("ERROR: Can't understand the read_as format : " + args.read_as)
            exit(-1)
        print("Done")
