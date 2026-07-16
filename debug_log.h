#pragma once

#include <stdio.h>

#ifndef RVSWD_DEBUG_LOG
#define RVSWD_DEBUG_LOG 0
#endif

#if RVSWD_DEBUG_LOG
#define RVSWD_LOG(...) printf(__VA_ARGS__)
#else
#define RVSWD_LOG(...) do { } while (0)
#endif
