// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2015, LH_Mouse. All wrongs reserved.

#include "thread.h"
#include "mcfwin.h"
#include "fenv.h"
#include "avl_tree.h"
#include "bail.h"
#include "mingw_hacks.h"
#include "eh_top.h"
#include "../ext/assert.h"
#include "../ext/unref_param.h"
#include <stdlib.h>
#include <winternl.h>
#include <ntdef.h>

extern __attribute__((__dllimport__, __stdcall__))
NTSTATUS NtSuspendThread(HANDLE hThread, LONG *plPrevCount);
extern __attribute__((__dllimport__, __stdcall__))
NTSTATUS NtResumeThread(HANDLE hThread, LONG *plPrevCount);

typedef struct tagTlsObject {
	MCF_AvlNodeHeader vHeader;

	intptr_t nValue;

	struct tagThreadMap *pMap;
	struct tagTlsObject *pPrevByThread;
	struct tagTlsObject *pNextByThread;

	struct tagTlsKey *pKey;
	struct tagTlsObject *pPrevByKey;
	struct tagTlsObject *pNextByKey;
} TlsObject;

static int ObjectComparatorNodeKey(const MCF_AvlNodeHeader *pObj1, intptr_t nKey2){
	const uintptr_t uKey1 = (uintptr_t)(((const TlsObject *)pObj1)->pKey);
	const uintptr_t uKey2 = (uintptr_t)(void *)nKey2;
	return (uKey1 < uKey2) ? -1 : ((uKey1 > uKey2) ? 1 : 0);
}
static int ObjectComparatorNodes(const MCF_AvlNodeHeader *pObj1, const MCF_AvlNodeHeader *pObj2){
	return ObjectComparatorNodeKey(pObj1, (intptr_t)(void *)(((const TlsObject *)pObj2)->pKey));
}

typedef struct tagThreadMap {
	SRWLOCK srwLock;
	MCF_AvlRoot pavlObjects;
	struct tagTlsObject *pLastByThread;
} ThreadMap;

typedef struct tagTlsKey {
	MCF_AvlNodeHeader vHeader;

	SRWLOCK srwLock;
	void (*pfnCallback)(intptr_t);
	struct tagTlsObject *pLastByKey;
} TlsKey;

static int KeyComparatorNodeKey(const MCF_AvlNodeHeader *pObj1, intptr_t nKey2){
	const uintptr_t uKey1 = (uintptr_t)(void *)pObj1;
	const uintptr_t uKey2 = (uintptr_t)(void *)nKey2;
	return (uKey1 < uKey2) ? -1 : ((uKey1 > uKey2) ? 1 : 0);
}
static int KeyComparatorNodes(const MCF_AvlNodeHeader *pObj1, const MCF_AvlNodeHeader *pObj2){
	return KeyComparatorNodeKey(pObj1, (intptr_t)(void *)pObj2);
}

static SRWLOCK     g_csKeyMutex = SRWLOCK_INIT;
static DWORD       g_dwTlsIndex = TLS_OUT_OF_INDEXES;
static MCF_AvlRoot g_pavlKeys   = nullptr;

bool __MCF_CRT_ThreadEnvInit(){
	g_dwTlsIndex = TlsAlloc();
	if(g_dwTlsIndex == TLS_OUT_OF_INDEXES){
		return false;
	}
	return true;
}
void __MCF_CRT_ThreadEnvUninit(){
	if(g_pavlKeys){
		MCF_AvlNodeHeader *const pRoot = g_pavlKeys;
		g_pavlKeys = nullptr;

		TlsKey *pKey;
		MCF_AvlNodeHeader *pCur = MCF_AvlPrev(pRoot);
		while(pCur){
			pKey = (TlsKey *)pCur;
			pCur = MCF_AvlPrev(pCur);
			free(pKey);
		}
		pCur = MCF_AvlNext(pRoot);
		while(pCur){
			pKey = (TlsKey *)pCur;
			pCur = MCF_AvlNext(pCur);
			free(pKey);
		}
		pKey = (TlsKey *)pRoot;
		free(pKey);
	}

	TlsFree(g_dwTlsIndex);
	g_dwTlsIndex = TLS_OUT_OF_INDEXES;
}

