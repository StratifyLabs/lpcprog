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


#ifndef ISPLIB_H_
#define ISPLIB_H_

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_LEVEL_MAX 100
#define DEBUG_ERROR

#if (DEBUG_LEVEL_MAX > 0) && defined __link
#include <stdio.h>
#define isplib_debug(LEVEL, ...) do { if ( LEVEL <= DEBUG_LEVEL_MAX){ printf("%s: ", __func__); printf(__VA_ARGS__); fflush(stdout); }} while(0)
#define isplib_error(...) do { printf("ERROR:%s: ", __func__); printf(__VA_ARGS__); fflush(stdout); } while(0)
#else
#define isplib_debug(LEVEL, ...)
#endif

#if defined DEBUG_ERROR && defined __link
#define isplib_error(...) do { printf("ERROR:%s: ", __func__); printf(__VA_ARGS__); fflush(stdout); } while(0)
#else
#define isplib_error(...)
#endif


#ifdef __cplusplus
}
#endif

#endif /* ISPLIB_H_ */

