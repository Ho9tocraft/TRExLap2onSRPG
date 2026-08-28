#include "pch.hpp"
#include "TRExLap2SpCommand.hpp"

#include <cstdint>
#include <string>
#include "TRExLap2Enums.hpp"

std::int64_t TRExLap2SpCommand::getLearnLevel() const
{
	return this->spLearnLevel;
}

std::int64_t TRExLap2SpCommand::getSpCost() const
{
	return this->spCost > -1LL ? this->spCost : this->spDefaultCost; //「夢」などの精神コマンドは、コストが0であるので、SPは-1
}

TRExLap2SpCommand::TRExLap2SpCommand(std::u8string pSpNameJa, std::u8string pSpNameEn,
	TRExLap2SpiritualsType pSpType, std::int64_t pSpDefaultCost)
{
	this->spType = pSpType;
	this->spNameJa = pSpNameJa;
	this->spNameEn = pSpNameEn;
	this->spDefaultCost = pSpDefaultCost;
	this->spCost = -1LL;
	this->spLearnLevel = -1LL; // 未習得状態は-1。0はツイン精神コマンドを示す。
}
