#define ENABLE_PROFILING

#ifndef PROFILING_UTILS_HPP
#define PROFILING_UTILS_HPP

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <iomanip>

#ifdef ENABLE_PROFILING
    #define TIMERSTART(label)											                \
        auto a##label = std::chrono::steady_clock::now();					            \
        auto b##label = a##label;												        \
        double elapsed_##label = 0.0;


    #define TIMERSTOP(label)												            \
        b##label = std::chrono::steady_clock::now();						            \
        elapsed_##label = std::chrono::duration<double, std::milli>(b##label - a##label).count();   \
        std::cerr << std::fixed << std::setprecision(4)                                 \
                << "# elapsed time (" << #label << "): "					            \
                << elapsed_##label << " s\n";
                                                         
#else
    // Empty macros for disabled profiling. 
    // They expand to nothing, causing ZERO overhead in the final binary.
    #define TIMERSTART(label)      do {} while(0)
    #define TIMERSTOP(label)       do {} while(0)
#endif
#endif