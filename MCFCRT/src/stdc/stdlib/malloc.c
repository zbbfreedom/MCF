// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2017, LH_Mouse. All wrongs reserved.

#include "../../env/_crtdef.h"
#include "../../env/heap.h"

#undef malloc

__attribute__((__noinline__))
void *malloc(size_t cb){
	return _MCFCRT_malloc(cb);
}
