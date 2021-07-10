#! /bin/sh

############################################################################
#set env#
export ARCH="arm"
export RANLIB="arm-oe-linux-gnueabi-ranlib"
export AS="arm-oe-linux-gnueabi-as "
export AR="arm-oe-linux-gnueabi-ar"
export CROSS_COMPILE="arm-oe-linux-gnueabi-"
export NM="arm-oe-linux-gnueabi-nm"

############################################################################
#compiling#
make mrproper

make ARCH=arm mdm9607_defconfig

make -j 8 CC=arm-oe-linux-gnueabi-gcc   LD=arm-oe-linux-gnueabi-ld.bfd 2>&1 | tee log.txt
make -j 8 CC=arm-oe-linux-gnueabi-gcc  modules | tee log.txt

cd ../
cp mt670_kernel/arch/arm/boot/zImage  mt670_kernel/mg_ol_tools/mg_mkboot/
cp mt670_kernel/arch/arm/boot/dts/qcom/mdm9607-cdp.dtb  mt670_kernel/mg_ol_tools/mg_mkboot/
cp mt670_kernel/arch/arm/boot/dts/qcom/mdm9607-mtp.dtb  mt670_kernel/mg_ol_tools/mg_mkboot/
cp mt670_kernel/arch/arm/boot/dts/qcom/mdm9607-mtp-sdcard.dtb  mt670_kernel/mg_ol_tools/mg_mkboot/
cp mt670_kernel/arch/arm/boot/dts/qcom/mdm9607-rcm.dtb  mt670_kernel/mg_ol_tools/mg_mkboot/
cp mt670_kernel/arch/arm/boot/dts/qcom/mdm9607-rumi.dtb  mt670_kernel/mg_ol_tools/mg_mkboot/

#############################################################################
