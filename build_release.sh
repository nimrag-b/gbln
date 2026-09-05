#!/bin/bash

mkdir -p build

gcc -O3 main.c hashmap.c gblnvm.c gblncompiler.c -o gbln
