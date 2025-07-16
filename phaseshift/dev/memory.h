// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_DEV_MEMORY_H_
#define PHASESHIFT_DEV_MEMORY_H_

#include "stdlib.h"
#include "stdio.h"
#include "string.h"


namespace phaseshift {
namespace dev {

inline int mem_usage_parse_line(char* line){
    // This assumes that a digit will be found and the line ends in " Kb".
    int i = strlen(line);
    const char* p = line;
    while (*p <'0' || *p > '9') p++;
    line[i-3] = '\0';
    i = atoi(p);
    return i;
}
// https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
//! [kB]
inline int mem_usage() { //Note: this value is in KB!
    #if defined(_WIN32) || defined(_WIN64)
        return -1;
    #elif defined(__MACH__) || defined(__APPLE__)
        return -1;
    #else  // Linux
        FILE* file = fopen("/proc/self/status", "r");
        int result = -1;
        char line[128];

        while (fgets(line, 128, file) != NULL){
            if (strncmp(line, "VmSize:", 7) == 0){
                result = mem_usage_parse_line(line);
                break;
            }
        }
        fclose(file);
        return result;
    #endif
}

}
}

#endif  // PHASESHIFT_DEV_MEMORY_H_
