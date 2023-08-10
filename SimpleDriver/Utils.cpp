#include "Utils.h"
#include <windef.h>
#include "Imports.h"

namespace Utils
{
    PVOID GetModuleBase(PCHAR szModuleName)
    {
        PVOID result = 0;
        ULONG length = 0;

        ZwQuerySystemInformation(SystemModuleInformation, &length, 0, &length);
        if (!length) {
            return result;
        }
        PSYSTEM_MODULE_INFORMATION system_modules = (PSYSTEM_MODULE_INFORMATION)ExAllocatePool(NonPagedPool, length);
        if (!system_modules) {
            return result;
        }
        NTSTATUS status = ZwQuerySystemInformation(SystemModuleInformation, system_modules, length, 0);
        if (NT_SUCCESS(status))
        {
            for (SIZE_T i = 0; i < (ULONG)system_modules->ulModuleCount; i++)
            {
                char* fileName = (char*)system_modules->Modules[i].ImageName + system_modules->Modules[i].ModuleNameOffset;
                if (!strcmp(fileName, szModuleName))
                {
                    result = system_modules->Modules[i].Base;
                    break;
                }
            }
        }
        ExFreePool(system_modules);
        return result;
    }

	BOOLEAN RtlCaptureAnsiString(PUNICODE_STRING DestinationString, PCSZ SourceString, BOOLEAN AllocateDestinationString)
	{
		ANSI_STRING ansi_string = { 0 };
		NTSTATUS status = STATUS_SUCCESS;

        if (!SourceString) {
            return FALSE;
        }
		RtlInitAnsiString(&ansi_string, SourceString);
		status = RtlAnsiStringToUnicodeString(DestinationString, &ansi_string, AllocateDestinationString);
		if (!NT_SUCCESS(status)) {
			return FALSE;
		}
		return TRUE;
	}

    PDRIVER_OBJECT GetDriverObjectByName(PWCHAR DriverName)
    {
        NTSTATUS		status;
        UNICODE_STRING	usObjectName;
        UNICODE_STRING	usFileObject;
        PDRIVER_OBJECT	DriverObject = NULL;
        WCHAR			szDriver[MAX_PATH] = L"\\Driver\\";
        WCHAR			szFileSystem[MAX_PATH] = L"\\FileSystem\\";

        wcscat(szDriver, DriverName);
        wcscat(szFileSystem, DriverName);

        RtlInitUnicodeString(&usObjectName, szDriver);
        RtlInitUnicodeString(&usFileObject, szFileSystem);

        // 有些是文件系统 "\\FileSystem\\Ntfs"  https://bbs.kanxue.com/thread-99970.htm
        status = ObReferenceObjectByName(
            &usObjectName,
            OBJ_CASE_INSENSITIVE,
            NULL,
            0,
            *IoDriverObjectType,
            KernelMode,
            NULL,
            (PVOID*)&DriverObject);

        if (!NT_SUCCESS(status))
        {
            status = ObReferenceObjectByName(
                &usFileObject,
                OBJ_CASE_INSENSITIVE,
                NULL,
                0,
                *IoDriverObjectType,
                KernelMode,
                NULL,
                (PVOID*)&DriverObject);

            if (!NT_SUCCESS(status)) {
                return NULL;
            }
        }

        ObDereferenceObject(DriverObject);
        return DriverObject;
    }

    ULONG64 FindPattern(ULONG64 base, SIZE_T size, PCHAR pattern)
    {
        //find pattern utils
        #define InRange(x, a, b) (x >= a && x <= b) 
        #define GetBits(x) (InRange(x, '0', '9') ? (x - '0') : ((x - 'A') + 0xA))
        #define GetByte(x) ((BYTE)(GetBits(x[0]) << 4 | GetBits(x[1])))

        //get module range
        PBYTE ModuleStart = (PBYTE)base;
        PBYTE ModuleEnd = (PBYTE)(ModuleStart + size);

        //scan pattern main
        PBYTE FirstMatch = nullptr;
        const char* CurPatt = pattern;
        for (; ModuleStart < ModuleEnd; ++ModuleStart)
        {
            bool SkipByte = (*CurPatt == '\?');
            if (SkipByte || *ModuleStart == GetByte(CurPatt)) {
                if (!FirstMatch) FirstMatch = ModuleStart;
                SkipByte ? CurPatt += 2 : CurPatt += 3;
                if (CurPatt[-1] == 0) return (ULONG64)FirstMatch;
            }

            else if (FirstMatch) {
                ModuleStart = FirstMatch;
                FirstMatch = nullptr;
                CurPatt = pattern;
            }
        }
        return NULL;
    }

    ULONG64 GetImageSectionByName(PVOID ImageBase, PCHAR SectionName, SIZE_T* SizeOut)
    {
        if (PIMAGE_DOS_HEADER(ImageBase)->e_magic != 0x5A4D) {
            return 0;
        }
        PIMAGE_NT_HEADERS64 NtHeader = RtlImageNtHeader(ImageBase);
        SIZE_T SectionCount = NtHeader->FileHeader.NumberOfSections;

        PIMAGE_SECTION_HEADER SectionHeader = IMAGE_FIRST_SECTION(NtHeader);
        for (size_t i = 0; i < SectionCount; ++i, ++SectionHeader) {
            if (!strcmp(SectionName, (const char*)SectionHeader->Name)) {
                if (SizeOut) {
                    *SizeOut = SectionHeader->Misc.VirtualSize;
                }
                return (ULONG64)ImageBase + SectionHeader->VirtualAddress;
            }
        }
        return 0;
    }

    ULONG64 FindPatternImage(PVOID ImageBase, PCHAR SectionName, PCHAR Pattern)
    {
        SIZE_T SectionSize = 0;
        ULONG64 SectionBase = GetImageSectionByName(ImageBase, SectionName, &SectionSize);
        if (!SectionBase) {
            return 0;
        }
        return FindPattern(SectionBase, SectionSize, Pattern);
    }

    ULONG64 FindPatternImage(PCHAR ModuleName, PCHAR SectionName, PCHAR Pattern)
    {
        PVOID ModuleBase = 0;
        SIZE_T SectionSize = 0;

        ModuleBase = GetModuleBase(ModuleName);
        if (!ModuleBase) {
            return 0;
        }
        auto SectionBase = GetImageSectionByName(ModuleBase, SectionName, &SectionSize);
        if (!SectionBase) {
            return 0;
        }
        return FindPattern(SectionBase, SectionSize, Pattern);
    }

    NTSTATUS WriteToReadOnly(PVOID destination, PVOID buffer, ULONG size)
    {
        PMDL mdl = IoAllocateMdl(destination, size, FALSE, FALSE, 0);
        if (!mdl) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        MmProbeAndLockPages(mdl, KernelMode, IoReadAccess);
        MmProtectMdlSystemAddress(mdl, PAGE_EXECUTE_READWRITE);

        auto mmMap = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmNonCached, NULL, FALSE, NormalPagePriority);
        memcpy(mmMap, buffer, size);

        MmUnmapLockedPages(mmMap, mdl);
        MmUnlockPages(mdl);
        IoFreeMdl(mdl);

        return STATUS_SUCCESS;
    }

    VOID Sleep(ULONG Milliseconds)
    {
        LARGE_INTEGER Timeout;
        Timeout.QuadPart = -1 * 10000LL * (LONGLONG)Milliseconds;
        KeDelayExecutionThread(KernelMode, FALSE, &Timeout);
    }

};

