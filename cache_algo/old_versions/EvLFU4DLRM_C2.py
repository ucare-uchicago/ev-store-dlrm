import pandas as pd
import numpy as np
import time
import os

# EvLFU part
EVPATH_C2 = "weights_and_biases/epoch-0/ev-table-8"
EvTable_C2 = dict()


def loadEvTable(ev_path_c2):
    print("****************Loading embedding layers")

    print("****************Load new set of EV Table from = " + ev_path_c2)

    for ev_idx in range(0, 26):
        # Reference: Daniar's Github

        # Read new EV Table from file
        new_ev_path = os.path.join(ev_path_c2,
                                   "ev-table-" + str(ev_idx + 1) + ".csv")
        new_ev_df = pd.read_csv(new_ev_path, dtype=float, delimiter=',')
        # Convert to numpy first before to tensor
        new_ev_arr = new_ev_df.to_numpy()
        # Convert to tensor
        print("*********************Loading NEW EV per embedding layer = " + new_ev_path)

        global EvTable_C2
        EvTable_C2[ev_idx + 1] = new_ev_arr

    print("****************All EvTable loaded in the Memory!")


def copycatCacheReplacementPolicy(key):
    tableNum, idx = key.split('-', 2)
    global EvTable_C2
    return EvTable_C2[int(tableNum)][int(idx), :]


###########################################EvLFU##########################################
cap_C2 = 1000
min_C2 = 0
vals_C2 = dict()
counts_C2 = dict()
lists_C2 = dict()
lists_C2[0] = []
# flushing part:
nPerfectItem_C2 = 0
flushRate_C2 = 0.4
perfectItemCapacity_C2 = 1.0


def request(group_keys):
    aggHitMissRecord = []
    aggHit = 0
    global cap_C2, min_C2, vals_C2, counts_C2, lists_C2, nPerfectItem_C2, flushRate_C2, perfectItemCapacity_C2
    for key in group_keys:
        if vals_C2.get(key) is not None:
            aggHitMissRecord.append(True)
            aggHit += 1
        else:
            aggHitMissRecord.append(False)
    emb_weights = []
    for key in group_keys:
        val = update(key, aggHit, len(aggHitMissRecord))
        emb_weights.append(val)

    if lists_C2.get(26) and not len(lists_C2.get(26)) == 0:
        nPerfectItem_C2 = len(lists_C2.get(26))

    return aggHitMissRecord, emb_weights


def update(key, aggHit, nGroup):
    val = get_val_from_mem(key, aggHit)
    if val is None:
        val = getValFromStore(key)
        set(key, val, aggHit)
    return val


def get_val_from_mem(key, aggHit):  # Get From Mem
    global cap_C2, min_C2, vals_C2, counts_C2, lists_C2, nPerfectItem_C2, flushRate_C2, perfectItemCapacity_C2
    if vals_C2.get(key) is None:
        return None
    count = counts_C2.get(key)
    newCount = count
    if count < aggHit:
        newCount = aggHit
    counts_C2[key] = newCount
    lists_C2.get(count).remove(key)

    if count == min_C2:
        while (lists_C2.get(min_C2) is None) or len(lists_C2.get(min_C2)) == 0:
            min_C2 += 1
    if lists_C2.get(newCount) is None:
        lists_C2[newCount] = []
        lists_C2 = dict(sorted(lists_C2.items()))
    lists_C2.get(newCount).append(key)
    return vals_C2[key]


def getValFromStore(key):
    # key Format:
    # #EVTABLE-#INDEX
    val = copycatCacheReplacementPolicy(key)
    return val


