#!/usr/bin/env python

import sys
import argparse
import os
import pandas as pd 
import numpy as np
from os import listdir
from pathlib import Path
from array import *
from tqdm import tqdm

# save to a file
def write_to_file(df, filePath):
    base_dir = Path(filePath).parent
    if not os.path.exists(base_dir):
        os.makedirs(base_dir)
    
    df.to_csv(filePath, index=False, header=True, sep=',')
    print("===== output file : " + filePath)

def round_down_to_odd(f):
    f = int(np.ceil(f))
    return f + 1 if f % 2 == 0 else f

def convert_ev_float_to_ushort(value):
    # new range is 0 - 65034
        # -1    -> x ->  65035
        # -0.65 -> 0 ->  65001
        # 0     -> 0.65 ->   0
        # 0.65  -> 1.3  ->  65000
        # 1     -> x ->  65034
    # Total value being represented:
        # 0.66 - 1 = 34 * 2 = 68
        # 6500*2= 13000
        # The dense region -> -0.65 .. 0.65
        # the smallest value = -0.0001
    if (value < -0.65):
        leftover = int(-100*(0.65 + value))
        if (leftover % 2 == 0): # is even
            # make an odd number (negative)
            leftover += 1
        return 65000 + leftover 
    elif (value > 0.65):
        leftover = int(100*(value - 0.65))
        if (leftover % 2 == 1): # is odd
            # make an even number (positive)
            leftover -= 1
        return 65000 + leftover 
    else:
        return int((value + 0.65) / 1.3 * 65000)

def convert_ushort_to_evfloat(value):
    # new range is 0 - 65034
        # -1    -> x ->  65035
        # -0.65 -> 0 ->  65001
        # 0     -> 0.65 ->   0
        # 0.65  -> 1.3  ->  65000
        # 1     -> x ->  65034
    if (value > 65000):
        diff = (value - 65000) / 100
        if (value % 2 == 1):
            return round(-1 * (0.65 + diff), 6)
        else:
            return round((0.65 + diff), 6)
    else:
        return round(((value / 65000) * 1.3 ) - 0.65, 6)

def convert_to_4bit_int_old(val):
    is_positive = val >= 0
    # -0.01 -> str(-1.01) -> 1 (count the zeros)
    # -0.001 -> str(-1.001) -> 2 (count the zeros)
    # 0     -> 0.0   -> 7
    # 0.0000001 -> 8
    
    is_positive = val >= 0
    if (is_positive):
        val += 1
    else: 
        val -= 1
    
    str_val = str(val)
    # remove the chars in the left of the dot
    str_val = str_val[str_val.find('.') + 1 :] 
    counter = 0
    # count the zeros
    for char in str_val:
        if (char == '0'):
            counter += 1
        else:
            break
    if (is_positive): # range = [7 ... 13]
        counter = 14 - counter
        if (counter < 7): 
            counter = 7
    else: # range = [0 ... 7]
        # cap the value to 7
        if counter > 7:
            counter = 7
    return counter

def convert_to_4bit_int(real_value):
    # TODO: REDO the accuracy F1 test using this mapping
    # -0.07 -0.06, -0.05,-0.04,-0.03,-0.02,-0.01, 0 ,0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.07
    # value_mapping_positive = [0.07, 0.06, 0.05, 0.04, 0.03, 0.02, 0.01, 0]

    value_mapping_positive = [0.5, 0.1, 0.07, 0.04, 0.01, 0.001, -0.0001, 0]
    value_mapping_negative = [-0.5, -0.1, -0.07, -0.04, -0.01, -0.001, -0.0001]

    # value_mapping_positive = [0.07, 0.03, 0.01, 0.001, 0.0001, 0.00001, 1e-06, 0]
    # value_mapping_negative = [-0.07, -0.03, -0.01, -0.001, -0.0001, -0.00001, -1e-06]

    # value_mapping_positive = [0.7, 0.3, 0.1, 0.01, 0.001, 0.0001, 1e-05, 0]
    # value_mapping_negative = [-0.7, -0.3, -0.1, -0.01, -0.001, -0.0001, -1e-05]
    if (real_value >= 0):
        int_val = 0
        for bracket_val in value_mapping_positive:
            if (real_value >= bracket_val):
                return int_val
            else:
                int_val += 1
    else:
        # negative value
        int_val = 7
        if (real_value > value_mapping_negative[len(value_mapping_negative) - 1]): # value close to 0
            return int_val
        else:
            # start the mapping from the biggest id
            int_val = 14

        for bracket_val in value_mapping_negative:
            if (real_value <= bracket_val):
                return int_val
            else:
                int_val -= 1

    print("ERROR: Can't find the value mapping at convert_to_4bit_int() " + str(real_value))
    exit(-1)

