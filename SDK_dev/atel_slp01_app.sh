#!/bin/sh

##Virtual address start from 0x43000000
DAM_RO_BASE=0x43000000

TOOL_PATH_ROOT="C:/compile_tools/LLVM/4.0.3"
TOOLCHAIN_PATH=$TOOL_PATH_ROOT/bin
TOOLCHAIN_PATH_STANDARdS=$TOOL_PATH_ROOT/armv7m-none-eabi/libc/include
LLVMLIB=$TOOL_PATH_ROOT/lib/clang/4.0.3/lib
LLVMLINK_PATH=$TOOL_PATH_ROOT/tools/bin
PYTHON_PATH="C:/Python27/python.exe"

export ARMLMD_LICENSE_FILE="8620@192.168.12.149"

SH_COMMAND=atel_slp01_app.sh
#DAM related path
DAM_OUTPUT_PATH="./bin"
DEMO_ELF_OUTPUT_PATH="./bin"
DAM_INC_BASE="./include"
DAM_LIB_PATH="./libs"
DEMO_SRC_PATH="./src"
DEMO_APP_UTILS_INC_PATH="./src/utils/include"

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

#interface
EVENT_IF_SRC="./src/interface/src"
EVENT_IF_INC="./src/interface/inc"

#example source and header path
DEMO_APP_SRC_PATH="./src/$1/src"
DEMO_APP_INC_PATH="./src/$1/inc"
DEMO_APP_OUTPUT_PATH="./src/build"
DEMO_APP_LD_PATH="./src/build"
DEMO_APP_UTILS_SRC_PATH="./src/utils/source"
DEMO_APP_UTILS_INC_PATH="./src/utils/include"

DAM_LIBNAME="txm_lib.lib"
TIMER_LIBNAME="timer_dam_lib.lib"
DIAG_LIB_NAME="diag_dam_lib.lib"
QMI_LIB_NAME="qcci_dam_lib.lib"
QMI_QCCLI_LIB_NAME="IDL_DAM_LIB.lib"

DAM_ELF_NAME="atel_slp01_$1.elf"
DAM_TARGET_BIN="atel_slp01_$1.bin"
DAM_TARGET_MAP="atel_slp01_$1.map"

if [ ! -d $DAM_OUTPUT_PATH ]; then
    mkdir $DAM_OUTPUT_PATH
fi

if [ ! -d $DEMO_APP_OUTPUT_PATH ]; then
    mkdir $DEMO_APP_OUTPUT_PATH
fi

echo "=== Application RO base selected = $DAM_RO_BASE"

export DAM_CPPFLAGS="-DQAPI_TXM_MODULE -DTXM_MODULE -DTX_DAM_QC_CUSTOMIZATIONS -DTX_ENABLE_PROFILING -DTX_ENABLE_EVENT_TRACE -DTX_DISABLE_NOTIFY_CALLBACKS  -DFX_FILEX_PRESENT -DTX_ENABLE_IRQ_NESTING  -DTX3_CHANGES"
export DAM_CFLAGS="-marm -target armv7m-none-musleabi -mfloat-abi=softfp -mfpu=none -mcpu=cortex-a7 -mno-unaligned-access  -fms-extensions -Osize -fshort-enums -Wbuiltin-macro-redefined"
export DAM_INCPATHS="-I $DAM_INC_BASE -I $DAM_INC_BASE/threadx_api -I $DAM_INC_BASE/qmi -I $DAM_INC_BASE/qapi -I $TOOLCHAIN_PATH_STANDARdS -I $DAM_CPPFLAGS -I $LLVMLIB -I $DEMO_APP_UTILS_INC_PATH -I $DEMO_APP_INC_PATH -I $MODULE_DEPENDS_INC -I $EVENT_IF_INC"
export APP_CFLAGS="-DTARGET_THREADX -DENABLE_IOT_INFO -DENABLE_IOT_DEBUG -DSENSOR_SIMULATE"

#Turn on verbose mode by default
set -x;

echo "=== Compiling Example $1"
echo "== Compiling .S file..."
$TOOLCHAIN_PATH/clang.exe -E  $DAM_CPPFLAGS $DAM_CFLAGS $DEMO_SRC_PATH/txm_module_preamble_llvm.S > txm_module_preamble_llvm_pp.S
$TOOLCHAIN_PATH/clang.exe  -c $DAM_CPPFLAGS $DAM_CFLAGS txm_module_preamble_llvm_pp.S -o $DEMO_APP_OUTPUT_PATH/txm_module_preamble_llvm.o
rm txm_module_preamble_llvm_pp.S

echo "== Compiling .C file..."
$TOOLCHAIN_PATH/clang.exe -c $DAM_CPPFLAGS $BUILD_APP_FLAG $DAM_CFLAGS $DAM_INCPATHS $DEMO_APP_SRC_PATH/*.c $DEMO_APP_UTILS_SRC_PATH/*.c $MODULE_DEPENDS_SRC/*.c $EVENT_IF_SRC/*.c
mv *.o $DEMO_APP_OUTPUT_PATH

echo "=== Linking Example $1 application"
$TOOLCHAIN_PATH/clang++.exe -d -o $DEMO_ELF_OUTPUT_PATH/$DAM_ELF_NAME -target armv7m-none-musleabi -fuse-ld=qcld -lc++ -Wl,-mno-unaligned-access -fuse-baremetal-sysroot -fno-use-baremetal-crt -Wl,-entry=$DAM_RO_BASE $DEMO_APP_OUTPUT_PATH/txm_module_preamble_llvm.o -Wl,-T$DEMO_APP_LD_PATH/quectel_dam_demo.ld -Wl,-Map,-Wl,-gc-sections $DEMO_APP_OUTPUT_PATH/*.o $DAM_LIB_PATH/*.lib
$PYTHON_PATH $LLVMLINK_PATH/llvm-elf-to-hex.py --bin $DEMO_ELF_OUTPUT_PATH/$DAM_ELF_NAME --output $DEMO_ELF_OUTPUT_PATH/$DAM_TARGET_BIN

set +x;

echo "=== application $1 is built at" $DAM_OUTPUT_PATH/$DAM_TARGET_BIN
echo -n "/datatx/atel_slp01_$1.bin" > ./bin/oem_app_path.ini
echo "Done."
