#!/bin/bash

set -e

ESDK=${EPIPHANY_HOME}
ELIBS=${ESDK}/tools/host/lib
EINCS=${ESDK}/tools/host/include
ELDF=${ESDK}/bsps/current/internal.ldf

SCRIPT=$(readlink -f "$0")
EXEPATH=$(dirname "$SCRIPT")
cd $EXEPATH

mkdir -p bin

# Build HOST side application
${CROSS_COMPILE}gcc src/scheduler.c -o bin/scheduler.elf -I ${EINCS} -L ${ELIBS} -le-hal -le-loader

# Build DEVICE side program
e-gcc -g -O2 -T ${ELDF} src/e_scheduler.c -o bin/e_scheduler.elf -le-lib -lm
