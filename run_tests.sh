#!/usr/bin/env bash
set -euo pipefail

DEBUG=${DEBUG:-false}

CC=${CC:-gcc}
# if debug
if [ "$DEBUG" = "true" ]; then
  CFLAGS="-std=c11 -O0 -g -Wall -Wextra -pedantic"
else
  CFLAGS="-std=c11 -O2 -Wall -Wextra -pedantic"
fi
# On some Linux distros, C11 threads need -pthread (even when using threads.h)
LDFLAGS="-pthread"

# build
$CC $CFLAGS -c queue.c -o queue.o

$CC $CFLAGS test_basic.c    queue.o -o test_basic    $LDFLAGS
$CC $CFLAGS test_blocking.c queue.o -o test_blocking $LDFLAGS
$CC $CFLAGS test_fairness.c queue.o -o test_fairness $LDFLAGS
$CC $CFLAGS test_stress.c   queue.o -o test_stress   $LDFLAGS

echo "Running basic..."
./test_basic

echo "Running blocking..."
./test_blocking

echo "Running fairness..."
./test_fairness

echo "Running stress (many seeds)..."
for seed in $(seq 1 30); do
  ./test_stress "$seed"
done

echo "Running stress (10 randomized seeds)..."
for i in $(seq 1 10); do
  seed=$RANDOM
  echo "Random seed: $seed"
  ./test_stress "$seed"
done

echo "ALL TESTS PASS"