def convert_to_4bit_int_posit(real_value):
                            #   0     1   2   3       4       5     6   7
    value_mapping_positive = [ 0.8, 0.6, 0.4, 0.25, 0.015, 0.00025, 0] #0
                            #  14   13    12    11    10       9         8
    value_mapping_negative = [ -1 ,-0.8, -0.6, -0.4, -0.25, -0.015, -0.00025]

    if (real_value == 0):
        return 7
    elif (real_value > 0):
        int_val = 0
        for bracket_val in value_mapping_positive:
            if (real_value >= bracket_val):
                return int_val
            else:
                int_val += 1
    else:
        # negative value
        int_val = 8
        if (real_value >= value_mapping_negative[len(value_mapping_negative) - 1]): # value close to 0 
            #  -0.00025 ... 0
            return int_val
        else:
            # start the mapping from the biggest id
            int_val = 15 # It's okay to have 15, but it will never happen! no value less than -1

        for bracket_val in value_mapping_negative:
            if (real_value < bracket_val):
                return int_val
            else:
                int_val -= 1

    print("ERROR: Can't find the value mapping at convert_to_4bit_int() " + str(real_value))
    exit(-1)

def convert_from_4bit_int_posit(int_val):
    value_mapping = [1, 0.8, 0.6, 0.4, 0.0625, 0.00390625, 0.0000153, 0, -0.0000153, -0.00390625, -0.0625, -0.4, -0.6, -0.8, -1]

    return value_mapping[int_val]

