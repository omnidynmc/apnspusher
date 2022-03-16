#!/bin/bash
make makeall &&
valgrind --tool=memcheck --leak-check=yes --undef-value-errors=no --log-file=valgrind.log ./apnspusher
