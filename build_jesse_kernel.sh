#!/bin/bash
# Jesse kernel build script v0.1

BUILD_COMMAND=$1

MODEL=${BUILD_COMMAND%%_*}
TEMP=${BUILD_COMMAND#*_}
REGION=${TEMP%%_*}
CARRIER=${TEMP##*_}
PRODUCT_NAME=${MODEL}${CARRIER}

BUILD_WHERE=$(pwd)
BUILD_KERNEL_DIR=$BUILD_WHERE
BUILD_ROOT_DIR=$BUILD_KERNEL_DIR/..
BUILD_KERNEL_OUT_DIR=$BUILD_ROOT_DIR/kernel_out/JESSE_KERNEL_OBJ
PRODUCT_OUT=$BUILD_ROOT_DIR/kernel_out

BUILD_CROSS_COMPILE=aarch64-linux-gnu-
BUILD_JOB_NUMBER=`grep processor /proc/cpuinfo|wc -l`

# Default Python version is 2.7
mkdir -p bin
ln -sf /usr/bin/python2.7 ./bin/python
export PATH=$(pwd)/bin:$PATH
KERNEL_DEFCONFIG=exynos8890-hero2lte_jesse_defconfig

#sed -i.bak "s/CONFIG_MODVERSIONS=y/CONFIG_MODVERSIONS=n/g" ${BUILD_KERNEL_DIR}/arch/arm/configs/${KERNEL_DEFCONFIG}

while getopts "w:t:" flag; do
	case $flag in
		w)
			BUILD_OPTION_HW_REVISION=$OPTARG
			echo "-w : "$BUILD_OPTION_HW_REVISION""
			;;
		t)
			TARGET_BUILD_VARIANT=$OPTARG
			echo "-t : "$TARGET_BUILD_VARIANT""
			;;
		*)
			echo "wrong 2nd param : "$OPTARG""
			exit -1
			;;
	esac
done

shift $((OPTIND-1))

DTS_NAMES=apq8084-sec-

case $1 in
		clean)
		echo "Not support... remove kernel out directory by yourself"
		exit 1
		;;
		
		*)
		
		BOARD_KERNEL_BASE=0x00000000
		BOARD_KERNEL_PAGESIZE=4096
		BOARD_KERNEL_TAGS_OFFSET=0x01E00000
		BOARD_RAMDISK_OFFSET=0x02000000
		BOARD_KERNEL_CMDLINE="console=ttyHSL0,115200,n8 androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x37 ehci-hcd.park=3"
		mkdir -p $BUILD_KERNEL_OUT_DIR
		;;

esac

KERNEL_IMG=$BUILD_KERNEL_OUT_DIR/arch/arm64/boot/Image
DTC=$BUILD_KERNEL_OUT_DIR/scripts/dtc/dtc

