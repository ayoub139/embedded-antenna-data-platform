#!/bin/bash

# Nombre de clients à lancer
NUM_CLIENTS=5

# Chemin vers le binaire client
CLIENT="./client"

echo "Launching $NUM_CLIENTS clients..."

for i in $(seq 1 "$NUM_CLIENTS"); do
    echo "Starting client $i"
    "$CLIENT" &
done

wait

echo "All clients have finished."
