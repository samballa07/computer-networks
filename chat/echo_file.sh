#! /bin/bash

input=$1

while IFS= read -r line 
do
    echo "${line}"
    sleep 2
done < ${input}