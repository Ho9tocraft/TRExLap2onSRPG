#pragma once
#include <array>
#include <cstdint>

class Xorshift1024ss
{
private:
	std::array<std::uint64_t, 16> state_;
	int p_ = 0;
	static std::uint64_t rotl(const std::uint64_t x, int k) noexcept;
public:
	using ResultType = std::uint64_t;

	static constexpr ResultType min() { return 0; }
	static constexpr ResultType max() { return 0xffffffffffffffffui64; }

	explicit Xorshift1024ss(std::uint64_t seed) noexcept;

	ResultType operator()() noexcept;

	void jump() noexcept;
};

