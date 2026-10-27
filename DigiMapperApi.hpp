#pragma once

#include <Windows.h>
#include "Mapper.hpp"

namespace DigiMapperApi
{
	struct MapperConfig
	{
		bool free = false;
		bool indPages = true;
		bool passAllocationPtr = true;
		bool copyHeader = false;
		ULONG64 param1 = 0;
		ULONG64 param2 = 0;
		DigiMapper::mapCallback callback = nullptr;
	};

	ULONG64 MapFromBytes(
		BYTE* driverData,
		size_t driverSize,
		MapperConfig config = {},
		NTSTATUS* exitCode = nullptr
	);
}
