// Common header of the game. The PINT port uses only Win32 and the C run-time
// of Windows NT 4.0; pintstl.h gives the small part of the C++ library that
// the game needs.

#ifndef PCH_H
#define PCH_H

#define _CRT_SECURE_NO_WARNINGS

// The game uses std::min and std::max, not the min and max macros.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>
#include <mmsystem.h>
// mmsystem.h makes PlaySound a macro, and the game has Sound::PlaySound.
#undef PlaySound

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "pintstl.h"

// The headers of Windows NT 4.0 do not have these keys.
#ifndef VK_OEM_PERIOD
#define VK_OEM_PERIOD 0xBE
#endif
#ifndef VK_OEM_2
#define VK_OEM_2 0xBF
#endif

constexpr char PathSeparator = '\\';

/*Use (void) to silent unused warnings.*/
#define assertm(exp, msg) assert(((void)msg, exp))


inline size_t pgm_save(int width, int height, char* data, FILE* outfile)
{
	size_t n = 0;
	n += fprintf(outfile, "P5\n%d %d\n%d\n", width, height, 0xFF);
	n += fwrite(data, 1, width * height, outfile);
	return n;
}

inline float RandFloat()
{
	return static_cast<float>(std::rand() / static_cast<double>(RAND_MAX));
}

template <typename T>
constexpr int Sign(T val)
{
	return (T(0) < val) - (val < T(0));
}

template <typename T>
const T& Clamp(const T& n, const T& lower, const T& upper)
{
	return std::max(lower, std::min(n, upper));
}

// File names are in the ANSI code page, as Windows NT 4.0 gives them.
inline FILE* fopenu(const char* path, const char* opt)
{
	return fopen(path, opt);
}

constexpr float Pi = 3.14159265358979323846f;

#endif //PCH_H
