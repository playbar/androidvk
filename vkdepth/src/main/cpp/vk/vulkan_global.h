#ifndef __VULKAN_GLOBAL_H__
#define __VULKAN_GLOBAL_H__

#include <vector>

#include <vulkan_wrapper.h>
#include "vulkan_device.h"

#define MAX_BUFFER_SIZE 8192
#define OFFSET_VALUE 0

#ifdef __cplusplus
extern "C"{
#endif

void StrToHex(unsigned char *pbDest, unsigned char *pbSrc, int nLen);

#ifdef __cplusplus
}
#endif

#endif
