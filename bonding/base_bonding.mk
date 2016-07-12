PRJ_TARGET = base_bonding
PRJ_TARGET_TYPE = exe

ifndef PRJ_DEBUG
PRJ_DEBUG = yes
endif

MK_DIR ?= $(PWD)


PRJ_SRC = base_bonding.c 

include ${MK_DIR}/std.mk