#! /bin/sh

############################################################################
#set env#
echo "set env"
export ARCH="arm"
export RANLIB="arm-oe-linux-gnueabi-ranlib"
export AS="arm-oe-linux-gnueabi-as "
export AR="arm-oe-linux-gnueabi-ar"
export CROSS_COMPILE="arm-oe-linux-gnueabi-"
export NM="arm-oe-linux-gnueabi-nm"
. ./mg_ol_crosstool/set-crosstool-env
############################################################################
#compiling#
echo "compilling"
make mrproper

make ARCH=arm mdm9607_defconfig

make -j 16 CC=arm-oe-linux-gnueabi-gcc   LD=arm-oe-linux-gnueabi-ld.bfd

echo "copy file to mg_mgboot"
cp ./arch/arm/boot/zImage  ./mg_ol_tools/mg_mkboot/
cp ./arch/arm/boot/dts/qcom/mdm9607-cdp.dtb  ./mg_ol_tools/mg_mkboot/
cp ./arch/arm/boot/dts/qcom/mdm9607-mtp.dtb  ./mg_ol_tools/mg_mkboot/
cp ./arch/arm/boot/dts/qcom/mdm9607-mtp-sdcard.dtb  ./mg_ol_tools/mg_mkboot/
cp ./arch/arm/boot/dts/qcom/mdm9607-rcm.dtb  ./mg_ol_tools/mg_mkboot/
cp ./arch/arm/boot/dts/qcom/mdm9607-rumi.dtb  ./mg_ol_tools/mg_mkboot/

#############################################################################
echo "packaging mdm9607-boot.img"
rm -rf ./mg_ol_tools/mg_mkboot/target/*.img
cd mg_ol_tools/mg_mkboot/
./mg_mkboot dtb2img ./
mv ./target/mdm9607-boot.img ./target/mdm9607_boot.img
cd ../../
echo "packaging mdm9607-boot.img end"
