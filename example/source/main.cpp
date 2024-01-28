#include <Windows.h>
#include <stdio.h>
#include <string>
#include <filesystem>
#include <format>
#include "../../bindfltapi.h"

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

bool TestMapSourceToDest(HANDLE JobHandle)
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

bool TestSpawnProcessInSiloWithRemapping(const wchar_t *ProcessPath)
{
	printf("Spawning process with virtualized filesystem...\n\n");

	// In order to create a temporary bindflt mapping we need to create an application silo. In order to
	// create an application silo we need to create a job object with the appropriate flags. Then we spawn
	// processes within said job object.
	//
	// Note these application silos aren't equivalent to full Windows Server silos. They're traditionally
	// used for UWP isolation on client SKUs.
	auto jobObject = CreateJobObjectW(nullptr, nullptr);

	if (!jobObject)
	{
		printf("CreateJobObjectW failed: %u\n", GetLastError());
		return false;
	}

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = {};
	jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	SetInformationJobObject(jobObject, JobObjectExtendedLimitInformation, &jobInfo, sizeof(jobInfo));
	SetInformationJobObject(jobObject, JobObjectCreateSilo, nullptr, 0);

	// Processes must be created with a job object (silo) attached. Assigning them to jobs after the fact
	// won't work.
	size_t listSize = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &listSize);

	auto attributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(_alloca(listSize));
	if (!InitializeProcThreadAttributeList(attributeList, 1, 0, &listSize))
	{
		printf("InitializeProcThreadAttributeList failed: %u\n", GetLastError());
		return false;
	}

	if (!UpdateProcThreadAttribute(attributeList, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, &jobObject, sizeof(jobObject), nullptr, nullptr))
	{
		printf("UpdateProcThreadAttribute failed: %u\n", GetLastError());
		return false;
	}

	// Finally remap directories and create the process
	TestMapSourceToDest(jobObject);
	TestDumpMappings(jobObject);

	PROCESS_INFORMATION processInfo = {};
	STARTUPINFOEXW startupInfoEx = {};
	startupInfoEx.lpAttributeList = attributeList;
	startupInfoEx.StartupInfo.cb = sizeof(startupInfoEx);

	const bool processCreated = CreateProcessW(
		ProcessPath,
		nullptr,
		nullptr,
		nullptr,
		false,
		EXTENDED_STARTUPINFO_PRESENT,
		nullptr,
		nullptr,
		&startupInfoEx.StartupInfo,
		&processInfo);

	if (processCreated)
	{
		WaitForSingleObject(processInfo.hProcess, INFINITE);
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
	}
	else
	{
		printf("CreateProcessW failed: %u\n", GetLastError());
	}

	DeleteProcThreadAttributeList(attributeList);
	CloseHandle(jobObject);

	return processCreated;
}

int main(int argc, char **argv)
{
	TestLoadImports();

	// TestMapSourceToDest(nullptr);
	TestMergeMultipleDirectories(nullptr);
	TestDumpMappings(nullptr);

	TestSpawnProcessInSiloWithRemapping(L"C:\\Windows\\System32\\cmd.exe");

	printf("Done\n");
	return 0;
}
