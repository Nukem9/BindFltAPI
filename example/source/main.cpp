#include <Windows.h>
#include <stdio.h>
#include <string>
#include <filesystem>
#include <format>
#include "../../bindfltapi.h"
#include "util.h"

const std::wstring g_TestingRootPath = L"D:\\vfs-testing-root";

decltype(&BfSetupFilter) PfnBfSetupFilter;
decltype(&BfRemoveMapping) PfnBfRemoveMapping;
decltype(&BfGetMappings) PfnBfGetMappings;

bool TestLoadImports()
{
	// Expected from Win10 RS6 or newer
	const auto bindfltModule = LoadLibraryW(L"bindfltapi.dll");

	PfnBfSetupFilter = reinterpret_cast<decltype(&BfSetupFilter)>(GetProcAddress(bindfltModule, "BfSetupFilter"));
	PfnBfRemoveMapping = reinterpret_cast<decltype(&BfRemoveMapping)>(GetProcAddress(bindfltModule, "BfRemoveMapping"));
	PfnBfGetMappings = reinterpret_cast<decltype(&BfGetMappings)>(GetProcAddress(bindfltModule, "BfGetMappings"));

	if (!bindfltModule || !PfnBfSetupFilter || !PfnBfRemoveMapping || !PfnBfGetMappings)
	{
		printf("Failed to load bindfltapi DLL: %u\n", GetLastError());
		return false;
	}

	return true;
}

bool TestDumpMappings(HANDLE JobHandle)
{
	printf("Dumping bindflt mappings for job 0x%p...\n\n", JobHandle);

	ULONG bufferSize = 128 * 1024;
	auto buffer = reinterpret_cast<uintptr_t>(_alloca(bufferSize));

	const auto hr = PfnBfGetMappings(
		JobHandle ? BINDFLT_GET_MAPPINGS_FLAG_SILO : BINDFLT_GET_MAPPINGS_FLAG_VOLUME, // Mutually exclusive flags
		JobHandle,
		JobHandle ? nullptr : g_TestingRootPath.c_str(),							   // Default to any on drive D:
		nullptr,
		&bufferSize,
		reinterpret_cast<void *>(buffer));

	const auto info = reinterpret_cast<const PBINDFLT_GET_MAPPINGS_INFO>(buffer);

	if (FAILED(hr) || info->Status != 0)
	{
		printf("BfGetMappings failed: 0x%X (0x%X)\n", hr, info->Status);
		return false;
	}

	for (uint32_t i = 0; i < info->MappingCount; i++)
	{
		auto& mapping = info->Entries[i];

		std::wstring_view virtRoot(
			reinterpret_cast<const wchar_t *>(buffer + mapping.VirtRootOffset),
			mapping.VirtRootLength / sizeof(wchar_t));

		printf("Mapping %u:\n", i);
		printf("\tVirtRoot: %.*S\n", static_cast<uint32_t>(virtRoot.length()), virtRoot.data());
		printf("\tFlags: 0x%X\n", mapping.Flags);

		for (uint32_t j = 0; j < mapping.NumberOfTargets; j++)
		{
			auto target = reinterpret_cast<const PBINDFLT_GET_MAPPINGS_TARGET_ENTRY>(
				buffer + mapping.TargetEntriesOffset + (j * sizeof(BINDFLT_GET_MAPPINGS_TARGET_ENTRY)));

			std::wstring_view targetRoot(
				reinterpret_cast<const wchar_t *>(buffer + target->TargetRootOffset),
				target->TargetRootLength / sizeof(wchar_t));

			printf("\tTarget %u:\n", j);
			printf("\t\tTargetRoot: %.*S\n", static_cast<uint32_t>(targetRoot.length()), targetRoot.data());
		}

		printf("\n");
	}

	return true;
}