void __MCF_CRT_TlsThreadCleanup(){
	ThreadMap *const pMap = TlsGetValue(g_dwTlsIndex);
	if(pMap){
		TlsObject *pObject = pMap->pLastByThread;
		while(pObject){
			TlsKey *const pKey = pObject->pKey;

			AcquireSRWLockExclusive(&(pKey->srwLock));
			{
				if(pKey->pLastByKey == pObject){
					pKey->pLastByKey = pObject->pPrevByKey;
				}
			}
			ReleaseSRWLockExclusive(&(pKey->srwLock));

			if(pKey->pfnCallback){
				(*pKey->pfnCallback)(pObject->nValue);
			}

			TlsObject *const pTemp = pObject->pPrevByThread;
			free(pObject);
			pObject = pTemp;
		}
		free(pMap);
		TlsSetValue(g_dwTlsIndex, nullptr);
	}

	__MCF_CRT_RunEmutlsDtors();
}

void *MCF_CRT_TlsAllocKey(void (*pfnCallback)(intptr_t)){
	TlsKey *const pKey = malloc(sizeof(TlsKey));
	if(!pKey){
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return nullptr;
	}
	InitializeSRWLock(&(pKey->srwLock));
	pKey->pfnCallback = pfnCallback;
	pKey->pLastByKey  = nullptr;

	AcquireSRWLockExclusive(&g_csKeyMutex);
	{
		MCF_AvlAttach(&g_pavlKeys, (MCF_AvlNodeHeader *)pKey, &KeyComparatorNodes);
	}
	ReleaseSRWLockExclusive(&g_csKeyMutex);

	return pKey;
}
bool MCF_CRT_TlsFreeKey(void *pTlsKey){
	TlsKey *const pKey = pTlsKey;
	if(!pKey){
		SetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}

	AcquireSRWLockExclusive(&g_csKeyMutex);
	{
		MCF_AvlDetach((MCF_AvlNodeHeader *)pKey);
	}
	ReleaseSRWLockExclusive(&g_csKeyMutex);

	TlsObject *pObject = pKey->pLastByKey;
	while(pObject){
		ThreadMap *const pMap = pObject->pMap;

		AcquireSRWLockExclusive(&(pMap->srwLock));
		{
			TlsObject *const pPrev = pObject->pPrevByThread;
			TlsObject *const pNext = pObject->pNextByThread;
			if(pPrev){
				pPrev->pNextByThread = pNext;
			}
			if(pNext){
				pNext->pPrevByThread = pPrev;
			}

			if(pMap->pLastByThread == pObject){
				pMap->pLastByThread = pObject->pPrevByThread;
			}
		}
		ReleaseSRWLockExclusive(&(pMap->srwLock));

		if(pKey->pfnCallback){
			(*pKey->pfnCallback)(pObject->nValue);
		}

		TlsObject *const pTemp = pObject->pPrevByKey;
		free(pObject);
		pObject = pTemp;
	}
	free(pKey);

	return true;
}

