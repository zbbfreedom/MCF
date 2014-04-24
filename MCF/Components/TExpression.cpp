// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2014. LH_Mouse. All wrongs reserved.

#include "../StdMCF.hpp"
#include "Notation.hpp"
#include "../Core/VVector.hpp"
using namespace MCF;

namespace {

WideString Unescape(const WideStringObserver &wsoSrc){
	WideString wcsRet;
	wcsRet.Reserve(wsoSrc.GetSize());

	enum {
		NORMAL,
		ESCAPED,
		UCS_CODE
	} eState = NORMAL;

	std::uint32_t u32CodePoint = 0;
	std::size_t uHexExpecting = 0;

	const auto PushUtf = [&]{
		if(u32CodePoint > 0x10FFFF){
			u32CodePoint = 0xFFFD;
		}
		if(u32CodePoint <= 0xFFFF){
			wcsRet.Append(u32CodePoint);
		} else {
			u32CodePoint -= 0x10000;
			wcsRet.Append((u32CodePoint >> 10)   | 0xD800);
			wcsRet.Append((u32CodePoint & 0x3FF) | 0xDC00);
		}
	};

	auto pwcCur = wsoSrc.GetBegin();
	const auto itEnd = wsoSrc.GetEnd();
	while(pwcCur != itEnd){
		const auto wc = *pwcCur;
		++pwcCur;
		switch(eState){
		case NORMAL:
			if(wc == L'\\'){
				eState = ESCAPED;
			} else {
				// eState = NORMAL;
				wcsRet.Push(wc);
			}
			break;

		case ESCAPED:
			switch(wc){
			case L'n':
				wcsRet.Append(L'\n');
				eState = NORMAL;
				break;

			case L'b':
				wcsRet.Append(L'\b');
				eState = NORMAL;
				break;

			case L'r':
				wcsRet.Append(L'\r');
				eState = NORMAL;
				break;

			case L't':
				wcsRet.Append(L'\t');
				eState = NORMAL;
				break;

			case L'x':
				u32CodePoint = 0;
				uHexExpecting = 2;
				eState = UCS_CODE;
				break;

			case L'u':
				u32CodePoint = 0;
				uHexExpecting = 4;
				eState = UCS_CODE;
				break;

			case L'U':
				u32CodePoint = 0;
				uHexExpecting = 8;
				eState = UCS_CODE;
				break;

			default:
				wcsRet.Append(wc);
				eState = NORMAL;
				break;
			}
			break;

		case UCS_CODE:
			{
				unsigned int uHex = wc - L'0';
				do {
					if(uHex <= 9){
						break;
					}
					uHex += L'0' - L'A';
					if(uHex <= 5){
						uHex += 0x0A;
						break;
					}
					uHex += L'A' - L'a';
					if(uHex <= 5){
						uHex += 0x0A;
						break;
					}
					uHex = 0x10;
				} while(false);

				if(uHex <= 0x0F){
					u32CodePoint = (u32CodePoint << 4) | uHex;
					--uHexExpecting;
				} else {
					uHexExpecting = 0;
				}
				if(uHexExpecting != 0){
					// eState = UCS_CODE;
				} else {
					PushUtf();
					eState = NORMAL;
				}
			}
			break;

		default:
			ASSERT(false);
		}
	}
	if(eState == UCS_CODE){
		PushUtf();
	}

	return std::move(wcsRet);
}
void Escape(WideString &wcsAppendTo, const WideStringObserver &wsoSrc){
	wcsAppendTo.Reserve(wcsAppendTo.GetLength() + wsoSrc.GetLength());
	for(const auto wc : wsoSrc){
		switch(wc){
		case L'\\':
		case L'=':
		case L'{':
		case L'}':
		case L';':
			wcsAppendTo.Append(L'\\');
			wcsAppendTo.Append(wc);
			break;

		case L'\n':
			wcsAppendTo.Append(L"\\n");
			break;

		case L'\b':
			wcsAppendTo.Append(L"\\b");
			break;

		case L'\r':
			wcsAppendTo.Append(L"\\r");
			break;

		case L'\t':
			wcsAppendTo.Append(L"\\t");
			break;

		default:
			wcsAppendTo.Append(wc);
			break;
		}
	}
}

}
/*
// ========== NotationPackage ==========
// 其他非静态成员函数。
const NotationPackage *NotationPackage::GetPackage(const WideStringObserver &wsoName) const noexcept {
	const auto pNode = xm_mapPackages.Find<0>(wsoName);
	if(!pNode){
		return nullptr;
	}
	return &(pNode->GetElement());
}
NotationPackage *NotationPackage::GetPackage(const WideStringObserver &wsoName) noexcept {
	const auto pNode = xm_mapPackages.Find<0>(wsoName);
	if(!pNode){
		return nullptr;
	}
	return &(pNode->GetElement());
}
NotationPackage *NotationPackage::CreatePackage(WideString wcsName){
	auto pNode = xm_mapPackages.Find<0>(wcsName);
	if(!pNode){
		pNode = xm_mapPackages.Insert(NotationPackage(), std::move(wcsName));
	}
	return &(pNode->GetElement());
}

const WideString *NotationPackage::GetValue(const WideStringObserver &wsoName) const noexcept {
	const auto pNode = xm_mapValues.Find<0>(wsoName);
	if(!pNode){
		return nullptr;
	}
	return &(pNode->GetElement());
}
WideString *NotationPackage::GetValue(const WideStringObserver &wsoName) noexcept {
	const auto pNode = xm_mapValues.Find<0>(wsoName);
	if(!pNode){
		return nullptr;
	}
	return &(pNode->GetElement());
}
WideString *NotationPackage::CreteValue(WideString wcsName){
	auto pNode = xm_mapValues.Find<0>(wcsName);
	if(!pNode){
		pNode = xm_mapValues.Insert(WideString(), std::move(wcsName));
	}
	return &(pNode->GetElement());
}

void NotationPackage::Clear() noexcept {
	xm_mapPackages.Clear();
	xm_mapValues.Clear();
}

// ========== Notation ==========
// 其他非静态成员函数。
std::pair<Notation::ErrorType, const wchar_t *> Notation::Parse(const WideStringObserver &wsoData){
	Clear();

	auto pwcRead = wsoData.GetBegin();
	if(pwcRead == wsoData.GetEnd()){
		return std::make_pair(ERR_NONE, pwcRead);
	}

	VVector<Package *> vecPackageStack;
	vecPackageStack.Push(this);

	auto itNameBegin = pwcRead;
	auto itNameEnd = pwcRead;
	auto itValueBegin = pwcRead;
	auto itValueEnd = pwcRead;

	enum {
		NAME_INDENT,
		NAME_BODY,
		NAME_PADDING,
		VAL_INDENT,
		VAL_BODY,
		VAL_PADDING,
		COMMENT
	} eState = NAME_INDENT;

	bool bEscaped = false;

	const auto PushPackage = [&]{
		ASSERT(itNameBegin != itNameEnd);

		auto wcsName = Unescape(WideStringObserver(itNameBegin, itNameEnd));
		auto &mapPackages = vecPackageStack.GetEnd()[-1]->xm_mapPackages;
		if(mapPackages.Find<0>(wcsName)){
			return false;
		}
		const auto pNewNode = mapPackages.Insert(Package(), std::move(wcsName));
		vecPackageStack.Push(&(pNewNode->GetElement()));
		itNameBegin = itNameEnd;
		return true;
	};
	const auto PopPackage = [&]{
		ASSERT(vecPackageStack.GetSize() > 0);

		vecPackageStack.Pop();
		itNameBegin = itNameEnd;
	};
	const auto SubmitValue = [&]{
		ASSERT(itNameBegin != itNameEnd);

		auto wcsName = Unescape(WideStringObserver(itNameBegin, itNameEnd));
		auto wcsValue = Unescape(WideStringObserver(itValueBegin, itValueEnd));
		auto &mapValues = vecPackageStack.GetEnd()[-1]->xm_mapValues;
		if(mapValues.Find<0>(wcsName)){
			return false;
		}
		mapValues.Insert(std::move(wcsValue), std::move(wcsName));
		itValueBegin = itValueEnd;
		return true;
	};

	do {
		const wchar_t ch = *pwcRead;

		if(bEscaped){
			bEscaped = false;
		} else {
			switch(ch){
			case L'\\':
				bEscaped = true;
				break;

			case L'=':
				switch(eState){
				case NAME_INDENT:
					return std::make_pair(ERR_NO_VALUE_NAME, pwcRead);

				case NAME_BODY:
				case NAME_PADDING:
					eState = VAL_INDENT;
					continue;

				case VAL_INDENT:
				case VAL_BODY:
				case VAL_PADDING:
					break;

				case COMMENT:
					continue;
				};
				break;

			case L'{':
				switch(eState){
				case NAME_INDENT:
					return std::make_pair(ERR_NO_VALUE_NAME, pwcRead);

				case NAME_BODY:
				case NAME_PADDING:
					if(!PushPackage()){
						return std::make_pair(ERR_DUPLICATE_PACKAGE, pwcRead);
					}
					eState = NAME_INDENT;
					continue;

				case VAL_INDENT:
				case VAL_BODY:
				case VAL_PADDING:
					if(!SubmitValue()){
						return std::make_pair(ERR_DUPLICATE_VALUE, pwcRead);
					}
					if(!PushPackage()){
						return std::make_pair(ERR_DUPLICATE_PACKAGE, pwcRead);
					}
					eState = NAME_INDENT;
					continue;

				case COMMENT:
					continue;
				};
				break;

			case L'}':
				switch(eState){
				case NAME_INDENT:
					if(vecPackageStack.GetSize() == 1){
						return std::make_pair(ERR_UNEXCEPTED_PACKAGE_CLOSE, pwcRead);
					}
					PopPackage();
					eState = NAME_INDENT;
					continue;

				case NAME_BODY:
				case NAME_PADDING:
					return std::make_pair(ERR_EQU_EXPECTED, pwcRead);

				case VAL_INDENT:
				case VAL_BODY:
				case VAL_PADDING:
					if(!SubmitValue()){
						return std::make_pair(ERR_DUPLICATE_VALUE, pwcRead);
					}
					if(vecPackageStack.GetSize() == 1){
						return std::make_pair(ERR_UNEXCEPTED_PACKAGE_CLOSE, pwcRead);
					}
					PopPackage();
					eState = NAME_INDENT;
					continue;

				case COMMENT:
					continue;
				};
				break;

			case L';':
				switch(eState){
				case NAME_INDENT:
					eState = COMMENT;
					continue;

				case NAME_BODY:
				case NAME_PADDING:
					return std::make_pair(ERR_EQU_EXPECTED, pwcRead);

				case VAL_INDENT:
				case VAL_BODY:
				case VAL_PADDING:
					if(!SubmitValue()){
						return std::make_pair(ERR_DUPLICATE_VALUE, pwcRead);
					}
					eState = COMMENT;
					continue;

				case COMMENT:
					continue;
				};
				break;

			case L'\n':
				switch(eState){
				case NAME_INDENT:
					continue;

				case NAME_BODY:
				case NAME_PADDING:
					return std::make_pair(ERR_EQU_EXPECTED, pwcRead);

				case VAL_INDENT:
				case VAL_BODY:
				case VAL_PADDING:
					if(!SubmitValue()){
						return std::make_pair(ERR_DUPLICATE_VALUE, pwcRead);
					}
					eState = NAME_INDENT;
					continue;

				case COMMENT:
					eState = NAME_INDENT;
					continue;
				};
				break;
			}
		}

		if(ch != L'\n'){
			switch(eState){
			case NAME_INDENT:
				if((ch == L' ') || (ch == L'\t')){
					// eState = NAME_INDENT;
				} else {
					itNameBegin = pwcRead;
					itNameEnd = pwcRead + 1;
					eState = NAME_BODY;
				}
				continue;

			case NAME_BODY:
				if((ch == L' ') || (ch == L'\t')){
					eState = NAME_PADDING;
				} else {
					itNameEnd = pwcRead + 1;
					// eState = NAME_BODY;
				}
				continue;

			case NAME_PADDING:
				if((ch == L' ') || (ch == L'\t')){
					// eState = NAME_PADDING;
				} else {
					itNameEnd = pwcRead + 1;
					eState = NAME_BODY;
				}
				continue;

			case VAL_INDENT:
				if((ch == L' ') || (ch == L'\t')){
					// eState = VAL_INDENT;
				} else {
					itValueBegin = pwcRead;
					itValueEnd = pwcRead + 1;
					eState = VAL_BODY;
				}
				continue;

			case VAL_BODY:
				if((ch == L' ') || (ch == L'\t')){
					eState = VAL_PADDING;
				} else {
					itValueEnd = pwcRead + 1;
					// eState = VAL_BODY;
				}
				continue;

			case VAL_PADDING:
				if((ch == L' ') || (ch == L'\t')){
					// eState = VAL_PADDING;
				} else {
					itValueEnd = pwcRead + 1;
					eState = VAL_BODY;
				}
				continue;

			case COMMENT:
				continue;
			}
		}
	} while(++pwcRead != wsoData.GetEnd());

	if(bEscaped){
		return std::make_pair(ERR_ESCAPE_AT_EOF, pwcRead);
	}
	if(vecPackageStack.GetSize() > 1){
		return std::make_pair(ERR_UNCLOSED_PACKAGE, pwcRead);
	}
	switch(eState){
	case NAME_BODY:
	case NAME_PADDING:
		return std::make_pair(ERR_EQU_EXPECTED, pwcRead);

	case VAL_INDENT:
	case VAL_BODY:
	case VAL_PADDING:
		if(!SubmitValue()){
			return std::make_pair(ERR_DUPLICATE_VALUE, pwcRead);
		}
		break;

	default:
		break;
	};

	return std::make_pair(ERR_NONE, pwcRead);
}
WideString Notation::Export(const WideStringObserver &wsoIndent) const {
	WideString wcsRet;

	VVector<std::tuple<const WideString *, const Package *, bool>> vecPackageStack;
	vecPackageStack.Push(nullptr, this, true);
	WideString wcsIndent;
	for(;;){
		auto &vTop = vecPackageStack.GetEnd()[-1];
		const auto &pkgTop = *std::get<1>(vTop);

		if(std::get<2>(vTop)){
			std::get<2>(vTop) = false;

			if(std::get<0>(vTop)){
				wcsRet.Append(wcsIndent);
				Escape(wcsRet, *std::get<0>(vTop));
				wcsRet.Append(L" {\n");
				wcsIndent.Append(wsoIndent);
			}
			for(auto pNode = pkgTop.xm_mapPackages.GetRBegin<0>(); pNode; pNode = pNode->GetPrev<0>()){
				vecPackageStack.Push(&(pNode->GetIndex<0>()), &(pNode->GetElement()), true);
			}
			continue;
		}

		for(auto pNode = pkgTop.xm_mapValues.GetBegin<0>(); pNode; pNode = pNode->GetNext<0>()){
			wcsRet.Append(wcsIndent);
			Escape(wcsRet, pNode->GetIndex<0>());
			wcsRet.Append(L" = ");
			Escape(wcsRet, pNode->GetElement());
			wcsRet.Append(L'\n');
		}

		vecPackageStack.Pop();

		if(!std::get<0>(vTop)){
			break;
		}
		wcsIndent.Truncate(wsoIndent.GetLength());
		wcsRet.Append(wcsIndent);
		wcsRet.Append(L"}\n");
	}

	ASSERT(vecPackageStack.IsEmpty());
	ASSERT(wcsIndent.IsEmpty());

	return std::move(wcsRet);
}
*/
