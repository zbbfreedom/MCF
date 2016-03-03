// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2016, LH_Mouse. All wrongs reserved.

#ifndef MCF_STREAM_FILTERS_TEXT_OUTPUT_STREAM_FILTER_HPP_
#define MCF_STREAM_FILTERS_TEXT_OUTPUT_STREAM_FILTER_HPP_

#include "AbstractOutputStreamFilter.hpp"
#include "../Core/StreamBuffer.hpp"

namespace MCF {

class TextOutputStreamFilter : public AbstractOutputStreamFilter {
private:
	mutable StreamBuffer x_vBuffer;

public:
	explicit TextOutputStreamFilter(PolyIntrusivePtr<AbstractOutputStream> pUnderlyingStream) noexcept
		: AbstractOutputStreamFilter(std::move(pUnderlyingStream))
	{
	}
	~TextOutputStreamFilter() override;

	TextOutputStreamFilter(TextOutputStreamFilter &&) noexcept = default;
	TextOutputStreamFilter& operator=(TextOutputStreamFilter &&) noexcept = default;

public:
	void Put(unsigned char byData) override;

	void Put(const void *pData, std::size_t uSize) override;

	void Flush(bool bHard) override;

	void Swap(TextOutputStreamFilter &rhs) noexcept {
		using std::swap;
		swap(x_pUnderlyingStream, rhs.x_pUnderlyingStream);
		swap(x_vBuffer,           rhs.x_vBuffer);
	}

	friend void swap(TextOutputStreamFilter &lhs, TextOutputStreamFilter &rhs) noexcept {
		lhs.Swap(rhs);
	}
};

}

#endif