FUNC_CLEAN_DTB()
{
	if ! [ -d $BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts ] ; then
		echo "no directory : "$BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts""
	else
		echo "rm files in : "$BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts/*.dtb""
		rm $BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts/*.dtb
	fi
}

INSTALLED_DTIMAGE_TARGET=${PRODUCT_OUT}/dt.img
DTBTOOL=$BUILD_KERNEL_DIR/tools/dtbTool

FUNC_BUILD_DTIMAGE_TARGET()
{
	echo ""
	echo "================================="
	echo "START : FUNC_BUILD_DTIMAGE_TARGET"
	echo "================================="
	echo ""
	echo "DT image target : $INSTALLED_DTIMAGE_TARGET"
	
	if ! [ -e $DTBTOOL ] ; then
		if ! [ -d $BUILD_ROOT_DIR/android/out/host/linux-x86/bin ] ; then
			mkdir -p $BUILD_ROOT_DIR/android/out/host/linux-x86/bin
		fi
		cp $BUILD_ROOT_DIR/kernel/tools/dtbTool $DTBTOOL
	fi

	echo "$DTBTOOL -o $INSTALLED_DTIMAGE_TARGET -s $BOARD_KERNEL_PAGESIZE \
						-p $BUILD_KERNEL_OUT_DIR/scripts/dtc/ $BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts/"
	$DTBTOOL -o $INSTALLED_DTIMAGE_TARGET -s $BOARD_KERNEL_PAGESIZE \
						-p $BUILD_KERNEL_OUT_DIR/scripts/dtc/ $BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts/

	chmod a+r $INSTALLED_DTIMAGE_TARGET

	echo ""
	echo "================================="
	echo "END   : FUNC_BUILD_DTIMAGE_TARGET"
	echo "================================="
	echo ""
}

FUNC_BUILD_KERNEL()
{
	echo ""
        echo "=============================================="
        echo "START : FUNC_BUILD_KERNEL"
        echo "=============================================="
        echo ""
        echo "build project="$PROJECT_NAME""
        echo "build common config="$KERNEL_DEFCONFIG ""

	FUNC_CLEAN_DTB
	mkdir $BUILD_KERNEL_DIR/output
	rm $BUILD_KERNEL_DIR/output/Image $KERNEL_IMG
	rm $BUILD_KERNEL_OUT_DIR/firmware/apm_8890_evt1.h
	ln -s $BUILD_KERNEL_DIR/firmware/apm_8890_evt1.h $BUILD_KERNEL_OUT_DIR/firmware/apm_8890_evt1.h
	rm $BUILD_KERNEL_OUT_DIR/init/vmm.elf
	ln -s $BUILD_KERNEL_DIR/init/vmm.elf $BUILD_KERNEL_OUT_DIR/init/vmm.elf
	make -C $BUILD_KERNEL_DIR O=$BUILD_KERNEL_OUT_DIR -j$BUILD_JOB_NUMBER ARCH=arm64 \
			CROSS_COMPILE=$BUILD_CROSS_COMPILE \
			$KERNEL_DEFCONFIG || exit -1

	cp $BUILD_KERNEL_OUT_DIR/.config $BUILD_KERNEL_DIR/arch/arm64/configs/$KERNEL_DEFCONFIG
	make -C $BUILD_KERNEL_DIR O=$BUILD_KERNEL_OUT_DIR -j$BUILD_JOB_NUMBER ARCH=arm64 \
			CROSS_COMPILE=$BUILD_CROSS_COMPILE || exit -1

	cp $KERNEL_IMG $BUILD_KERNEL_DIR/output/Image
#	FUNC_BUILD_DTIMAGE_TARGET
	
	echo ""
	echo "================================="
	echo "END   : FUNC_BUILD_KERNEL"
	echo "================================="
	echo ""
}

FUNC_MKBOOTIMG()
{
	echo ""
	echo "==================================="
	echo "START : FUNC_MKBOOTIMG"
	echo "==================================="
	echo ""
	MKBOOTIMGTOOL=$BUILD_ROOT_DIR/android/kernel/tools/mkbootimg

	if ! [ -e $MKBOOTIMGTOOL ] ; then
		if ! [ -d $BUILD_ROOT_DIR/android/out/host/linux-x86/bin ] ; then
			mkdir -p $BUILD_ROOT_DIR/android/out/host/linux-x86/bin
		fi
		cp $BUILD_ROOT_DIR/anroid/kernel/tools/mkbootimg $MKBOOTIMGTOOL
	fi

	echo "Making boot.img ..."
	echo "	$MKBOOTIMGTOOL --kernel $KERNEL_IMG \
			--ramdisk $PRODUCT_OUT/ramdisk.img \
			--output $PRODUCT_OUT/boot.img \
			--cmdline "$BOARD_KERNEL_CMDLINE" \
			--base $BOARD_KERNEL_BASE \
			--pagesize $BOARD_KERNEL_PAGESIZE \
			--ramdisk_offset $BOARD_RAMDISK_OFFSET \
			--tags_offset $BOARD_KERNEL_TAGS_OFFSET \
			--dt $INSTALLED_DTIMAGE_TARGET"
			
	$MKBOOTIMGTOOL --kernel $KERNEL_IMG \
			--ramdisk $PRODUCT_OUT/ramdisk.img \
			--output $PRODUCT_OUT/boot.img \
			--cmdline "$BOARD_KERNEL_CMDLINE" \
			--base $BOARD_KERNEL_BASE \
			--pagesize $BOARD_KERNEL_PAGESIZE \
			--ramdisk_offset $BOARD_RAMDISK_OFFSET \
			--tags_offset $BOARD_KERNEL_TAGS_OFFSET \
			--dt $INSTALLED_DTIMAGE_TARGET
	
	cd $PRODUCT_OUT
	tar cvf boot_${MODEL}_${CARRIER}.tar boot.img

	cd $BUILD_ROOT_DIR
	if ! [ -d output ] ; then
		mkdir -p output
	fi

	mv $PRODUCT_OUT/boot_${MODEL}_${CARRIER}.tar output/

	cd ~
	
	echo ""
	echo "==================================="
	echo "END   : FUNC_MKBOOTIMG"
	echo "==================================="
	echo ""	
}

# MAIN FUNCTION
rm -rf ./build.log
(
    START_TIME=`date +%s`

	FUNC_BUILD_KERNEL
	#FUNC_RAMDISK_EXTRACT_N_COPY

    END_TIME=`date +%s`
	
    let "ELAPSED_TIME=$END_TIME-$START_TIME"
    echo "Total compile time is $ELAPSED_TIME seconds"
) 2>&1	 | tee -a ./build.log
