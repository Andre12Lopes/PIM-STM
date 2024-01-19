#/bin/bash

if [[ $# != 3 ]]; then
	echo "Invalid number of parameters"
	echo "Usage: ./test.sh [backend {norec|rwlocks_wbctl|rwlocks_wbetl|rwlocks_wtetl|tiny_wbctl|tiny_wbetl|tiny_wtetl}] [benchmark {bank|linkedlist|kmeans|labyrinth}] [contention]"
	exit 1
fi

folder=${2^}

if [[ $folder == "Linkedlist" ]]; then
	folder="LinkedList"
fi

if [[ ! -d "Results/$folder/$1" ]]; then
	mkdir -p "./Results/$folder/$1"
fi

echo -e "N_THREADS\tN_TRANSACTIONS\tTIME\tN_ABORTS\tPROCESS_READ_TIME\tPROCESS_WRITE_TIME\tPROCESS_VALIDATION_TIME\tPROCESS_OTHER_TIME\tCOMMIT_VALIDATION_TIME\tCOMMIT_OTHER_TIME\tWASTED_TIME" > Results/$folder/$1/results_mram_$3.txt

for (( i = 1; i < 13; i++ )); do
	bash build.sh $1 $2 $3 $i > /dev/null
	for (( j = 0; j < 10; j++ )); do
		RES=$(/usr/bin/time -f '%e' ./$folder/bin/host 2>&1)
		echo "$RES" | head -n 1 | tr -d '\n' >> Results/$folder/$1/results_mram_$3.txt
		echo -ne "\t" >> Results/$folder/$1/results_mram_$3.txt
		echo "$RES" | tail -1 >> Results/$folder/$1/results_mram_$3.txt
		#./$folder/bin/host >> Results/$folder/$1/results_mram_$3.txt
	done
done
