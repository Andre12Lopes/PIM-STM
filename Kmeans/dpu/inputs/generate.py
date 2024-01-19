#!/usr/bin/python3

import random
import sys

if len(sys.argv) != 4:
    print("Usage: generate.py <rows> <dimensions> <centers>")
    sys.exit(1)

numRow    = int(sys.argv[1])
numDim    = int(sys.argv[2])
numCenter = int(sys.argv[3])

random.seed(0)


# clustered random using gaussian
centers = []
for i in range(numCenter):
    center = []
    for dim in range(numDim):
        center.append(random.random())
    centers.append(center)
sigma = (1. / numCenter) ** 3
for row in range(1, numRow+1):
    center = random.choice(centers)
    print("{", end=' ')
    for dim in range(numDim):
        noise = random.gauss(0, sigma)
        print(center[dim] + noise, end=', ')
    print("},")
    