#!/bin/sh -e

export ARCH=arm
export CROSS_COMPILE=${CROSS_COMPILE:-arm-hisiv500-linux-}

OUTPUTDIR="${OUTPUTDIR:-.}"
SOCS="hi3516av200 hi3519v101"

for soc in ${SOCS}; do
	make clean
	make ${soc}_config
	cp reg_info_${soc}.bin .reg
	make -j$(nproc)
	make mini-boot.bin
	cp mini-boot.bin "${OUTPUTDIR}/u-boot-${soc}-universal.bin"
done
