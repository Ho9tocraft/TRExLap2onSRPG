#include "pch.hpp"
#include "Xorshift1024ss.hpp"

std::uint64_t Xorshift1024ss::rotl(const std::uint64_t x, int k) noexcept
{
	return (x << k) | (x >> (64 - k));
}

Xorshift1024ss::Xorshift1024ss(std::uint64_t seed) noexcept
{
	std::uint64_t z = seed;
	for (auto& s : this->state_)
	{
		z += 0x9e3779b97f4a7c15ui64;
		uint64_t val = z;
		val = (val ^ (val >> 30)) * 0xbf58476d1ce4e5b9ui64;
		val = (val ^ (val >> 27)) * 0x94d049bb133111ebui64;
		s = val ^ (val >> 31);
	}

	if (std::all_of(this->state_.begin(), this->state_.end(), [](std::uint64_t s) { return s == 0; }))
	{
		this->state_[0] = 1;
	}
}

Xorshift1024ss::ResultType Xorshift1024ss::operator()() noexcept
{
	const int q = this->p_;
	this->p_ = (p_ + 1) & 15;

	const uint64_t s0 = this->state_[this->p_];
	uint64_t s15 = this->state_[q];

	const uint64_t result = rotl(s0 * 5, 7) + 9;

	s15 ^= s0;
	state_[q] = rotl(s0, 25) ^ s15 ^ (s15 << 27);
	state_[this->p_] = rotl(s15, 36);

	return result;
}

void Xorshift1024ss::jump() noexcept
{
	static constexpr uint64_t JUMP[] = {
		0x817f1d179e75dfc5, 0xec61e1a216859367, 0x39da4e23d1812197, 0x5a286199b57455e6,
		0x62eec44b3602d45b, 0xcdc90f05500144f8, 0x6e5546fa01c3181b, 0x6e1b46a337d6a457,
		0x6ee2d8ac11a68bf3, 0x6733f3807217c244, 0x47e87b7a8eeed201, 0x229fc8b560868f76,
		0xdb910d54a20b0805, 0x1d4d8ee9eb0c6b1a, 0x51cd889547d33fc0, 0x37fc8ea83bec12cb
	};
	std::array<std::uint64_t, 16> t = {0};
	for (int i = 0; i < 16; i++) {
		for (int b = 0; b < 64; b++) {
			if (JUMP[i] & (1ULL << b)) {
				for (int j = 0; j < 16; j++) {
					t[j] ^= this->state_[(j + this->p_) & 15];
				}
			}
			this->operator()();
		}
	}
	for (int j = 0; j < 16; j++) {
		this->state_[(j + this->p_) & 15] = t[j];
	}
}
