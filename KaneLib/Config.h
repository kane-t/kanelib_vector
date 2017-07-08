// Preprocessor defines and other configuration values for KaneLib
// I'm just sticking K or KANELIB in front of all of these, because I need to put SOMETHING there,
// and KANELIB sounds better than BLAR.
#pragma once

// By default, we use both __forceinline and inline.  For compilers which don't support 
// __forceinline, or for testing purposes, KFINLINE and KINLINE can be replaced.  
#ifndef KFINLINE
#define KFINLINE __forceinline
#endif
#ifndef KINLINE
#define KINLINE inline
#endif
#ifndef KNOINLINE
#define KNOINLINE __declspec(noinline)
#endif

// Windows.h is so horrible, so horrible
#define NOMINMAX