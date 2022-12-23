import csv
import numpy as np

def read_table(table_count):
    fileNames = []
    for i in range(1, table_count+1):
        fileNames.append("../../../stored_model/criteo_kaggle_all/epoch-00/ev-table/"+"ev-table-" + str(i) + ".csv")

    arrOfNpArr = []
    arrOfLen = []
    for name in fileNames:
        print("Processing", name)
        with open(name) as csv_file:
            csv_reader = csv.reader(csv_file, delimiter=',')
            line_count = 0
            for row in csv_reader:
                if line_count != 0:
                    temp = []
                    for col in row:
                        temp.append(float(col))
                    arrOfNpArr.append(temp)

                line_count += 1
            arrOfLen.append(line_count-1)

    arrOfNpArr = np.array(arrOfNpArr)
    print(arrOfNpArr.shape)
    return arrOfNpArr, arrOfLen