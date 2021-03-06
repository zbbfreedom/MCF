// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2017, LH_Mouse. All wrongs reserved.

#include "IsaacGenerator.hpp"
#include "../Core/CopyMoveFill.hpp"

namespace MCF {

// http://www.burtleburtle.net/bob/rand/isaacafa.html
// http://www.burtleburtle.net/bob/c/readable.c

void IsaacGenerator::X_RefreshInternal() noexcept {
	++x_u32C;
	x_u32B += x_u32C;

	for(std::size_t uIndex = 0; uIndex < 256; uIndex += 4){
		const auto Step = [&](auto uStep, auto fnSpec){
			const auto u32X = x_au32Internal[uIndex + uStep];
			fnSpec();
			x_u32A += x_au32Internal[(uIndex + uStep + 128) % 256];
			const auto u32Y = x_au32Internal[(u32X >> 2) % 256] + x_u32A + x_u32B;
			x_au32Internal[uIndex + uStep] = u32Y;
			x_u32B = x_au32Internal[(u32Y >> 10) % 256] + u32X;
			x_au32Result[uIndex + uStep] = x_u32B;
		};

		Step(0u, [this]{ x_u32A ^= (x_u32A << 13); });
		Step(1u, [this]{ x_u32A ^= (x_u32A >>  6); });
		Step(2u, [this]{ x_u32A ^= (x_u32A <<  2); });
		Step(3u, [this]{ x_u32A ^= (x_u32A >> 16); });
	}
}

void IsaacGenerator::Init(std::uint32_t u32Seed) noexcept {
	Array<std::uint32_t, 8> au32Seed;
	Fill(au32Seed.GetBegin(), au32Seed.GetEnd(), u32Seed);
	Init(au32Seed);
}
void IsaacGenerator::Init(const Array<std::uint32_t, 8> &au32Seed) noexcept {
	std::uint32_t au32Temp[8];
	FillN(au32Temp, 8, 0x9E3779B9u);

	const auto Mix = [&]{
		au32Temp[0] ^= (au32Temp[1] << 11); au32Temp[3] += au32Temp[0]; au32Temp[1] += au32Temp[2];
		au32Temp[1] ^= (au32Temp[2] >>  2); au32Temp[4] += au32Temp[1]; au32Temp[2] += au32Temp[3];
		au32Temp[2] ^= (au32Temp[3] <<  8); au32Temp[5] += au32Temp[2]; au32Temp[3] += au32Temp[4];
		au32Temp[3] ^= (au32Temp[4] >> 16); au32Temp[6] += au32Temp[3]; au32Temp[4] += au32Temp[5];
		au32Temp[4] ^= (au32Temp[5] << 10); au32Temp[7] += au32Temp[4]; au32Temp[5] += au32Temp[6];
		au32Temp[5] ^= (au32Temp[6] >>  4); au32Temp[0] += au32Temp[5]; au32Temp[6] += au32Temp[7];
		au32Temp[6] ^= (au32Temp[7] <<  8); au32Temp[1] += au32Temp[6]; au32Temp[7] += au32Temp[0];
		au32Temp[7] ^= (au32Temp[0] >>  9); au32Temp[2] += au32Temp[7]; au32Temp[0] += au32Temp[1];
	};

	for(std::size_t uIndex = 0; uIndex < 4; ++uIndex){
		Mix();
	}
	for(std::size_t uIndex = 0; uIndex < 256; uIndex += 8){
		for(std::size_t uStep = 0; uStep < 8; ++uStep){
			au32Temp[uStep] += au32Seed[uStep];
		}
		Mix();
		CopyN(x_au32Internal + uIndex, au32Temp, 8);
	}
	for(std::size_t uIndex = 0; uIndex < 256; uIndex += 8){
		for(std::size_t uStep = 0; uStep < 8; ++uStep){
			au32Temp[uStep] += x_au32Internal[uIndex + uStep];
		}
		Mix();
		CopyN(x_au32Internal + uIndex, au32Temp, 8);
	}
	x_u32A = 0;
	x_u32B = 0;
	x_u32C = 0;
	X_RefreshInternal();

	x_uRead = 0;
}

std::uint32_t IsaacGenerator::Get() noexcept {
	if(x_uRead == 0){
		X_RefreshInternal();
	}
	const auto u32Ret = x_au32Result[x_uRead];
	x_uRead = (x_uRead + 1) % 256;
	return u32Ret;
}

}
