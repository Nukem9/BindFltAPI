//
// Undocumented BindFlt user mode API.
//
// All functions and structures have been reverse engineered or taken from MIT-licensed
// sources below. See https://github.com/Nukem9/bindfltapi for examples.
//
// Bindlink information
// https://learn.microsoft.com/en-us/windows/win32/api/bindlink/
// https://learn.microsoft.com/en-us/windows/win32/api/bindlink/nf-bindlink-createbindlink
// https://learn.microsoft.com/en-us/windows/win32/api/bindlink/nf-bindlink-removebindlink
//
// BindFlt information
// https://github.com/microsoft/BuildXL/blob/a6dce509f0d4f774255e5fbfb75fa6d5290ed163/Public/Src/Utilities/Native/Processes/Windows/NativeContainerUtilities.cs#L193
// https://github.com/microsoft/hcsshim/blob/bebc7447316b33a2be4efdbd30e306f2c30681a5/internal/winapi/bindflt.go
// https://github.com/microsoft/hcsshim/blob/bedca7475220426727ba4a0d11f042de6b8e73cc/internal/winapi/bindflt.go
// https://github.com/microsoft/go-winio/blob/008bc6ea439f15884bef0b52f2772190c382bf46/pkg/bindfilter/bind_filter.go
//
#ifndef _BINDFLTAPI_
#define _BINDFLTAPI_

#include <winnt.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum _BINDFLT_SETUP_FILTER_FLAGS
{
	BINDFLT_FLAG_NONE = 0x00000000,
	BINDFLT_FLAG_READ_ONLY_MAPPING = 0x00000001,
	BINDFLT_FLAG_MERGED_BIND_MAPPING = 0x00000002, // Generates a merged binding, mapping target entries to the virtualization root.
	BINDFLT_FLAG_USE_CURRENT_SILO_MAPPING = 0x00000004, // Use the binding mapping attached to the mapped-in job object (silo) instead of the default global mapping.
	BINDFLT_FLAG_REPARSE_ON_FILES = 0x00000008,
	BINDFLT_FLAG_SKIP_SHARING_CHECK = 0x00000010, // Skips checks on file/dir creation inside a non-merged, read-only mapping. Only usable when READ_ONLY_MAPPING is set.
	BINDFLT_FLAG_CLOUD_FILES_ECPS = 0x00000020,
	BINDFLT_FLAG_NO_MULTIPLE_TARGETS = 0x00000040, // Tells bindflt to fail mapping with STATUS_INVALID_PARAMETER if a mapping produces multiple targets.
	BINDFLT_FLAG_IMMUTABLE_BACKING = 0x00000080, // Turns on caching by asserting that the backing store for name mappings is immutable.
	BINDFLT_FLAG_PREVENT_CASE_SENSITIVE_BINDING = 0x00000100,
	BINDFLT_FLAG_EMPTY_VIRT_ROOT = 0x00000200, // Tells bindflt to fail with STATUS_OBJECT_PATH_NOT_FOUND when a mapping is being added but its parent paths (ancestors) have not already been added.
	BINDFLT_FLAG_NO_REPARSE_ON_ROOT = 0x10000000,
	BINDFLT_FLAG_BATCHED_REMOVE_MAPPINGS = 0x20000000,
} BINDFLT_SETUP_FILTER_FLAGS;

typedef enum _BINDFLT_GET_MAPPINGS_FLAGS
{
	BINDFLT_GET_MAPPINGS_FLAG_VOLUME = 0x00000001,
	BINDFLT_GET_MAPPINGS_FLAG_SILO = 0x00000002,
	BINDFLT_GET_MAPPINGS_FLAG_USER = 0x00000004,
} BINDFLT_GET_MAPPINGS_FLAGS;

typedef struct _BINDFLT_GET_MAPPINGS_TARGET_ENTRY
{
	ULONG TargetRootLength;
	ULONG TargetRootOffset;
} BINDFLT_GET_MAPPINGS_TARGET_ENTRY, *PBINDFLT_GET_MAPPINGS_TARGET_ENTRY;

typedef struct _BINDFLT_GET_MAPPINGS_ENTRY
{
	ULONG VirtRootLength;
	ULONG VirtRootOffset;
	ULONG Flags;
	ULONG NumberOfTargets;
	ULONG TargetEntriesOffset;
} BINDFLT_GET_MAPPINGS_ENTRY, *PBINDFLT_GET_MAPPINGS_ENTRY;

typedef struct _BINDFLT_GET_MAPPINGS_INFO
{
	ULONG Size; // Returned size of this output structure
	LONG Status; // NTSTATUS result
	ULONG MappingCount; // Number of BINDFLT_GET_MAPPINGS_ENTRY elements
	BINDFLT_GET_MAPPINGS_ENTRY Entries[0];
} BINDFLT_GET_MAPPINGS_INFO, *PBINDFLT_GET_MAPPINGS_INFO;

HRESULT
WINAPI
BfAttachFilter(
	_In_ LPCWSTR DosPath,
	_Out_opt_ PBOOL FilterAttached
);

HRESULT
WINAPI
BfSetupFilter(
	_In_opt_ HANDLE JobHandle,
	_In_ ULONG Flags,
	_In_ LPCWSTR VirtualizationRootPath,
	_In_ LPCWSTR VirtualizationTargetPath,
	_In_reads_opt_( VirtualizationExceptionPathCount ) LPCWSTR* VirtualizationExceptionPaths,
	_In_opt_ ULONG VirtualizationExceptionPathCount
);

HRESULT
WINAPI
BfSetupFilterEx(
	_In_ ULONG Flags,
	_In_opt_ HANDLE JobHandle,
	_In_opt_ PSID Sid,
	_In_ LPCWSTR VirtualizationRootPath,
	_In_ LPCWSTR VirtualizationTargetPath,
	_In_reads_opt_( VirtualizationExceptionPathCount ) LPCWSTR* VirtualizationExceptionPaths,
	_In_opt_ ULONG VirtualizationExceptionPathCount
);

HRESULT
WINAPI
BfSetupFilterBatched(
	_In_opt_ HANDLE JobHandle,
	_In_opt_ PSID Sid,
	_In_ LPVOID BatchedConfigBuffer,
	_In_ ULONG BatchedConfigBufferSize,
	_In_ ULONG Flags,
	_In_reads_opt_( AttachFilterPathCount ) LPCWSTR* AttachFilterPaths,
	_In_opt_ ULONG AttachFilterPathCount
);

HRESULT
WINAPI
BfRemoveMapping(
	_In_opt_ HANDLE JobHandle,
	_In_ LPCWSTR VirtualizationRootPath
);

HRESULT
WINAPI
BfRemoveMappingEx(
	_In_opt_ HANDLE JobHandle,
	_In_opt_ PSID Sid,
	_In_ LPCWSTR VirtualizationRootPath
);

HRESULT
WINAPI
BfGetMappings(
	_In_ ULONG Flags,
	_In_opt_ HANDLE JobHandle,
	_In_opt_ LPCWSTR VirtualizationRootPath,
	_In_opt_ PSID Sid,
	_Inout_ PULONG BufferSize,
	_Out_opt_ LPVOID OutBuffer
);

HRESULT
WINAPI
BfGenerateBatchedConfig(
	_In_reads_bytes_( VirtualizationConfigSize ) LPCWSTR VirtualizationConfig,
	_In_ ULONG VirtualizationConfigSize,
	_Out_ LPVOID BatchedConfigBuffer,
	_Inout_ PULONG BatchedConfigBufferSize
);

#ifdef __cplusplus
}
#endif

#endif // _BINDFLTAPI_
