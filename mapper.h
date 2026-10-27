#pragma once
#include <windows.h>
#include <src/memory/DigiMapperApi.hpp>
#include <src/memory/driver_bytes.h>

inline bool MapDriverLocally() {
	if (azure_driver_size == 0)
		return false;

	DigiMapperApi::MapperConfig config{};
	NTSTATUS exit_code = 0;
	const ULONG64 result = DigiMapperApi::MapFromBytes(
		azure_driver,
		azure_driver_size,
		config,
		&exit_code
	);

	return result != 0 && exit_code == 0;
}

inline bool LoadDriver() {
	return MapDriverLocally();
}