def convert_from_4bit_int(int_val):
    # value_mapping = [None, -0.01, -0.001, -0.0001, -0.00001, -0.000001, -0.0000001, 0, 0.0000001, 0.000001, 0.00001, 0.0001, 0.001, 0.01]
    # value_mapping = [0.7, 0.3, 0.1, 0.01, 0.001, 0.0001, 1e-05, 0, -0.7, -0.3, -0.1, -0.01, -0.001, -0.0001, -1e-05]
    # value_mapping = [0.07, 0.03, 0.01, 0.001, 0.0001, 0.00001, 1e-06, 0, -0.07, -0.03, -0.01, -0.001, -0.0001, -0.00001, -1e-06]
    value_mapping = [0.5, 0.1, 0.07, 0.04, 0.01, 0.001, -0.0001, 0, -0.5, -0.1, -0.07, -0.04, -0.01, -0.001, -0.0001]

    return value_mapping[int_val]


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-file", help="File path of the raw ev data",type=str)
    # parser.add_argument("-out_dir", help="File path of the output directory",type=str)
    parser.add_argument("-read_as", help="the ev data can be read as fp32 or fp16",type=str)
    parser.add_argument("-new_precision", help="the new precision, should be lower than read_as",type=str)
    args = parser.parse_args()
    if (not args.file) or (not args.new_precision) or (not args.read_as):
        print("ERROR: You must provide these 2 arguments: -file <the input file> -read_as <read as fp32 or fp16> -new_precision <precision lower than read_as>")
        exit(-1)
    else:
        # Create output directory at the PARENT dir
        inFile = args.file
        parentDir = str(Path(inFile).parent)
        fileName = os.path.basename(inFile) 
        outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/" + fileName
        
        print("===== Read ev data as " + args.read_as)

        if (args.read_as == "fp32"):
            df = pd.read_csv(inFile, dtype=object, delimiter=',').astype(np.float32)
        elif (args.read_as == "fp16"):
            print("ERROR: read as fp16 is no longer supported! The conversion should always rooted from the 32bit version")
            exit(-1)
            df = pd.read_csv(inFile, dtype=object, delimiter=',').astype(np.float16)
        else:
            print("ERROR: Can't understand the read_as format : " + args.read_as)
            exit(-1)

        print("prev = " + str(df['0'].iloc[0]) + " " + str(df['0'].iloc[1]) + " " + str(df['0'].iloc[2]))

        # reduce the precision
        if (args.new_precision == "32"):
            print("   Reduce the precision to fp32")
            df = pd.read_csv(inFile, dtype=object, delimiter=',').astype(np.float32)
            write_to_file(df, outFile)
        elif (args.new_precision == "16"):
            print("   Reduce the precision to fp16")
            # using np.float16 to pack the binary is not efficient; the unpacking is too slow (in C++)
            # df = pd.read_csv(inFile, dtype=object, delimiter=',').astype(np.float16)

            # New method: We will pack the data into unsign short (2Bytes)
            # struct.unpack('H', struct.pack('H', 64000))
            # 1. Let's generate the u_short representation
            cols = df.columns
            print("This is as ushort, then will be converted to binary")
            for col in cols:
                # range [0 .. 650XY] -> this will represent +/-[0 .. 0.65000]
                    # negative number has even number as the last digit
                    # positive number has odd number as the last digit
                # How about 0.66, 0.67, 0.69, 0.70, 0.71 .. 0.99?
                    # let's just represent the 0.7, 0.8, and 0.9 since these numbers are so rare
                    # we use the X at 650XY to be added on top of 0.65 + 0.X
                        # the Y is for positive/negative sign bit
                df[col] = df[col].apply(lambda x: convert_ev_float_to_ushort(x))
            
            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/u_short/" + fileName
            write_to_file(df, outFile)
            
            # 2. Generate the converted float value to be used by DLRM accuracy testing
            print("This is to be used to test dlrm accuracy using py cache reader")
            for col in cols:
                # convert the [0 - 65000] to [-1 .. 0 .. 1]
                df[col] = df[col].apply(lambda x: convert_ushort_to_evfloat(x))

            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/u_short_to_float/" + fileName
            write_to_file(df, outFile)

            # write_to_file(df, outFile)
        elif (args.new_precision == "8"):
            # Make sure that 0 is ALWAYS 0
            print("   Reduce the precision to 8bit")
            # This will produce 2 output file, 
            # A. 1 csv file which will be converted to binary, and used by the C++ pipeline
                # we scale down the range to 0-254 and save it as char, then write to file!!!
            cols = df.columns
            print("This is to be converted to binary and floats")
            for col in cols:
                # new range is 0 - 254
                    # -1 -> 0 ->  0
                    # 0 -> 1 -> 127
                    # 1 -> 2 ->  254
                df[col] = df[col].apply(lambda x: round(((x + 1)/2) * 254))
            
            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/u_char/" + fileName
            write_to_file(df, outFile)

            # B. 1 csv file with the value as float, to be used as accuracy testing only!
                # we scale down the range to 0-254 then convert back to floats!
            print("This is to be used to test dlrm accuracy using py cache reader")
            for col in cols:
                # new range is -1 .. 1
                    # 0 -> -1
                    # 127 -> 0
                    # 254 -> 1
                df[col] = df[col].apply(lambda x: round(((x/254)*2)-1, 3))
            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/u_char_to_float/" + fileName
            write_to_file(df, outFile)
        elif (args.new_precision == "4posit"): # CURRENT DEFAULT 4bit
            # Make sure that 0 is ALWAYS 0
            # id  mapping     range
            # 0   1           0.9 - 0.25
            # 1   0.0625      0.24 - 0.015
            # 2   0.00390625  0.014 - 0.00025
            # 3   1.53E-05    0.00024 - 0 
            # 4   0           0

            print("   Reduce the precision to 4bit")
            # This will produce 3 output file, 
            # A. 1 csv file which will be converted to binary, and used by the C++ pipeline
                # we scale down the range to 0-14 and save it as char, then write to file!!!
            cols = df.columns
            print("This 4bit data is to be paired and form a char")
            for col in cols:
                df[col] = df[col].apply(lambda x: convert_to_4bit_int_posit(x))

            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/raw_u_4bit/" + fileName
            write_to_file(df, outFile)

            # B. Combine every 2 unsigned 4bit to be come 1 u_char
            df_uchar = pd.DataFrame()
            print("This will form a char out of 2 4bit data; then it can be used to make a binary")
            # Make sure that we have even number of columns, because we want to paired the columns
            if (len(cols)%2 != 0):
                print("ERROR: The number of column must be even!! Otherwise, the pairing will not work")
                exit(-1)

            new_col = 0
            for idx in range(0, len(cols)):
                if (idx % 2 == 0):
                    # combine two 4bit
                    # 9 + 7 -> (9*16)+7 = 151 
                    # 15 + 15 -> (15*16)+15 = 255
                    df_uchar[new_col] = (df[cols[idx]] * 16) + df[cols[idx + 1]]
                    new_col = new_col + 1
            # print(df_uchar.head())
            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/raw_u_4bit_to_u_char/" + fileName
            write_to_file(df_uchar, outFile)

            # C. 1 csv file with the value as float, to be used as accuracy testing only!
                # we scale down the range to 0-14 then convert back to floats!
            print("This is to be used to test dlrm accuracy using py cache reader")
            for col in cols:
                df[col] = df[col].apply(lambda x: convert_from_4bit_int_posit(x))
            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/raw_u_4bit_to_float/" + fileName
            write_to_file(df, outFile)
   
        elif (args.new_precision == "4"): # CURRENT DEFAULT 4bit
            # Make sure that 0 is ALWAYS 0
            # New range is only 0-14, so we want to represent the majority
                # we will hold the value of  -0.7 -0.3, -0.1,-0.01,-0.001,-0.0001,-0.00001, 0 ,0.00001, 0.0001, 0.001 0.01 0.1  0.3  0.7
                #                              0    1     2     3     4       5        6    7      8       9      10   11   12   13   14
            print("   Reduce the precision to 4bit")
            # This will produce 3 output file, 
            # A. 1 csv file which will be converted to binary, and used by the C++ pipeline
                # we scale down the range to 0-14 and save it as char, then write to file!!!
            cols = df.columns
            print("This 4bit data is to be paired and form a char")
            for col in cols:
                df[col] = df[col].apply(lambda x: convert_to_4bit_int(x))

            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/raw_u_4bit/" + fileName
            write_to_file(df, outFile)

            # B. Combine every 2 unsigned 4bit to be come 1 u_char
            df_uchar = pd.DataFrame()
            print("This will form a char out of 2 4bit data; then it can be used to make a binary")
            # Make sure that we have even number of columns, because we want to paired the columns
            if (len(cols)%2 != 0):
                print("ERROR: The number of column must be even!! Otherwise, the pairing will not work")
                exit(-1)

            new_col = 0
            for idx in range(0, len(cols)):
                if (idx % 2 == 0):
                    # combine two 4bit
                    # 9 + 7 -> (9*16)+7 = 151 
                    # 15 + 15 -> (15*16)+15 = 255
                    df_uchar[new_col] = (df[cols[idx]] * 16) + df[cols[idx + 1]]
                    new_col = new_col + 1
            # print(df_uchar.head())
            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/raw_u_4bit_to_u_char/" + fileName
            write_to_file(df_uchar, outFile)

            # C. 1 csv file with the value as float, to be used as accuracy testing only!
                # we scale down the range to 0-14 then convert back to floats!
            print("This is to be used to test dlrm accuracy using py cache reader")
            for col in cols:
                df[col] = df[col].apply(lambda x: convert_from_4bit_int(x))
            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/raw_u_4bit_to_float/" + fileName
            write_to_file(df, outFile)
   
        elif (args.new_precision == "4new0"):
            # Make sure that 0 is ALWAYS 0
            # New range is only 0-14, so we want to represent the majority
                # we will hold the value of  -0.1 -0.01, -0.001,-0.0001,-0.00001,-0.000001,-0.0000001, 0 ,0.0000001, 0.000001, 0.00001 0.0001 0.001  0.01  0.1
                #                              0      1      2      3       4        5         6       7        8         9        10    11     12    13   14
            print("   Reduce the precision to 4bit")
            # This will produce 3 output file, 
            # A. 1 csv file which will be converted to binary, and used by the C++ pipeline
                # we scale down the range to 0-14 and save it as char, then write to file!!!
            cols = df.columns
            print("This 4bit data is to be paired and form a char")
            for col in cols:
                # new range is 0 - 14
                    # -1    -> x -> 0
                    # -0.1  -> x -> 0
                    # -0.01 -> str(-1.01) -> 1 (count the zeros)
                    # -0.001 -> str(-1.001) -> 2 (count the zeros)
                    # 0     -> 0.0   -> 7
                df[col] = df[col].apply(lambda x: 0 if (x <= -0.1) else 14 if (x >= 0.1) else 
                    # handling the conversion of 1 to 13
                    convert_to_4bit_int_old(x)
                )

            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/raw_u_4bit/" + fileName
            write_to_file(df, outFile)

            # B. Combine every 2 unsigned 4bit to be come 1 u_char
            df_uchar = pd.DataFrame()
            print("This will form a char out of 2 4bit data; then it can be used to make a binary")
            # Make sure that we have even number of columns, because we want to paired the columns
            if (len(cols)%2 != 0):
                print("ERROR: The number of column must be even!! Otherwise, the pairing will not work")
                exit(-1)

            new_col = 0
            for idx in range(0, len(cols)):
                if (idx % 2 == 0):
                    # combine two 4bit
                    # 9 + 7 -> (9*16)+7 = 151 
                    # 15 + 15 -> (15*16)+15 = 255
                    df_uchar[new_col] = (df[cols[idx]] * 16) + df[cols[idx + 1]]
                    new_col = new_col + 1
            # print(df_uchar.head())
            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/raw_u_4bit_to_u_char/" + fileName
            write_to_file(df_uchar, outFile)

            # C. 1 csv file with the value as float, to be used as accuracy testing only!
                # we scale down the range to 0-14 then convert back to floats!
            print("This is to be used to test dlrm accuracy using py cache reader")
            for col in cols:
                # new range is -1 .. 1
                    # 0 -> -0.1
                    # 7 -> 0
                    # 14 -> 0.1
                df[col] = df[col].apply(lambda x: -0.1 if (x == 0) else 0.1 if (x == 14) else convert_from_4bit_int(x))
            #     print(df[col])
            #     break
            # print("Stop test")
            # exit()
            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/raw_u_4bit_to_float/" + fileName
            write_to_file(df, outFile)

        elif (args.new_precision == "4old"):
            # Make sure that 0 is ALWAYS 0
            # New range is only 0-14, so we want to represent the majority
                # we will hold the value of  -0.07 -0.06, -0.05,-0.04,-0.03,-0.02,-0.01, 0 ,0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.07
                #                              0      1     2     3     4     5      6   7    8     9     10    11   12    13   14
            print("   Reduce the precision to 4bit")
            # This will produce 3 output file, 
            # A. 1 csv file which will be converted to binary, and used by the C++ pipeline
                # we scale down the range to 0-14 and save it as char, then write to file!!!
            cols = df.columns
            print("This 4bit data is to be paired and form a char")
            for col in cols:
                # new range is 0 - 14
                    # -1    -> x -> 0
                    # -0.07 -> x -> 0
                    # -0.06 -> 0.01 -> 1
                    # 0     -> 0.07   -> 7
                    # 0.06  -> 0.13 -> 13
                    # 0.07  -> >xxx -> 14
                    # 1     -> >xxx ->  14
                df[col] = df[col].apply(lambda x: 0 if (x <= -0.07) else 14 if (x >= 0.07) else round((x + 0.07)*100) )
            
            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/raw_u_4bit/" + fileName
            write_to_file(df, outFile)

            # B. Combine every 2 unsigned 4bit to be come 1 u_char
            df_uchar = pd.DataFrame()
            print("This will form a char out of 2 4bit data; then it can be used to make a binary")
            # Make sure that we have even number of columns, because we want to paired the columns
            if (len(cols)%2 != 0):
                print("ERROR: The number of column must be even!! Otherwise, the pairing will not work")
                exit(-1)

            new_col = 0
            for idx in range(0, len(cols)):
                if (idx % 2 == 0):
                    # combine two 4bit
                    # 9 + 7 -> (9*16)+7 = 151 
                    # 15 + 15 -> (15*16)+15 = 255
                    df_uchar[new_col] = (df[cols[idx]] * 16) + df[cols[idx + 1]]
                    new_col = new_col + 1
            # print(df_uchar.head())
            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/raw_u_4bit_to_u_char/" + fileName
            write_to_file(df_uchar, outFile)

            # C. 1 csv file with the value as float, to be used as accuracy testing only!
                # we scale down the range to 0-14 then convert back to floats!
            print("This is to be used to test dlrm accuracy using py cache reader")
            for col in cols:
                # new range is -1 .. 1
                    # 0 -> -0.07
                    # 7 -> 0
                    # 14 -> 0.07
                df[col] = df[col].apply(lambda x: -0.07 if (x == 0) else 0.07 if (x == 14) else round((x/100) - 0.07, 2))
            outFile = str(Path(parentDir).parent) + "/ev-table-" + args.new_precision + "/raw_u_4bit_to_float/" + fileName
            write_to_file(df, outFile)

        elif (args.new_precision == "0"):
            print("   Reduce the precision to 0 (FOR TESTING)")
            cols = df.columns
            for col in cols:
                # Make all value to 0
                df[col] = df[col].apply(lambda x: 0)
            write_to_file(df, outFile)
        else:
            print("ERROR: Can't understand the new_precision format : " + args.new_precision)
            exit(-1)

        print("now  = " + str(df['0'].iloc[0]) + " " + str(df['0'].iloc[1]) + " " + str(df['0'].iloc[2]))






