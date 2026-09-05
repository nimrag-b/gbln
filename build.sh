#!/bin/bash

mkdir -p build

gcc -g -fsanitize=address main.c hashmap.c gblnvm.c gblncompiler.c -o build/gbln
