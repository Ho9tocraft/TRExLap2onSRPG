#pragma once
#include <cstdint>
#include <string>

#include "TRExLap2Enums.hpp"

class TRExLap2SpCommand
{
protected:
	TRExLap2SpiritualsType spType;
	std::u8string spNameJa;
	std::u8string spNameEn;
	std::int64_t spLearnLevel;
	std::int64_t spDefaultCost;
	std::int64_t spCost;
public:
	std::int64_t getLearnLevel() const;
	std::int64_t getSpCost() const;
	TRExLap2SpCommand(std::u8string pSpNameJa, std::u8string pSpNameEn, TRExLap2SpiritualsType pSpType, std::int64_t pSpDefaultCost);
};