def set(key, value, aggHit):
    global cap_C2, min_C2, vals_C2, counts_C2, lists_C2, nPerfectItem_C2, flushRate_C2, perfectItemCapacity_C2
    if cap_C2 <= 0:
        return
    if vals_C2.get(key) is not None:
        vals_C2[key] = value
        get_val_from_mem(key, aggHit)
        return

    # Flushing:
    if nPerfectItem_C2 >= int(cap_C2 * perfectItemCapacity_C2):
        # print("flushing!")
        for i in range(0, int(flushRate_C2 * cap_C2) + 1):
            evictKey = lists_C2.get(26)[0]
            lists_C2.get(26).remove(evictKey)
            vals_C2.pop(evictKey)
            counts_C2.pop(evictKey)

        nPerfectItem_C2 = len(lists_C2.get(26))
        if len(vals_C2) < cap_C2:
            min_C2 = aggHit

    # key allows to insert in the cache:
    if aggHit >= min_C2:
        if len(vals_C2) >= cap_C2:
            evictKey = lists_C2.get(min_C2)[0]
            lists_C2.get(min_C2).remove(evictKey)
            vals_C2.pop(evictKey)
            counts_C2.pop(evictKey)
        # If the key is new, insert the value:
        vals_C2[key] = value
        counts_C2[key] = aggHit

        if lists_C2.get(aggHit) is None:
            lists_C2[aggHit] = []
            lists_C2 = dict(sorted(lists_C2.items()))
        lists_C2.get(aggHit).append(key)
    else:
        min_C2 = aggHit
        if len(vals_C2) < cap_C2:
            vals_C2[key] = value
            counts_C2[key] = aggHit

            if lists_C2.get(aggHit) is None:
                lists_C2[aggHit] = []
                lists_C2 = dict(sorted(lists_C2.items()))
            lists_C2.get(aggHit).append(key)
    while (lists_C2.get(min_C2) is None) or len(lists_C2.get(min_C2)) == 0:
        min_C2 += 1


def main():
    global cap_C2, min_C2, vals_C2, counts_C2, lists_C2, nPerfectItem_C2, flushRate_C2, perfectItemCapacity_C2
    # giving workload:
    workload_dir = "D:\\github\\EV\\cache-benchmark\\Archive-new-0.5M\\"
    workload_files = []
    workload_files.append("workload-group-1.csv")
    workload_files.append("workload-group-2.csv")
    workload_files.append("workload-group-3.csv")
    workload_files.append("workload-group-5.csv")
    workload_files.append("workload-group-10.csv")
    workload_files.append("workload-group-11.csv")
    workload_files.append("workload-group-12.csv")
    workload_files.append("workload-group-20.csv")
    workload_files.append("workload-group-21.csv")
    workload_files.append("workload-group-22.csv")
    workload_files.append("workload-group-23.csv")

    workload_files.append("workload-group-4.csv")
    workload_files.append("workload-group-6.csv")
    workload_files.append("workload-group-7.csv")
    workload_files.append("workload-group-8.csv")
    workload_files.append("workload-group-9.csv")
    workload_files.append("workload-group-13.csv")
    workload_files.append("workload-group-14.csv")
    workload_files.append("workload-group-15.csv")
    workload_files.append("workload-group-16.csv")
    workload_files.append("workload-group-17.csv")
    workload_files.append("workload-group-18.csv")
    workload_files.append("workload-group-19.csv")
    workload_files.append("workload-group-24.csv")
    workload_files.append("workload-group-25.csv")
    workload_files.append("workload-group-26.csv")

    nTableWorkload = len(workload_files)
    arrRawWorkload = []

    # read all workloads:
    for workload_file in workload_files:
        workload = np.asarray(pd.read_csv(workload_dir + workload_file, header=None).values[0:500000, 0])
        arrRawWorkload.append(workload)

    arrRawWorkload = np.asarray(arrRawWorkload)
    # print(arrRawWorkload.shape)
    # merge the workloads
    arrMergedWorkload = np.stack(arrRawWorkload, axis=1)
    groupedWorkloadKeys = arrMergedWorkload
    print(arrMergedWorkload.shape)
    print("Done merging ALL workloads: total = ", arrRawWorkload.shape[0], 'rows')

    # Run the Alg:

    start_time = time.time()
    prefectHit = 0
    countR = 0
    for groupKeys in groupedWorkloadKeys:
        aggHitMissRecord, _ = request(groupKeys)
        flag = True
        for isHit in aggHitMissRecord:
            if not isHit:
                flag = False
                break
        if flag:
            prefectHit += 1
    print(prefectHit)
    print("time:")
    print(time.time() - start_time)


if __name__ == '__main__':
    loadEvTable(EVPATH_C2)
    main()