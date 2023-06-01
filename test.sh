#!/bin/bash
make clean && make && clear && ./bin/cfcc > bin/test.S && gcc bin/test.S -o bin/out && ./bin/out
echo $?