bool TestMergeMultipleDirectories(HANDLE JobHandle)
{
	printf("Mapping multiple directories for job 0x%p...\n\n", JobHandle);

	const std::wstring testingRoot = g_TestingRootPath;

	PfnBfRemoveMapping(JobHandle, std::format(L"{}\\virtualfinal", testingRoot).c_str());
	PfnBfRemoveMapping(JobHandle, std::format(L"{}\\aliased_to_virtualfinal", testingRoot).c_str());

	std::filesystem::create_directories(std::format(L"{}\\physical1\\", testingRoot));
	std::filesystem::create_directories(std::format(L"{}\\physical2\\", testingRoot));
	std::filesystem::create_directories(std::format(L"{}\\physical3\\", testingRoot));

	fclose(_wfopen(std::format(L"{}\\physical1\\file1.txt", testingRoot).c_str(), L"ab+"));
	fclose(_wfopen(std::format(L"{}\\physical2\\file2.txt", testingRoot).c_str(), L"ab+"));
	fclose(_wfopen(std::format(L"{}\\physical3\\file3.txt", testingRoot).c_str(), L"ab+"));

	auto hr = S_OK;

	hr |= PfnBfSetupFilter(
		JobHandle,
		BINDFLT_FLAG_READ_ONLY_MAPPING | BINDFLT_FLAG_MERGED_BIND_MAPPING, // readonly isn't actually readonly in merge mode
		std::format(L"{}\\virtualfinal", testingRoot).c_str(),
		std::format(L"{}\\physical1", testingRoot).c_str(),
		nullptr,
		0);

	hr |= PfnBfSetupFilter(
		JobHandle,
		BINDFLT_FLAG_READ_ONLY_MAPPING | BINDFLT_FLAG_MERGED_BIND_MAPPING,
		std::format(L"{}\\virtualfinal", testingRoot).c_str(),
		std::format(L"{}\\physical2", testingRoot).c_str(),
		nullptr,
		0);

	hr |= PfnBfSetupFilter(
		JobHandle,
		BINDFLT_FLAG_READ_ONLY_MAPPING | BINDFLT_FLAG_MERGED_BIND_MAPPING,
		std::format(L"{}\\virtualfinal", testingRoot).c_str(),
		std::format(L"{}\\physical3", testingRoot).c_str(),
		nullptr,
		0);

	hr |= PfnBfSetupFilter(
		JobHandle,
		BINDFLT_FLAG_READ_ONLY_MAPPING | BINDFLT_FLAG_MERGED_BIND_MAPPING,
		std::format(L"{}\\aliased_to_virtualfinal", testingRoot).c_str(),
		std::format(L"{}\\virtualfinal", testingRoot).c_str(), // this ends up pointing to physical3 for whatever reason. wrong.
		nullptr,
		0);

	if (FAILED(hr))
	{
		printf("BfSetupFilter failed: 0x%X\n", hr);
		return false;
	}

	return true;
}

bool TestMapFileSourceToDest(HANDLE JobHandle)
{
	printf("Mapping virtual file for job 0x%p...\n\n", JobHandle);

	const std::wstring testingRoot = g_TestingRootPath;

	const auto sourcePath = std::format(L"{}\\physical_file_1.txt", testingRoot);
	const auto destPath = std::format(L"{}\\virtual_file_1.txt", testingRoot);

	fclose(_wfopen(sourcePath.c_str(), L"ab+"));

	auto hr = S_OK;
	hr = PfnBfRemoveMapping(JobHandle, destPath.c_str());
	hr = PfnBfSetupFilter(JobHandle, BINDFLT_FLAG_USE_CURRENT_SILO_MAPPING, destPath.c_str(), sourcePath.c_str(), nullptr, 0);

	if (FAILED(hr))
	{
		printf("BfSetupFilter failed: 0x%X\n", hr);
		return false;
	}

	return true;
}

bool TestSpawnProcessInSiloWithFileRemapping(const wchar_t *ProcessPath)
{
	return SpawnProcess(
		ProcessPath,
		[](HANDLE JobObject)
		{
			TestMapFileSourceToDest(JobObject);
			TestDumpMappings(JobObject);
		},
		nullptr);
}

