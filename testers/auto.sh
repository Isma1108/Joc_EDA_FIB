#!/bin/sh
for i in {1000..1300}
do
   ./Game Machinima MiniTorinov2 Miquel crack -s $i < default.cnf > default.res 2> aux.txt
   echo -n "Seed $i: " 
   grep 'top' aux.txt
   #grep 'got score' aux.txt
done
