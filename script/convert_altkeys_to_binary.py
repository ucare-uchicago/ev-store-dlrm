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

def write_as_binary(arr_values, output_file):
    # https://stackoverflow.com/questions/27238680/writing-integers-in-binary-to-file-in-python
    # bytearray
    with open(output_file, 'wb') as f:
        # per row
        for val in arr_values:
            # https://stackoverflow.com/questions/58113899/creating-only-one-byte-with-struct-pack
            # Code for Unisgned INT: https://docs.python.org/3/library/struct.html
            f.write(struct.pack('>I', val))
            # f.write(val.to_bytes(4, byteorder='big', signed=False)) # integer is 4Bytes
    f.close()
    print("===== output file : " + output_file)
    # exit()

def convert_altkeys_to_binary(input_file, output_file):
    df = pd.read_csv(input_file, dtype=object, delimiter='-', header=None)
    df.rename(columns={0: 'tableId', 1: 'rowId'}, inplace=True)

    #combine the tableId and rowId
    # XXXXXXXXXXYY the last 2 digits is for tableId
        # YY = tableId 
        # XXXXXXXXXX = the rowId

    df["altKey"] = df.apply(lambda row: int(row['tableId']) + 100 * int(row["rowId"]), axis = 1)
    # print(df.head)

    # All python integers are long under the hood (https://stackoverflow.com/questions/34247166/python-convert-int-to-unsigned-short-then-back-to-int)
    arr_alt_keys = df["altKey"].tolist()
    # print(arr_alt_keys[0])
    # print(type(arr_alt_keys[0]))
    write_as_binary(arr_alt_keys, output_file)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-input_folder", help="Folder path of the raw alternative-keys files",type=str)
    args = parser.parse_args()
    if (not args.input_folder):
        print("ERROR: You must provide 1 argument: -input_folder <the input folder> ")
        exit(-1)
    else:
        # Create output directory at the PARENT dir
        input_folder_path = args.input_folder
        arr_files = get_files_in_dir(input_folder_path, "ev-table")
        Path(os.path.join(input_folder_path, OUT_BINARY_DIR_NAME)).mkdir(parents=True, exist_ok=True)

        # process each file
        for input_filepath in arr_files:
            print("Processing ... " + input_filepath)
            fileName = os.path.basename(input_filepath)
            fileName = os.path.splitext(fileName)[0] # removing .csv extension
            output_filepath = os.path.join(input_folder_path, OUT_BINARY_DIR_NAME, fileName + ".bin")

            # Convert each line in this file
            convert_altkeys_to_binary(input_filepath, output_filepath)
            # print(output_filepath)
            # exit(-1)
        print("output folder : " + input_folder_path + "/binary/")
        print("Done")
