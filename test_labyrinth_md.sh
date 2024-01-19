#/bin/bash

if [[ $# != 1 ]]; then
	echo "Invalid number of parameters"
	echo "Usage: ./test.sh [backend {norec|rwlocks_wbctl|rwlocks_wbetl|rwlocks_wtetl|tiny_wbctl|tiny_wbetl|tiny_wtetl}]"
	exit 1
fi

folder="Labyrinth_Multi_DPU"

if [[ ! -d "Results/$folder/$1" ]]; then
	mkdir -p "./Results/$folder/$1"
fi

echo -e "N_THREADS_DPU\tNUM_TXS_PER_DPU\tN_DPUS\tCOMM_TIME\tTOTAL_TIME" > Results/$folder/$1/results_mram.txt

DPUS="1 500 1000 1500 2000 2500"

for d in $DPUS; do
	bash build.sh $1 labyrinth_md $d 5 > /dev/null
	for (( j = 0; j < 5; j++ )); do
		./$folder/bin/host >> Results/$folder/$1/results_mram.txt
	done
done
