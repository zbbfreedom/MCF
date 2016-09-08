// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2016, LH_Mouse. All wrongs reserved.

#include "../../env/_crtdef.h"
#include "_fpu.h"

#undef fmaf
#undef fma
#undef fmal

// https://en.wikipedia.org/wiki/Extended_precision#x86_Extended_Precision_Format
typedef union x87reg_ {
	long double f;
	struct __attribute__((__packed__)) {
		uint64_t f64 : 64;
		uint16_t exp : 15;
		uint16_t sgn :  1;
	};
	struct __attribute__((__packed__)) {
		uint32_t flo : 32;
		uint32_t fhi : 32;
//		uint16_t exp : 15;
//		uint16_t sgn :  1;
	};
} x87reg;

static inline void break_down(x87reg *restrict lo, x87reg *restrict hi, long double x){
	hi->f = x;
	const uint32_t flo = hi->flo;
	const uint32_t exp = hi->exp;
	const bool     sgn = hi->sgn;
	hi->flo = 0;

	if(flo == 0){
		lo->f64 = 0;
		lo->exp = 0;
	} else {
		static_assert(sizeof(unsigned long) * CHAR_BIT == 32, "Please fix this!");
		const unsigned shn = (unsigned)__builtin_clzl(flo) + 32;
		if(shn < exp){
			lo->f64 = (uint64_t)flo << shn;
			lo->exp = (exp - shn) & 0x7FFF;
		} else {
			lo->f64 = (uint64_t)flo << exp;
			lo->exp = 0;
		}
	}
	lo->sgn = sgn;
}
static inline long double fpu_fma(long double x, long double y, long double z){
	x87reg xlo, xhi, ylo, yhi;
	break_down(&xlo, &xhi, x);
	break_down(&ylo, &yhi, y);
	long double ret = z;
	ret += xhi.f * yhi.f;
	ret += xhi.f * ylo.f + xlo.f * yhi.f;
	ret += xlo.f * ylo.f;
	return ret;
}

float fmaf(float x, float y, float z){
	// return (float)fpu_fma(x, y, z);
	return (float)((double)x * (double)y + (double)z);
}
double fma(double x, double y, double z){
	return (double)fpu_fma(x, y, z);
}
long double fmal(long double x, long double y, long double z){
	return fpu_fma(x, y, z);
}
