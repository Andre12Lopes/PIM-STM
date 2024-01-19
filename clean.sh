#/bin/bash

tm_folders="NoREC RWLocksSTM TinySTM"
benchmark_folders="Bank Kmeans LinkedList Labyrinth Labyrinth_Multi_DPU"

cd Backends
for f in $tm_folders; do
	cd $f
	make clean
	cd ..
done
cd ..

for f in $benchmark_folders; do
	cd $f
	make clean
	cd ..
done