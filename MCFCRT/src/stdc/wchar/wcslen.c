// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2017, LH_Mouse. All wrongs reserved.

#include "../../env/_crtdef.h"
#include "../../ext/rawwmemchr.h"

#undef wcslen

size_t wcslen(const wchar_t *s){
	const wchar_t *const p = _MCFCRT_rawwmemchr(s, 0);
	return (size_t)(p - s);
}