bool TestSpawnProcessInSiloWithSystemLibraryRemapping(const wchar_t *ProcessPath)
{
	return SpawnProcess(
		ProcessPath,
		[](HANDLE JobObject)
		{
			printf("Mapping virtual file for job 0x%p...\n\n", JobObject);
			CopyFileW(L"C:\\Windows\\System32\\KernelBase.dll", std::format(L"{}\\KernelBase_copy.dll", g_TestingRootPath).c_str(), false);
			CopyFileW(L"C:\\Windows\\System32\\kernel32.dll", std::format(L"{}\\kernel32_copy.dll", g_TestingRootPath).c_str(), false);

			auto physPath = std::format(L"{}\\kernel32_copy.dll", g_TestingRootPath);
			auto virtPath = std::format(L"C:\\Windows\\System32\\kernel32.dll");

			auto hr = PfnBfRemoveMapping(JobObject, virtPath.c_str());
			hr = PfnBfSetupFilter(
				JobObject,
				BINDFLT_FLAG_USE_CURRENT_SILO_MAPPING | BINDFLT_FLAG_PREVENT_CASE_SENSITIVE_BINDING,
				virtPath.c_str(),
				physPath.c_str(),
				nullptr,
				0);

			physPath = std::format(L"{}\\KernelBase_copy.dll", g_TestingRootPath);
			virtPath = std::format(L"C:\\Windows\\System32\\KernelBase.dll");

			hr = PfnBfRemoveMapping(JobObject, virtPath.c_str());
			hr = PfnBfSetupFilter(
				JobObject,
				BINDFLT_FLAG_USE_CURRENT_SILO_MAPPING | BINDFLT_FLAG_PREVENT_CASE_SENSITIVE_BINDING,
				virtPath.c_str(),
				physPath.c_str(),
				nullptr,
				0);

			if (FAILED(hr))
			{
				printf("BfSetupFilter failed: 0x%X\n", hr);
				__debugbreak();
			}

			TestDumpMappings(JobObject);
		},
		[](HANDLE JobObject, HANDLE Process)
		{
			using PfnNtQueryInformationProcess = LONG(WINAPI *)(HANDLE, UINT, PVOID, ULONG, PULONG);

			typedef struct _PROCESS_BASIC_INFORMATION
			{
				LONG ExitStatus;
				struct PEB *PebBaseAddress;
				KAFFINITY AffinityMask;
				UINT BasePriority;
				HANDLE UniqueProcessId;
				HANDLE InheritedFromUniqueProcessId;
			} PROCESS_BASIC_INFORMATION, *PPROCESS_BASIC_INFORMATION;

			PROCESS_BASIC_INFORMATION pbi = {};

			auto ntdllHandle = GetModuleHandleW(L"ntdll.dll");
			auto ntQIP = reinterpret_cast<PfnNtQueryInformationProcess>(GetProcAddress(ntdllHandle, "NtQueryInformationProcess"));

			// !!! Set PEB->IsProtectedProcess to 1 to prevent KnownDLLs section mapping from being used. Otherwise ntdll never
			// !!! queries the filesystem for the DLL and we can't remap it.
			if (ntQIP(Process, 0, &pbi, sizeof(pbi), nullptr) == 0)
			{
				printf("Process PEB: 0x%p\n", pbi.PebBaseAddress);

				const uintptr_t pebFlagsAddress = reinterpret_cast<uintptr_t>(pbi.PebBaseAddress) + 0x3;
				const bool status = WriteProcessMemory(Process, reinterpret_cast<void *>(pebFlagsAddress), "\x02", 1, nullptr);

				printf("Enable PEB->IsProtectedProcess: %s\n", status ? "Succeeded" : "Failed");
			}

			printf("\n");
		});
}

int main(int argc, char **argv)
{
	TestLoadImports();
	printf("Using scratch directory: %S\n", g_TestingRootPath.c_str());

	// Globally remap one file to another
	//TestMapFileSourceToDest(nullptr);

	// Globally remap and merge multiple directories to other virtual directories
	//TestMergeMultipleDirectories(nullptr);

	// Globally list mappings
	//TestDumpMappings(nullptr);

	// Per-process remap one file to another and list mappings
	//TestSpawnProcessInSiloWithFileRemapping(L"C:\\Windows\\System32\\cmd.exe");

	// Per-process remap system library (kernel32.dll, kernelbase.dll) and list mappings
	TestSpawnProcessInSiloWithSystemLibraryRemapping(L"C:\\Windows\\System32\\cmd.exe");

	printf("Done\n");
	return 0;
}
