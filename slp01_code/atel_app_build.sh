#!/bin/sh

##Virtual address start from 0x4000_0000
DAM_RO_BASE=0x40000000

TOOLCHAIN_PATH="C:/compile_tools/ARM_Compiler_5/bin"
#export LM_LICENSE_FILE="C:/compile_tools/license.dat"
export LM_LICENSE_FILE="C:/compile_tools/armlic.dat"

SH_COMMAND=./atel_app_build.sh
#DAM related path
DAM_OUTPUT_PATH="./bin"
DAM_INC_BASE="./include"
DAM_LIB_PATH="./libs"

#application related path
APP_SRC_P_PATH="./src"
APP_OUTPUT_PATH="./src/build"

#example utils source and header path
APP_UTILS_SRC_PATH+="./src/utils/source"
APP_UTILS_INC_PATH+="./src/utils/include"

#parse command line input parameters
if [ $# -eq 1 ]; then
    if [ $1 == "-c" ]; then
		echo "Cleaning..."
		rm -rf $DAM_OUTPUT_PATH/*
		rm -rf $APP_OUTPUT_PATH/*o
		echo "Done."
		exit
	# build app
	elif [ $1 == "app" ]; 	then	BUILD_APP_FLAG="-D__ATEL_BG96_APP__"
	# build help
	elif [ $1 == "help" ] || [ $1 == "?" ];			then
			echo "  app         [ cmd - \"$SH_COMMAND app\"         ]"
			exit
	else
		echo "Please input a valid example build id !"
		exit
	fi
else
	echo "error usage !"
	exit
fi

#example source and header path
APP_SRC_PATH="./src/$1/src"
APP_INC_PATH="./src/$1/inc"

#depend modules
MODULE_DEPENDS_SRC="./src/modules_depend/src"
MODULE_DEPENDS_INC="./src/modules_depend/inc"


DAM_LIBNAME="txm_lib.lib"
TIMER_LIBNAME="timer_dam_lib.lib"

DAM_ELF_NAME="atel_BG96_$1.elf"
DAM_TARGET_BIN="atel_BG96_$1.bin"
DAM_TARGET_MAP="atel_BG96_$1.map"

if [ ! -d $DAM_OUTPUT_PATH ]; then
    mkdir $DAM_OUTPUT_PATH
fi

echo "=== Application RO base selected = $DAM_RO_BASE"

export DAM_CPPFLAGS="-DT_ARM -D__RVCT__ -D_ARM_ASM_ -DQAPI_TXM_MODULE -DTXM_MODULE -DTX_DAM_QC_CUSTOMIZATIONS -DTX_ENABLE_PROFILING -DTX_ENABLE_EVENT_TRACE -DTX_DISABLE_NOTIFY_CALLBACKS -DTX_DAM_QC_CUSTOMIZATIONS" 
export DAM_CLAGS="-O1 --diag_suppress=9931 --diag_error=warning --cpu=Cortex-A7 --protect_stack --arm_only --apcs=/interwork"
export DAM_INCPATHS="-I $DAM_INC_BASE -I $DAM_INC_BASE/threadx_api -I $DAM_INC_BASE/qapi -I $APP_UTILS_INC_PATH -I $APP_INC_PATH -I $MODULE_DEPENDS_INC"
export APP_CFLAGS="-DTARGET_THREADX -DENABLE_IOT_INFO -DENABLE_IOT_DEBUG -DSENSOR_SIMULATE"

MM16_BUILD_FLAG="-DATEL_GPS_LAN_POWER_ON"
#Turn on verbose mode by default
set -x;

echo "=== Compiling Example $1"
echo "== Compiling .S file..."
$TOOLCHAIN_PATH/armcc -E -g $DAM_CPPFLAGS $DAM_CFLAGS $APP_SRC_P_PATH/txm_module_preamble.S > txm_module_preamble_pp.S
$TOOLCHAIN_PATH/armcc -g -c $DAM_CPPFLAGS $DAM_CFLAGS txm_module_preamble_pp.S -o $APP_OUTPUT_PATH/txm_module_preamble.o
rm txm_module_preamble_pp.S

echo "== Compiling .C file..."
$TOOLCHAIN_PATH/armcc -g -c $DAM_CPPFLAGS $DAM_CFLAGS $APP_CFLAGS $BUILD_APP_FLAG $MM16_BUILD_FLAG $DAM_INCPATHS $APP_UTILS_SRC_PATH/*.c $APP_SRC_PATH/*.c $MODULE_DEPENDS_SRC/*.c 
mv *.o $APP_OUTPUT_PATH

echo "=== Linking Example $1 application"
$TOOLCHAIN_PATH/armlink -d -o $DAM_OUTPUT_PATH/$DAM_ELF_NAME --elf --ro $DAM_RO_BASE --first txm_module_preamble.o --entry=_txm_module_thread_shell_entry --map --remove --symbols --list $DAM_OUTPUT_PATH/$DAM_TARGET_MAP $APP_OUTPUT_PATH/*.o $DAM_LIB_PATH/$DAM_LIBNAME $DAM_LIB_PATH/$TIMER_LIBNAME
$TOOLCHAIN_PATH/fromelf --bincombined $DAM_OUTPUT_PATH/$DAM_ELF_NAME --output $DAM_OUTPUT_PATH/$DAM_TARGET_BIN

set +x;

echo "=== application $1 is built at" $DAM_OUTPUT_PATH/$DAM_TARGET_BIN
echo -n "/datatx/atel_BG96_$1.bin" > ./bin/oem_app_path.ini
echo "Done."
