import torch
import sys
import numpy as np
import pandas as pd
import time
import random
import EvLFU


workload_dir = "/home/cc/workload/Archive-new-1.0M/"
workload_files = []
for i in range(1, 27):
    workload_files.append("workload-group-" + str(i) + ".csv")
arrRawWorkload = []
# read all workloads:
for workload_file in workload_files:
    workload = np.asarray(pd.read_csv(workload_dir + workload_file).values[:, 0])
    arrRawWorkload.append(workload)

arrRawWorkload = np.asarray(arrRawWorkload)
# print(arrRawWorkload.shape)
# merge the workloads
arrMergedWorkload = np.stack(arrRawWorkload, axis=1)
groupedWorkloadKeys = arrMergedWorkload
print(arrMergedWorkload.shape)
print("Done merging ALL workloads: total = ", arrRawWorkload.shape[0], 'rows')

# Run the alg:
EvLFU.cinit(768)
prefectHit = 0

groupedWorkloadIds = []

for groupKeys in groupedWorkloadKeys:
    groupKeys = groupKeys.tolist()
    for i in range(26):
        groupKeys[i] = int(groupKeys[i].split('-')[1])

    groupedWorkloadIds.append(groupKeys)

EvLFU.cload_ev_tables()
start_time = time.time()

for group_row_ids in groupedWorkloadIds:
    # print(type(groupKeys))
    # print(type(groupKeys[0]))
    aggHitMissRecord, x = EvLFU.crequest(group_row_ids, False)
    # print(aggHitMissRecord)
    # print(x)
    # exit(0)
    flag = True
    for isHit in aggHitMissRecord:
        if not isHit:
            flag = False
            break
    if flag:
        prefectHit += 1
print("perfect hit:", prefectHit)
print(time.time() - start_time)
EvLFU.cclose_ev_tables()