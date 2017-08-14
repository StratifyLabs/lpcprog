/*

Copyright 2011-2017 Tyler Gilbert

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

 */

#ifndef LPC_DEVICES_H_
#define LPC_DEVICES_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t lpc_device_get_checksum_addr(const char * dev);
uint32_t lpc_device_get_ram_start(const char * dev);
uint32_t lpc_device_get_sector_number(const char * dev, uint32_t addr);

#ifdef __cplusplus
}
#endif

#endif /* LPC_DEVICES_H_ */

