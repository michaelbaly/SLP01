#------------------------------------------------------------
# (C) Copyright [2006-2008] Marvell International Ltd.
# All Rights Reserved
#------------------------------------------------------------

#=========================================================================
# File Name      : SPI_NOR.mak
# Description    : Main make file for the hal/UART package.
#
# Usage          : make [-s] -f SPI_NOR_dep.mak OPT_FILE=<path>/<opt_file>
#
# Notes          : The options file defines macro values defined
#                  by the environment, target, and groups. It
#                  must be included for proper package building.
#
# Copyright (c) 2002 Intel Corporation. All Rights Reserved
#=========================================================================

# Package build options
include ${OPT_FILE}

# Package Makefile information
GEN_PACK_MAKEFILE = ${BUILD_ROOT}/env/${HOST}/build/package.mak

# Define Package ---------------------------------------

PACKAGE_NAME     = SPI_NOR
PACKAGE_BASE     = hal
PACKAGE_DEP_FILE = SPI_NOR_dep.mak
PACKAGE_PATH     = $(BUILD_ROOT)/$(PACKAGE_BASE)/$(PACKAGE_NAME)

# DEFAULT VARIANT

# The relative path locations of source and include file directories.
PACKAGE_SRC_PATH    = $(PACKAGE_PATH)/src
PACKAGE_INC_PATHS   = $(PACKAGE_PATH)/src $(PACKAGE_PATH)/inc  \
                        ${BUILD_ROOT}\hop\pm\inc      \
                        ${BUILD_ROOT}\os\nu_xscale\inc \
                        ${BUILD_ROOT}\os\osa\inc \
                        ${BUILD_ROOT}/diag/diag_logic/inc \
                        ${BUILD_ROOT}/softutil/wkfatsys/in \
                        ${BUILD_ROOT}/diag/diag_logic/src \
                        ${BUILD_ROOT}/diag/diag_logic/src

# Package source files, paths not required
PACKAGE_SRC_FILES = spi \
					giga.c \
					macronix.c

#*******************************ARBEL******************************
ifneq (,$(findstring ARBEL,$(VARIANT_LIST)))

# this is the drivers that not yet unify. after unify can be erased
PACKAGE_INC_PATHS   +=  ${BUILD_ROOT}\hop\intc\inc     \
                        ${BUILD_ROOT}\hop\gpio\inc     \
                        ${BUILD_ROOT}\hop\dma\inc      \
                        ${BUILD_ROOT}\hop\core\inc

# handle the DVT_TEST_ENABLE variant ------------
ifneq ($(DVT_TEST_ENABLE),)
 PACKAGE_SRC_PATH   += $(PACKAGE_PATH)/test

 PACKAGE_INC_PATHS  += ${BUILD_ROOT}\hal\dvt\inc
endif

endif
#*******************************END ARBEL**************************

#*******************************BOERNE*****************************
ifneq (,$(findstring BOERNE,$(TARGET_VARIANT)))

PACKAGE_INC_PATHS   +=  ${BUILD_ROOT}\hal\intc\inc     \
                        ${BUILD_ROOT}\hal\gpio\inc     \
                        ${BUILD_ROOT}\hal\dma\inc      \
                        ${BUILD_ROOT}\hal\core\inc


# handle the DVT_TEST_ENABLE variant ------------
ifneq ($(DVT_TEST_ENABLE),)
 PACKAGE_SRC_FILES  += uart_test_dvt.c
 PACKAGE_SRC_PATH   += $(PACKAGE_PATH)/test
 PACKAGE_INC_PATHS  += ${BUILD_ROOT}\hal\dvt\inc
endif

endif
#*******************************END BOERNE**************************

ifneq (,$(findstring UARTDEBUG,$(TARGET_VARIANT)))
PACKAGE_DFLAGS += -DUARTDEBUG
endif


# Include the Standard Package Make File ---------------
include ${GEN_PACK_MAKEFILE}

# Include the Make Dependency File ---------------------
# This must be the last line in the file
include ${PACKAGE_DEP_FILE}
