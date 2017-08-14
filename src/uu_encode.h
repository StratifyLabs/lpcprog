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
#ifndef UU_ENCODE_H_
#define UU_ENCODE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


char uu_encode_line(char * dest_uu, void * src, uint8_t bytes);
char uu_decode_line(void * dest, char * src_uu, uint8_t bytes);

#ifdef __cplusplus
}
#endif

#endif /* UU_ENCODE_H_ */
