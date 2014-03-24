// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2014. LH_Mouse. All wrongs reserved.

#include "../StdMCF.hpp"
#include "HttpClient.hpp"
#include "../../MCFCRT/c/ext/assert.h"
#include "../../MCFCRT/cpp/ext/multi_indexed_map.hpp"
#include "../../MCFCRT/cpp/ext/vvector.hpp"
#include "../Core/UniqueHandle.hpp"
#include "../Core/Exception.hpp"
#include "../Core/Utilities.hpp"
#include <winhttp.h>
using namespace MCF;

// 嵌套类定义。
class HttpClient::xDelegate : NO_COPY {
private:
	// RAII
	struct WinHttpCloser {
		constexpr HINTERNET operator()() const noexcept {
			return NULL;
		}
		void operator()(HINTERNET hInternet) const noexcept {
			::WinHttpCloseHandle(hInternet);
		}
	};
	typedef UniqueHandle<WinHttpCloser> xWinHttpHandle;

	struct GlobalFreer {
		constexpr void *operator()() const noexcept {
			return nullptr;
		}
		void operator()(void *pGlobal) const noexcept {
			::GlobalFree((HGLOBAL)pGlobal);
		}
	};
	typedef UniqueHandle<GlobalFreer> xPGlobal;

	// Cookies
/*	struct CookieItem {
		Utf16String wcsValue;
		bool bSecure;
		bool bHttpOnly;
	};



	typedef MultiIndexedMap<
		CookieItem,

	> CookieMap;
*/
private:
	Utf16String xm_ucsPacUrl;
	Utf16String xm_ucsProxyUserName;
	Utf16String xm_ucsProxyPassword;
	xWinHttpHandle xm_hSession;

