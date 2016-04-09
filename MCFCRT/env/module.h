// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2016, LH_Mouse. All wrongs reserved.

#ifndef __MCFCRT_ENV_MODULE_H_
#define __MCFCRT_ENV_MODULE_H_

#include "_crtdef.h"

__MCFCRT_EXTERN_C_BEGIN

// __MCFCRT_BeginModule() 在所有 CRT 功能都初始化成功后返回 true。
extern bool __MCFCRT_BeginModule(void);
extern void __MCFCRT_EndModule(void);

typedef void (*_MCFCRT_AtEndModuleCallback)(_MCFCRT_STD intptr_t __nContext);

extern bool _MCFCRT_AtEndModule(_MCFCRT_AtEndModuleCallback __pfnProc, _MCFCRT_STD intptr_t __nContext) _MCFCRT_NOEXCEPT;

extern void *_MCFCRT_GetModuleBase(void) _MCFCRT_NOEXCEPT;

typedef struct _MCFCRT_tagModuleSectionInfo {
	const void *__pInternalTable;
	_MCFCRT_STD size_t __uInternalSize;
	_MCFCRT_STD size_t __uNextIndex;

	char __achName[8];
	_MCFCRT_STD size_t __uRawSize;
	void *__pBase;
	_MCFCRT_STD size_t __uSize;
} _MCFCRT_ModuleSectionInfo;

extern bool _MCFCRT_EnumerateFirstModuleSection(_MCFCRT_ModuleSectionInfo *__pInfo) _MCFCRT_NOEXCEPT;
extern bool _MCFCRT_EnumerateNextModuleSection(_MCFCRT_ModuleSectionInfo *__pInfo) _MCFCRT_NOEXCEPT;

__MCFCRT_EXTERN_C_END

#endif
