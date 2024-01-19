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


def process(file, num_threads, num_runs, num_loops, output):
	data = read_file(file)

	time = []
	energy = []
	for i in range(num_runs):
		time.append([])
		energy.append([])

	for i in range(num_runs):
		time[i].append(float(data[i][3])/num_loops)
		time[i].append(float(data[i][4])/num_loops)
		time[i].append(float(data[i][5])/num_loops)
		time[i].append(float(data[i][6])/num_loops)

		energy[i].append(float(data[i][7])/num_loops)
		energy[i].append(float(data[i][8])/num_loops)
		energy[i].append(float(data[i][9])/num_loops)
		energy[i].append(float(data[i][10])/num_loops)

	avg_time = []
	avg_energy = []
	for i in range(num_runs):
		avg_time.append(statistics.mean(time[i]))
		avg_energy.append(statistics.mean(energy[i]))

	f = open(f'./{output}', 'w')
	f.write(f'GRID_SIZE\tN_THREADS_PER_PROC\tN_PROCS\tAVG_TIME\tSTDEV_TIME\tAVG_ENERGY\tSTDEV_ENERGY\n')
	f.write(f'{data[0][0]}\t{data[0][1]}\t{data[0][2]}\t{statistics.mean(avg_time)}\t{statistics.stdev(avg_time)}\t{statistics.mean(avg_energy)}\t{statistics.stdev(avg_energy)}\n')
	f.close()

if __name__ == "__main__":
	process(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), sys.argv[5])