	xWinHttpHandle xm_hConnect;
	xWinHttpHandle xm_hRequest;

public:
	xDelegate(bool bAutoProxy, const wchar_t *pwchProxy, std::size_t uProxyLen, const wchar_t *pwszUserAgent){
		if(!pwchProxy){
			pwchProxy = L"";
			uProxyLen = 0;
		} else if(uProxyLen == (std::size_t)-1){
			uProxyLen = StrLen(pwchProxy);
		}

		Utf16String ucsNamedProxy;
		Utf16String ucsNamedProxyBypass;

		if(uProxyLen > 0){
			if(bAutoProxy){
				// 使用指定的 PAC（指定 URL）
				xm_ucsPacUrl = pwchProxy;
			} else {
				// 使用指定的代理服务器（user:pass@addr:port|bypass）
				const auto pwchBypassBegin = std::wmemchr(pwchProxy, L'|', uProxyLen);
				const auto uProxyUrlLen = pwchBypassBegin ? (std::size_t)(pwchBypassBegin - pwchProxy) : uProxyLen;

				URL_COMPONENTS vUrlComponents;
				vUrlComponents.dwStructSize			= sizeof(vUrlComponents);
				vUrlComponents.lpszScheme			= nullptr;
				vUrlComponents.dwSchemeLength		= 0;
				vUrlComponents.lpszHostName			= nullptr;
				vUrlComponents.dwHostNameLength		= 1;
				vUrlComponents.lpszUserName			= nullptr;
				vUrlComponents.dwUserNameLength		= 1;
				vUrlComponents.lpszPassword			= nullptr;
				vUrlComponents.dwPasswordLength		= 1;
				vUrlComponents.lpszUrlPath			= nullptr;
				vUrlComponents.dwUrlPathLength		= 0;
				vUrlComponents.lpszExtraInfo		= nullptr;
				vUrlComponents.dwExtraInfoLength	= 0;
				if(!::WinHttpCrackUrl(pwchProxy, uProxyUrlLen, 0, &vUrlComponents)){
					MCF_THROW(::GetLastError(), L"::WinHttpCrackUrl() 失败。");
				}

				if(vUrlComponents.lpszUserName){
					xm_ucsProxyUserName.Assign(vUrlComponents.lpszUserName, vUrlComponents.dwUserNameLength);
					vUrlComponents.lpszUserName = nullptr;
					vUrlComponents.dwUserNameLength = 0;
				}
				if(vUrlComponents.lpszPassword){
					xm_ucsProxyPassword.Assign(vUrlComponents.lpszPassword, vUrlComponents.dwPasswordLength);
					vUrlComponents.lpszPassword = nullptr;
					vUrlComponents.dwPasswordLength = 0;
				}

				DWORD dwUrlLength = 0;
				::WinHttpCreateUrl(&vUrlComponents, 0, nullptr, &dwUrlLength);
				const auto ulErrorCode = ::GetLastError();
				if(ulErrorCode != ERROR_INSUFFICIENT_BUFFER){
					MCF_THROW(::GetLastError(), L"::WinHttpCreateUrl() 失败。");
				}
				ucsNamedProxy.Resize(dwUrlLength);
				if(!::WinHttpCreateUrl(&vUrlComponents, 0, ucsNamedProxy.GetCStr(), &dwUrlLength)){
					MCF_THROW(::GetLastError(), L"::WinHttpCreateUrl() 失败。");
				}
				ucsNamedProxy.Resize(dwUrlLength);

				if(pwchBypassBegin){
					ucsNamedProxyBypass.Assign(pwchBypassBegin + 1, pwchProxy + uProxyLen - pwchBypassBegin);
				}
			}
		} else if(bAutoProxy){
			// 使用 IE 的代理服务器
			WINHTTP_CURRENT_USER_IE_PROXY_CONFIG vIEProxyConfig;
			if(::WinHttpGetIEProxyConfigForCurrentUser(&vIEProxyConfig)){
				const xPGlobal pAutoConfigUrl	(vIEProxyConfig.lpszAutoConfigUrl);
				const xPGlobal pProxy			(vIEProxyConfig.lpszProxy);
				const xPGlobal pProxyBypass		(vIEProxyConfig.lpszProxyBypass);

				if(vIEProxyConfig.fAutoDetect){
					xm_ucsPacUrl = vIEProxyConfig.lpszAutoConfigUrl;
				} else {
					ucsNamedProxy = vIEProxyConfig.lpszProxy;
					ucsNamedProxyBypass = vIEProxyConfig.lpszProxyBypass;
				}
			}
		}

		if(ucsNamedProxy.IsEmpty()){
			xm_hSession.Reset(::WinHttpOpen(
				pwszUserAgent,
				WINHTTP_ACCESS_TYPE_NO_PROXY,
				WINHTTP_NO_PROXY_NAME,
				WINHTTP_NO_PROXY_BYPASS,
				0
			));
		} else {
			xm_hSession.Reset(::WinHttpOpen(
				pwszUserAgent,
				WINHTTP_ACCESS_TYPE_NAMED_PROXY,
				ucsNamedProxy.GetCStr(),
				ucsNamedProxyBypass.GetCStr(),
				0
			));
		}
		if(!xm_hSession){
			MCF_THROW(::GetLastError(), L"::WinHttpOpen() 失败。");
		}
	}

public:
	void Connect(
		const wchar_t *pwszVerb,
		const wchar_t *pwchUrl,
		std::size_t uUrlLen,
		const void *pContents,
		std::size_t uContentSize,
		const wchar_t *pwszContentType
	){
		Disconnect();

		if(uUrlLen == (std::size_t)-1){
			uUrlLen = StrLen(pwchUrl);
		}

		URL_COMPONENTS vUrlComponents;
		vUrlComponents.dwStructSize			= sizeof(vUrlComponents);
		vUrlComponents.lpszScheme			= nullptr;
		vUrlComponents.dwSchemeLength		= 1;
		vUrlComponents.lpszHostName			= nullptr;
		vUrlComponents.dwHostNameLength		= 1;
		vUrlComponents.lpszUserName			= nullptr;
		vUrlComponents.dwUserNameLength		= 1;
		vUrlComponents.lpszPassword			= nullptr;
		vUrlComponents.dwPasswordLength		= 1;
		vUrlComponents.lpszUrlPath			= nullptr;
		vUrlComponents.dwUrlPathLength		= 1;
		vUrlComponents.lpszExtraInfo		= nullptr;
		vUrlComponents.dwExtraInfoLength	= 1;
		if(!::WinHttpCrackUrl(pwchUrl, uUrlLen, 0, &vUrlComponents)){
			MCF_THROW(::GetLastError(), L"::WinHttpCrackUrl() 失败。");
		}

		Utf16String wcsHostName(vUrlComponents.lpszHostName, vUrlComponents.dwHostNameLength);
		Utf16String wcsPathExtra;
		wcsPathExtra.Resize(vUrlComponents.dwUrlPathLength + vUrlComponents.dwExtraInfoLength);
		if(wcsPathExtra.IsEmpty()){
			wcsPathExtra.Assign(L'/');
		} else {
			std::copy_n(
				vUrlComponents.lpszExtraInfo,
				vUrlComponents.dwExtraInfoLength,
				std::copy_n(
					vUrlComponents.lpszUrlPath,
					vUrlComponents.dwUrlPathLength,
					wcsPathExtra.GetCStr()
				)
			);
		}

		Utf16String wcsUserName;
		Utf16String wcsPassword;

		if(vUrlComponents.lpszUserName){
			wcsUserName.Assign(vUrlComponents.lpszUserName, vUrlComponents.dwUserNameLength);
			vUrlComponents.lpszUserName = nullptr;
			vUrlComponents.dwUserNameLength = 0;
		}
		if(vUrlComponents.lpszPassword){
			wcsPassword.Assign(vUrlComponents.lpszPassword, vUrlComponents.dwPasswordLength);
			vUrlComponents.lpszPassword = nullptr;
			vUrlComponents.dwPasswordLength = 0;
		}

		xWinHttpHandle hConnect(::WinHttpConnect(
			xm_hSession.Get(),
			wcsHostName.GetCStr(),
			vUrlComponents.nPort,
			0
		));
		if(!hConnect){
			MCF_THROW(::GetLastError(), L"::WinHttpConnect() 失败。");
		}

		xWinHttpHandle hRequest(::WinHttpOpenRequest(
			hConnect.Get(),
			pwszVerb,
			wcsPathExtra.GetCStr(),
			nullptr,
			WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			(vUrlComponents.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0
		));
		if(!hRequest){
			MCF_THROW(::GetLastError(), L"::WinHttpOpenRequest() 失败。");
		}

		static const DWORD DISABLE_COOKIES = WINHTTP_DISABLE_COOKIES;
		if(!::WinHttpSetOption(hRequest.Get(), WINHTTP_OPTION_DISABLE_FEATURE, (void *)&DISABLE_COOKIES, sizeof(DISABLE_COOKIES))){
			MCF_THROW(::GetLastError(), L"::WinHttpSetOption() 失败。");
		}

		if(!xm_ucsPacUrl.IsEmpty()){
			DWORD dwUrlLength = 0;
			::WinHttpCreateUrl(&vUrlComponents, 0, nullptr, &dwUrlLength);
			const auto ulErrorCode = ::GetLastError();
			if(ulErrorCode != ERROR_INSUFFICIENT_BUFFER){
				MCF_THROW(::GetLastError(), L"::WinHttpCreateUrl() 失败。");
			}
			Utf16String wcsUrlWithoutAuth;
			wcsUrlWithoutAuth.Resize(dwUrlLength);
			if(!::WinHttpCreateUrl(&vUrlComponents, 0, wcsUrlWithoutAuth.GetCStr(), &dwUrlLength)){
				MCF_THROW(::GetLastError(), L"::WinHttpCreateUrl() 失败。");
			}
			wcsUrlWithoutAuth.Resize(dwUrlLength);

			WINHTTP_AUTOPROXY_OPTIONS vAutoProxyOptions;
			vAutoProxyOptions.dwFlags					= WINHTTP_AUTOPROXY_CONFIG_URL;
			vAutoProxyOptions.dwAutoDetectFlags			= 0;
			vAutoProxyOptions.lpszAutoConfigUrl			= xm_ucsPacUrl.GetCStr();
			vAutoProxyOptions.lpvReserved 				= nullptr;
			vAutoProxyOptions.dwReserved  				= 0;
			vAutoProxyOptions.fAutoLogonIfChallenged	= FALSE;

			WINHTTP_PROXY_INFO vProxyInfo;
			if(!::WinHttpGetProxyForUrl(xm_hSession.Get(), wcsUrlWithoutAuth.GetCStr(), &vAutoProxyOptions, &vProxyInfo)){
				MCF_THROW(::GetLastError(), L"::WinHttpGetProxyForUrl() 失败。");
			}
			const xPGlobal pProxy		(vProxyInfo.lpszProxy);
			const xPGlobal pProxyBypass	(vProxyInfo.lpszProxyBypass);

			if(!::WinHttpSetOption(hRequest.Get(), WINHTTP_OPTION_PROXY, &vProxyInfo, sizeof(vProxyInfo))){
				MCF_THROW(::GetLastError(), L"::WinHttpSetOption() 失败。");
			}
		}

		if(!xm_ucsProxyUserName.IsEmpty()){
			if(!::WinHttpSetCredentials(
				hRequest.Get(),
				WINHTTP_AUTH_TARGET_PROXY,
				WINHTTP_AUTH_SCHEME_BASIC,
				xm_ucsProxyUserName.GetCStr(),
				xm_ucsProxyPassword.GetCStr(),
				nullptr
			)){
				MCF_THROW(::GetLastError(), L"::WinHttpSetCredentials() 失败。");
			}
		}

		if(!wcsUserName.IsEmpty()){
			if(!::WinHttpSetCredentials(
				hRequest.Get(),
				WINHTTP_AUTH_TARGET_SERVER,
				WINHTTP_AUTH_SCHEME_BASIC,
				wcsUserName.GetCStr(),
				wcsPassword.GetCStr(),
				nullptr
			)){
				MCF_THROW(::GetLastError(), L"::WinHttpSetCredentials() 失败。");
			}
		}

		// add headers

		if(uContentSize > 0){
			Utf16String wcsContentType(L"Content-Type: ");
			wcsContentType += pwszContentType;
			if(!::WinHttpAddRequestHeaders(
				hRequest.Get(),
				wcsContentType.GetCStr(),
				wcsContentType.GetSize(),
				WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE
			)){
				MCF_THROW(::GetLastError(), L"::WinHttpAddRequestHeaders() 失败。");
			}
		}

		if(!::WinHttpSendRequest(
			hRequest.Get(),
			WINHTTP_NO_ADDITIONAL_HEADERS,
			0,
			(void *)pContents,
			uContentSize,
			uContentSize,
			0
		)){
			MCF_THROW(::GetLastError(), L"::WinHttpSendRequest() 失败。");
		}

		if(!::WinHttpReceiveResponse(hRequest.Get(), nullptr)){
			MCF_THROW(::GetLastError(), L"::WinHttpReceiveResponse() 失败。");
		}

		DWORD dwCookieIndex = 0;
		for(;;){
			Utf16String wcsSetCookie;
			DWORD dwSetCookieLength = 127 * sizeof(wchar_t);
			wcsSetCookie.Resize(dwSetCookieLength / sizeof(wchar_t));
			if(!::WinHttpQueryHeaders(
				hRequest.Get(),
				WINHTTP_QUERY_SET_COOKIE,
				WINHTTP_HEADER_NAME_BY_INDEX,
				wcsSetCookie.GetCStr(),
				&dwSetCookieLength,
				&dwCookieIndex
			)){
				auto ulErrorCode = ::GetLastError();
				if(ulErrorCode == ERROR_WINHTTP_HEADER_NOT_FOUND){
					break;
				}
				if(ulErrorCode != ERROR_INSUFFICIENT_BUFFER){
					MCF_THROW(ulErrorCode, L"::WinHttpQueryHeaders() 失败。");
				}
				wcsSetCookie.Resize(dwSetCookieLength / sizeof(wchar_t));
				if(!::WinHttpQueryHeaders(
					hRequest.Get(),
					WINHTTP_QUERY_SET_COOKIE,
					WINHTTP_HEADER_NAME_BY_INDEX,
					wcsSetCookie.GetCStr(),
					&dwSetCookieLength,
					&dwCookieIndex
				)){
					MCF_THROW(ulErrorCode, L"::WinHttpQueryHeaders() 失败。");
				}
			}
			wcsSetCookie.Resize(dwSetCookieLength / sizeof(wchar_t));

			printf("set-cookie: %ls\n", wcsSetCookie.GetCStr());
		}

		xm_hConnect = std::move(hConnect);
		xm_hRequest = std::move(hRequest);
	}
	void Disconnect() noexcept {
		xm_hRequest.Reset();
		xm_hConnect.Reset();
	}
};

// 构造函数和析构函数。
HttpClient::HttpClient(bool bAutoProxy, const wchar_t *pwchProxy, std::size_t uProxyLen, const wchar_t *pwszUserAgent)
	: xm_pDelegate(new xDelegate(bAutoProxy, pwchProxy, uProxyLen, pwszUserAgent))
{
}
HttpClient::HttpClient(bool bAutoProxy, const Utf16String &wcsProxy, const wchar_t *pwszUserAgent)
	: HttpClient(bAutoProxy, wcsProxy.GetCStr(), wcsProxy.GetLength(), pwszUserAgent)
{
}
HttpClient::HttpClient(HttpClient &&rhs) noexcept
	: xm_pDelegate(std::move(rhs.xm_pDelegate))
{
}
HttpClient &HttpClient::operator=(HttpClient &&rhs) noexcept {
	if(&rhs != this){
		xm_pDelegate = std::move(rhs.xm_pDelegate);
	}
	return *this;
}
HttpClient::~HttpClient(){
}

// 其他非静态成员函数。
unsigned long HttpClient::ConnectNoThrow(
	const wchar_t *pwszVerb,
	const wchar_t *pwchUrl,
	std::size_t uUrlLen,
	const void *pContents,
	std::size_t uContentSize,
	const wchar_t *pwszContentType
){
	try {
		Connect(pwszVerb, pwchUrl, uUrlLen, pContents, uContentSize, pwszContentType);
		return ERROR_SUCCESS;
	} catch(Exception &e){
		return e.ulErrorCode;
	}
}
void HttpClient::Connect(
	const wchar_t *pwszVerb,
	const wchar_t *pwchUrl,
	std::size_t uUrlLen,
	const void *pContents,
	std::size_t uContentSize,
	const wchar_t *pwszContentType
){
	xm_pDelegate->Connect(pwszVerb, pwchUrl, uUrlLen, pContents, uContentSize, pwszContentType);
}

unsigned long HttpClient::ConnectNoThrow(
	const wchar_t *pwszVerb,
	const Utf16String &wcsUrl,
	const void *pContents,
	std::size_t uContentSize,
	const wchar_t *pwszContentType
){
	return ConnectNoThrow(pwszVerb, wcsUrl.GetCStr(), wcsUrl.GetLength(), pContents, uContentSize, pwszContentType);
}
void HttpClient::Connect(
	const wchar_t *pwszVerb,
	const Utf16String &wcsUrl,
	const void *pContents,
	std::size_t uContentSize,
	const wchar_t *pwszContentType
){
	Connect(pwszVerb, wcsUrl.GetCStr(), wcsUrl.GetLength(), pContents, uContentSize, pwszContentType);
}
void HttpClient::Disconnect() noexcept {
	xm_pDelegate->Disconnect();
}
