#pragma once
#include <ntifs.h>

namespace Private
{
	NTSTATUS ReadProcessMemory(HANDLE ProcessId, PVOID VitualAddress, PVOID Buffer, SIZE_T Size);
	NTSTATUS WriteProcessMemory(HANDLE ProcessId, PVOID VitualAddress, PVOID Buffer, SIZE_T Size);
	PVOID GetProcessModule(HANDLE ProcessId, PUNICODE_STRING ModuleName);
	
	ULONG64 GetDirectoryTableBase(PEPROCESS Process);
	ULONG64 TranslateAddress(ULONG64 DirectoryTableBase, ULONG64 VirtualAddress);
	NTSTATUS ReadPhysicalAddress(ULONG64 PhysicalAddress, PVOID Buffer, SIZE_T Size, SIZE_T* SizeRead);
	NTSTATUS WritePhysicalAddress(ULONG64 PhysicalAddress, PVOID Buffer, SIZE_T Size, SIZE_T* SizeRead);
	NTSTATUS ReadProcessMemoryByPhysical(HANDLE ProcessId, PVOID VitualAddress, PVOID Buffer, SIZE_T Size);
	NTSTATUS WriteProcessMemoryByPhysical(HANDLE ProcessId, PVOID VitualAddress, PVOID Buffer, SIZE_T Size);

	VOID InitializePageBase();
};

