#!/usr/bin/env python
import sys
import numpy as np
import pandas as pd
from pathlib import Path
import os
import argparse
import fileinput


def find_and_replace_variable(file_path, variable_name, desired_value):
    replaced = False
    if ("-" in desired_value and variable_name == "SIZE_PROPORTION"):
        # desired_value is a STRING
        for line in fileinput.input(file_path, inplace=1):
            if (not replaced) and (variable_name in line):
                # x="#define SIZE_PROPORTION       "30-50-20"  // 30-50-
                # we ASSUME that the value of this string MUST contains a '-'
                the_target_str = [s for s in line.split(" ") if ("-" in s)][0]
                # replacing the first occurence with the desired_value
                line = line.replace(str(the_target_str), str("\"" + desired_value + "\"") , 1)
                replaced = True
            sys.stdout.write(line)
    elif (desired_value.isdigit()):
        # desired_value is an INT
        for line in fileinput.input(file_path, inplace=1):
            if (not replaced) and (variable_name in line):
                # x="#define MAIN_PRECISION        8           // 32, 16, 8, or 4"
                the_first_number = [int(s) for s in line.split(" ") if s.isdigit()][0]
                # replacing the first occurence with the desired_value
                line = line.replace(str(the_first_number), desired_value, 1)
                replaced = True
            sys.stdout.write(line)
    elif (variable_name == "ctl.load_library"):
        print("   Changing the cpp caching library to be loaded by Ctypes")
        for line in fileinput.input(file_path, inplace=1):
            if (not replaced) and (variable_name in line):
                # line="cache_manager_cpp = ctl.load_library('libcachemanager.so', libdir)
                # the_target_str="ctl.load_library('libcachemanager.so',"
                the_target_str = [s for s in line.split(" ") if ("ctl.load_library" in s)][0]
                the_target_word = [s for s in the_target_str.split("'") if (".so" in s)][0]
                # replacing the first occurence with the desired_value
                line = line.replace(str(the_target_word), str( desired_value ) , 1)
                replaced = True
            sys.stdout.write(line)
    else :
        # params modification is UNKNOWN!
        print("ERROR: params modification is UNKNOWN! " + variable_name + " " + desired_value)
        exit(-1)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-file", help="File path of the code to edit",type=str)
    parser.add_argument("-params", help="Parameter to modify and the desired value",type=str)
    args = parser.parse_args()
    if (not args.file):
        print("ERROR: You must provide at least 1 arg: -file <the input file> ")
        exit(-1)
    else:
        print(args.file)
        print(args.params)

        arr_params = args.params.split(" ")
        print(arr_params)

        # iterate through each variables
        for param in arr_params:
            variable_name = param.split('=')[0]
            desired_value = param.split('=')[1]
            print("Will change the value of " + variable_name + " to " + desired_value)

            find_and_replace_variable(args.file, variable_name, desired_value)

    print("Done")
# ./modify_param.py -file /mnt/extra/ev-store-dlrm/mixed_precs_caching/cache_manager.cpp -params "N_CACHING_LAYER=2 MAIN_PRECISION=32 SECONDARY_PRECISION=4 TOTAL_SIZE=15085"
