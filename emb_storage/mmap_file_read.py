import os
import torch
import struct
import mmap

BINARY_DIR_NAME = "binary/"
arr_files = []
arr_mmap_files = []
TOTAL_BYTE_PER_ROW = -1
EV_DIMENSION = 36

# Load value as bytes!!
def open_files_as_binary(ev_path_c1, bit_precision = 32):
    global arr_files, TOTAL_BYTE_PER_ROW
    BYTE_PRECISION = int(bit_precision/8)
    TOTAL_BYTE_PER_ROW = EV_DIMENSION * BYTE_PRECISION

    print("**************** Opening all Binary EV-files")
    print("**************** from = " + ev_path_c1)
    arr_files.append("ID Zero is not being used!")
    arr_mmap_files.append("ID Zero is not being used!")
    for ev_idx in range(0, 26):
        binFilename = "ev-table-" + str(ev_idx + 1) + ".bin"
        bin_ev_path = os.path.join(ev_path_c1, BINARY_DIR_NAME, binFilename)
        print("************* Opening Binnary EV = " + bin_ev_path)
        file = open(bin_ev_path, 'rb')
        arr_files.append(file)
        arr_mmap_files.append(mmap.mmap(file.fileno(), 0, prot=mmap.PROT_READ))
    print("**************** All Files are opened!")
    print("**************** TOTAL_BYTE_PER_ROW = " + str(TOTAL_BYTE_PER_ROW))

def get(tableId, rowId):
    # tableId started at id = 1
    # file = arr_files[tableId]
    file = arr_mmap_files[tableId]
    # print(TOTAL_BYTE_PER_ROW * rowId )
    file.seek(TOTAL_BYTE_PER_ROW * rowId)
    blob = file.read(TOTAL_BYTE_PER_ROW)
    # return struct.unpack('f'*36, blob[0:TOTAL_BYTE_PER_ROW])
    return struct.unpack('f'*36, blob)

def close():
    arr_files.pop(0) # this item0 is not really a file
    for file in arr_files:
        file.close()
    print("**************** All Files are closed!")
