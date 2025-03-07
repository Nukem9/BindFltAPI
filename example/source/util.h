#pragma once

#include <functional>

bool SpawnProcess(
	const wchar_t *ProcessPath,
	std::function<void(HANDLE JobObject)> JobSetup,
	std::function<void(HANDLE JobObject, HANDLE Process)> ProcessCreated)
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

	if (JobSetup)
		JobSetup(jobObject);

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
		EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED,
		nullptr,
		nullptr,
		&startupInfoEx.StartupInfo,
		&processInfo);

	if (processCreated)
	{
		if (ProcessCreated)
			ProcessCreated(jobObject, processInfo.hProcess);

		ResumeThread(processInfo.hThread);
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