void (*MCF_CRT_TlsGetCallback(void *pTlsKey))(intptr_t){
	TlsKey *const pKey = pTlsKey;
	if(!pKey){
		SetLastError(ERROR_INVALID_PARAMETER);
		return nullptr;
	}
	SetLastError(ERROR_SUCCESS);
	return pKey->pfnCallback;
}
bool MCF_CRT_TlsGet(void *pTlsKey, bool *restrict pbHasValue, intptr_t *restrict pnValue){
	TlsKey *const pKey = pTlsKey;
	if(!pKey){
		SetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}

	*pbHasValue = false;

	ThreadMap *const pMap = TlsGetValue(g_dwTlsIndex);
	if(!pMap){
		return true;
	}

	AcquireSRWLockExclusive(&(pMap->srwLock));
	{
		TlsObject *const pObject = (TlsObject *)MCF_AvlFind(
			&(pMap->pavlObjects), (intptr_t)pKey, &ObjectComparatorNodeKey);
		if(pObject){
			*pbHasValue = true;
			*pnValue = pObject->nValue;
		}
	}
	ReleaseSRWLockExclusive(&(pMap->srwLock));

	return true;
}
bool MCF_CRT_TlsReset(void *pTlsKey, intptr_t nNewValue){
	TlsKey *const pKey = pTlsKey;
	if(!pKey){
		SetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}

	bool bHasOldValue;
	intptr_t nOldValue;
	if(!MCF_CRT_TlsExchange(pTlsKey, &bHasOldValue, &nOldValue, nNewValue)){
		if(pKey->pfnCallback){
			const DWORD dwErrorCode = GetLastError();
			(*pKey->pfnCallback)(nNewValue);
			SetLastError(dwErrorCode);
		}
		return false;
	}
	if(bHasOldValue){
		if(pKey->pfnCallback){
			(*pKey->pfnCallback)(nOldValue);
		}
	}
	return true;
}
bool MCF_CRT_TlsExchange(void *pTlsKey, bool *restrict pbHasOldValue, intptr_t *restrict pnOldValue, intptr_t nNewValue){
	TlsKey *const pKey = pTlsKey;
	if(!pKey){
		SetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}

	*pbHasOldValue = false;

	ThreadMap *pMap = TlsGetValue(g_dwTlsIndex);
	if(!pMap){
		pMap = malloc(sizeof(ThreadMap));
		if(!pMap){
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			return false;
		}
		InitializeSRWLock(&(pMap->srwLock));
		pMap->pavlObjects   = nullptr;
		pMap->pLastByThread = nullptr;

		TlsSetValue(g_dwTlsIndex, pMap);
	}

	AcquireSRWLockExclusive(&(pMap->srwLock));
	{
		TlsObject *const pObject = (TlsObject *)MCF_AvlFind(
			&(pMap->pavlObjects), (intptr_t)pKey, &ObjectComparatorNodeKey);
		if(pObject){
			*pbHasOldValue = true;
			*pnOldValue = pObject->nValue;
			pObject->nValue = nNewValue;
		}
	}
	ReleaseSRWLockExclusive(&(pMap->srwLock));

	if(!*pbHasOldValue){
		TlsObject *const pObject = malloc(sizeof(TlsObject));
		if(!pObject){
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			return false;
		}
		pObject->nValue = nNewValue;
		pObject->pMap = pMap;
		pObject->pKey = pKey;

		AcquireSRWLockExclusive(&(pMap->srwLock));
		{
			TlsObject *const pPrev = pMap->pLastByThread;
			pMap->pLastByThread = pObject;

			pObject->pPrevByThread = pPrev;
			pObject->pNextByThread = nullptr;
			if(pPrev){
				pPrev->pNextByThread = pObject;
			}
			MCF_AvlAttach(&(pMap->pavlObjects), (MCF_AvlNodeHeader *)pObject, &ObjectComparatorNodes);
		}
		ReleaseSRWLockExclusive(&(pMap->srwLock));

		AcquireSRWLockExclusive(&(pKey->srwLock));
		{
			TlsObject *const pPrev = pKey->pLastByKey;
			pKey->pLastByKey = pObject;

			pObject->pPrevByKey = pPrev;
			pObject->pNextByKey = nullptr;
			if(pPrev){
				pPrev->pNextByKey = pObject;
			}
		}
		ReleaseSRWLockExclusive(&(pKey->srwLock));
	}

	return true;
}

