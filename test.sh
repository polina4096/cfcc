#!/bin/bash
make clean && make -j8 && clear && ./bin/cfcc > bin/test.S && gcc bin/test.S -o bin/out && ./bin/out
