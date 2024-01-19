#!/usr/bin/python3

import statistics
import sys
from pathlib import Path

def read_file(name):
	f = open(name, "r")

	lines = []
	for line in f.readlines():
		lines.append(line.rstrip().split("\t"))
	f.close()
	
	return lines[1:]


def process(file, num_threads, num_runs, output):
	data = read_file(file)

	time = []
	energy = []
	aborts = []
	for i in range(num_threads):
		time.append([])
		energy.append([])

	# print(data)
	for i in range(num_threads):
		for j in range(num_runs):
			time[i].append(float(data[(i * num_runs) + j][2]))
			energy[i].append(float(data[(i * num_runs) + j][3]))
	# print(time)
	
	f = open(f'./{output}', 'w')
	f.write(f'N_POINTS\tAVG_TIME\tSTDEV_TIME\tAVG_ENERGY\tSTDEV_ENERGY\n')
	for i in range(num_threads):
		f.write(f'{data[i * num_runs][0]}\t{statistics.mean(time[i])}\t{statistics.stdev(time[i])}\t{statistics.mean(energy[i])}\t{statistics.stdev(energy[i])}\n')
	f.close()

if __name__ == "__main__":
	process(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4])