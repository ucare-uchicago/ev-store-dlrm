#!/usr/bin/env python
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from pathlib import Path
import os
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("--input-file", help="File path of the raw cdf data", type=str)

args = parser.parse_args()

parentDir = str(Path(args.input_file).parent)
filename = os.path.basename(args.input_file).split('.')[0]
output_path = os.path.join(parentDir, filename + ".png")

df=pd.read_csv(args.input_file, sep=',')
x_label = df.columns[1]
y_label = df.columns[0]

# define data values
plt.plot(df[x_label], df[y_label])  # Plot the chart
# plt.show()  # display
plt.title('CDF of Latency')
plt.xlabel(x_label)
plt.ylabel("CDF")

# set y and x axis range
plt.ylim(ymin=0)  # this line
plt.ylim(ymax=1)  # this line
plt.xlim(xmin=0)
plt.xlim(xmax=4)

plt.savefig(output_path)
print("CDF figure is written at : " + output_path)