int MCF_CRT_AtEndThread(void (*pfnProc)(intptr_t), intptr_t nContext){
	void *const pKey = MCF_CRT_TlsAllocKey(pfnProc);
	if(!pKey){
		return -1;
	}
	if(!MCF_CRT_TlsReset(pKey, nContext)){
		const DWORD dwLastError = GetLastError();
		MCF_CRT_TlsFreeKey(pKey);
		SetLastError(dwLastError);
		return -1;
	}
	return 0;
}

typedef struct tagThreadInitInfo {
	unsigned (*pfnProc)(intptr_t);
	intptr_t nParam;
} ThreadInitInfo;

static __MCF_C_STDCALL __MCF_HAS_EH_TOP
DWORD CRTThreadProc(LPVOID pParam){
	DWORD dwExitCode;
	__MCF_EH_TOP_BEGIN
	{
		const ThreadInitInfo vInitInfo = *(ThreadInitInfo *)pParam;
		free(pParam);

		__MCF_CRT_FEnvInit();

		dwExitCode = (*vInitInfo.pfnProc)(vInitInfo.nParam);
	}
	__MCF_EH_TOP_END
	return dwExitCode;
}

void *MCF_CRT_CreateThread(unsigned (*pfnThreadProc)(intptr_t), intptr_t nParam, bool bSuspended, unsigned long *restrict pulThreadId){
	ThreadInitInfo *const pInitInfo = malloc(sizeof(ThreadInitInfo));
	if(!pInitInfo){
		return nullptr;
	}
	pInitInfo->pfnProc  = pfnThreadProc;
	pInitInfo->nParam   = nParam;

	const HANDLE hThread = CreateThread(nullptr, 0, &CRTThreadProc, pInitInfo, CREATE_SUSPENDED, pulThreadId);
	if(!hThread){
		const DWORD dwErrorCode = GetLastError();
		free(pInitInfo);
		SetLastError(dwErrorCode);
		return nullptr;
	}

	if(!bSuspended){
		ResumeThread(hThread);
	}
	return (void *)hThread;
}
void MCF_CRT_CloseThread(void *hThread){
	if(!CloseHandle((HANDLE)hThread)){
		ASSERT_MSG(false, L"CloseHandle() 失败。");
	}
}
unsigned long MCF_CRT_GetCurrentThreadId(){
	return GetCurrentThreadId();
}

long MCF_CRT_SuspendThread(void *hThread){
	LONG lPrevCount;
	const NTSTATUS lStatus = NtSuspendThread((HANDLE)hThread, &lPrevCount);
	if(!NT_SUCCESS(lStatus)){
		ASSERT_MSG(false, L"NtSuspendThread() 失败。");
	}
	return lPrevCount;
}
long MCF_CRT_ResumeThread(void *hThread){
	LONG lPrevCount;
	const NTSTATUS lStatus = NtResumeThread((HANDLE)hThread, &lPrevCount);
	if(!NT_SUCCESS(lStatus)){
		ASSERT_MSG(false, L"NtResumeThread() 失败。");
	}
	return lPrevCount;
}

bool MCF_CRT_WaitForThread(void *hThread, MCF_STD uint64_t u64MilliSeconds){
	if(u64MilliSeconds > (uint64_t)INT64_MIN / 10000){
		MCF_CRT_WaitForThreadInfinitely(hThread);
		return true;
	}

	LARGE_INTEGER liTimeout;
	liTimeout.QuadPart = -(int64_t)(u64MilliSeconds * 10000);
	const NTSTATUS lStatus = NtWaitForSingleObject((HANDLE)hThread, false, &liTimeout);
	if(!NT_SUCCESS(lStatus)){
		ASSERT_MSG(false, L"NtWaitForSingleObject() 失败。");
	}
	if(lStatus == STATUS_TIMEOUT){
		return false;
	}
	return true;
}
void MCF_CRT_WaitForThreadInfinitely(void *hThread){
	const NTSTATUS lStatus = NtWaitForSingleObject((HANDLE)hThread, false, nullptr);
	if(!NT_SUCCESS(lStatus)){
		ASSERT_MSG(false, L"NtWaitForSingleObject() 失败。");
	}
}
