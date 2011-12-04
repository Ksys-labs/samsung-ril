# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    call.c \
    samsung-ril.c \
    misc.c \
    net.c \
    sat.c \
    sim.c \
    sms.c \
    util.c \
    pwr.c \
    disp.c \
    socket.c

LOCAL_SHARED_LIBRARIES := \
    libcutils libutils libril

LOCAL_STATIC_LIBRARIES := libsamsung-ipc

# for asprinf
LOCAL_CFLAGS := -D_GNU_SOURCE

ifeq ($(TARGET_DEVICE),crespo)
	LOCAL_CFLAGS += -DDEVICE_CRESPO
endif
ifeq ($(TARGET_DEVICE),h1)
	LOCAL_CFLAGS += -DDEVICE_H1
endif

LOCAL_C_INCLUDES := external/libsamsung-ipc/include
LOCAL_C_INCLUDES += hardware/ril/libsamsung-ipc/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

LOCAL_MODULE_TAGS := optional

LOCAL_PRELINK_MODULE := false

ifeq (foo,foo)
  #build shared library
  LOCAL_SHARED_LIBRARIES += \
      libcutils libutils
  LOCAL_LDLIBS += -lpthread
  LOCAL_CFLAGS += -DRIL_SHLIB -DDEVICE_H1
  LOCAL_MODULE:= libsamsung-ril
  include $(BUILD_SHARED_LIBRARY)
else
  #build executable
  LOCAL_SHARED_LIBRARIES += \
      libril
  LOCAL_MODULE:= samsung-ril
  include $(BUILD_EXECUTABLE)
endif
