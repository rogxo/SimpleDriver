#include "Private.h"
#include "Imports.h"
#include <intrin.h>
#include "ia32.h"


ULONG64 PTE_BASE;
ULONG64 PDE_BASE;
ULONG64 PPE_BASE;
ULONG64 PXE_BASE;

namespace Private
{
	NTSTATUS ReadProcessMemory(HANDLE ProcessId, PVOID VitualAddress, PVOID Buffer, SIZE_T Size)
	{
		NTSTATUS status;
		PEPROCESS Process;
		SIZE_T SizeRead;
		
		if (VitualAddress > MmHighestUserAddress || Buffer > MmHighestUserAddress
			|| (ULONG64)VitualAddress + Size > (ULONG64)MmHighestUserAddress
			|| (ULONG64)Buffer + Size > (ULONG64)MmHighestUserAddress){
			return STATUS_INVALID_PARAMETER;
		}
		status = PsLookupProcessByProcessId(ProcessId, &Process);
		if (!NT_SUCCESS(status)) {
			return status;
		}
		ObDereferenceObject(Process);

		status = MmCopyVirtualMemory(Process, VitualAddress, PsGetCurrentProcess(), Buffer, Size, KernelMode, &SizeRead);	
		return status;
	}

	NTSTATUS WriteProcessMemory(HANDLE ProcessId, PVOID VitualAddress, PVOID Buffer, SIZE_T Size)
	{
		NTSTATUS status;
		PEPROCESS Process;
		SIZE_T SizeRead;

		if (VitualAddress > MmHighestUserAddress || Buffer > MmHighestUserAddress
			|| (ULONG64)VitualAddress + Size > (ULONG64)MmHighestUserAddress
			|| (ULONG64)Buffer + Size > (ULONG64)MmHighestUserAddress) {
			return STATUS_INVALID_PARAMETER;
		}
		status = PsLookupProcessByProcessId(ProcessId, &Process);
		if (!NT_SUCCESS(status)) {
			return status;
		}
		ObDereferenceObject(Process);

		status = MmCopyVirtualMemory(PsGetCurrentProcess(), Buffer, Process, VitualAddress, Size, KernelMode, &SizeRead);
		return status;
	}

	PVOID GetProcessModule(HANDLE ProcessId, PUNICODE_STRING ModuleName)
	{
		NTSTATUS status;
		PEPROCESS Process;
		KAPC_STATE ApcState;
		PVOID ModuleBase = NULL;

		status = PsLookupProcessByProcessId(ProcessId, &Process);
		if (!NT_SUCCESS(status)) {
			return NULL;
		}
		ObDereferenceObject(Process);

		KeStackAttachProcess(Process, &ApcState);
		PPEB32 pPeb32 = (PPEB32)PsGetProcessWow64Process(Process);
		if (pPeb32)
		{
			for (PLIST_ENTRY32 pListEntry = (PLIST_ENTRY32)((PPEB_LDR_DATA32)pPeb32->Ldr)->InMemoryOrderModuleList.Flink;
				pListEntry != &((PPEB_LDR_DATA32)pPeb32->Ldr)->InMemoryOrderModuleList;
				pListEntry = (PLIST_ENTRY32)pListEntry->Flink)
			{
				PLDR_DATA_TABLE_ENTRY32 LdrEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY32, InMemoryOrderLinks);
				UNICODE_STRING usCurrentName = { 0 };
				RtlInitUnicodeString(&usCurrentName, (PWCHAR)LdrEntry->BaseDllName.Buffer);
				if (RtlEqualUnicodeString(&usCurrentName, ModuleName, TRUE)) {
					ModuleBase = (PVOID)LdrEntry->DllBase;
					break;
				}
			}
		}
		else
		{
			PPEB pPeb = PsGetProcessPeb(Process);
			for (PLIST_ENTRY pListEntry = pPeb->Ldr->InMemoryOrderModuleList.Flink;
				pListEntry != &pPeb->Ldr->InMemoryOrderModuleList;
				pListEntry = pListEntry->Flink)
			{
				PLDR_DATA_TABLE_ENTRY pEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
				//DbgPrint("[SimpleDriver] %wZ\n", pEntry->BaseDllName);
				if (RtlEqualUnicodeString(&pEntry->BaseDllName, ModuleName, TRUE)) {
					ModuleBase = pEntry->DllBase;
					break;
				}
			}
		}
		KeUnstackDetachProcess(&ApcState);
		return ModuleBase;
	}

	ULONG64 GetDirectoryTableBase(PEPROCESS Process)
	{
		if (!Process) {
			return 0;
		}

		ULONG64 DirectoryTableBase = *(PULONG64)((PUCHAR)Process + 0x28);
		if (DirectoryTableBase == 0)
		{
			//auto userdiroffset = getoffsets();
			//ULONG_PTR process_userdirbase = *(PULONG_PTR)(process + userdiroffset);
			//return process_userdirbase;
		}
		return DirectoryTableBase;
	}


	static constexpr ULONG64 mask = (~0xfull << 8) & 0xfffffffffull;
	ULONG64 TranslateAddress(ULONG64 DirectoryTableBase, ULONG64 VirtualAddress)
	{
		SIZE_T SizeRead;	// Not used dummy
		DirectoryTableBase &= ~0xf;	//PML4 table base

		ULONG64 PageOffset = VirtualAddress & ~(~0ul << 12);	// Offset in physica page
		ULONG64 PteIndex = ((VirtualAddress >> 12) & (0x1ffll));	// Index of PTE on Page table
		ULONG64 PdeIndex = ((VirtualAddress >> 21) & (0x1ffll));	// Index of PDE on Page-Directory
		ULONG64 PpeIndex = ((VirtualAddress >> 30) & (0x1ffll));	// Index of PDPTE on Page-Directory Pointer Table
		ULONG64 PxeIndex = ((VirtualAddress >> 39) & (0x1ffll));	// Index of PML4E on PML4

		ULONG64 Pxe = 0;
		ReadPhysicalAddress(DirectoryTableBase + 8 * PxeIndex, &Pxe, sizeof(PVOID), &SizeRead);
		if (~Pxe & 1) {
			return 0;	// Not preset
		}
		ULONG64 Ppe = 0;
		ReadPhysicalAddress((Pxe & mask) + 8 * PpeIndex, &Ppe, sizeof(PVOID), &SizeRead);
		if (~Ppe & 1) {
			return 0;	// Not preset
		}
		if (Ppe & 0x80) {
			return (Ppe & (~0ull << 42 >> 12)) + (VirtualAddress & ~(~0ull << 30));	// 1GB large page
		}
		ULONG64 Pde = 0;
		ReadPhysicalAddress((Ppe & mask) + 8 * PdeIndex, &Pde, sizeof(PVOID), &SizeRead);
		if (~Pde & 1) {
			return 0;	// Not preset
		}
		if (Pde & 0x80) {
			return (Pde & mask) + (VirtualAddress & ~(~0ull << 21));	// 2MB large page
		}
		ULONG64 Pte = 0;
		ReadPhysicalAddress((Pde & mask) + 8 * PteIndex, &Pte, sizeof(PVOID), &SizeRead);
		ULONG64 PageAddress = Pte & mask;	//Page physical address
		if (!PageAddress) {
			return 0;
		}
		return PageAddress + PageOffset;
	}

	/*
	//实际编译后生成的汇编代码不如上面直接进行逻辑运算速度快
	ULONG64 TranslateAddress(ULONG64 DirectoryTableBase, ULONG64 VirtualAddress)
	{
		SIZE_T SizeRead;	// Not used dummy
		DirectoryTableBase &= ~0xf;	//PML4 table base
		
		ULONG64 PageOffset = VirtualAddress & ~(~0ul << 12);	// Offset in physica page
		ULONG64 PteIndex = ((VirtualAddress >> 12) & (0x1ffll));	// Index of PTE on Page table
		ULONG64 PdeIndex = ((VirtualAddress >> 21) & (0x1ffll));	// Index of PDE on Page-Directory
		ULONG64 PpeIndex = ((VirtualAddress >> 30) & (0x1ffll));	// Index of PDPTE on Page-Directory Pointer Table
		ULONG64 PxeIndex = ((VirtualAddress >> 39) & (0x1ffll));	// Index of PML4E on PML4

		PML4E_64 Pxe;
		ReadPhysicalAddress(DirectoryTableBase + 8 * PxeIndex, &Pxe, sizeof(PVOID), &SizeRead);
		if (!Pxe.Present) {
			return 0;
		}
		PDPTE_64 Ppe;
		ReadPhysicalAddress((Pxe.PageFrameNumber << PDPTE_64_PAGE_FRAME_NUMBER_BIT) + 8 * PpeIndex, &Ppe, sizeof(PVOID), &SizeRead);
		if (!Ppe.Present) {
			return 0;
		}
		if (Ppe.LargePage) {
			PDPTE_1GB_64 Ppe_1G;	// 1GB large page
			Ppe_1G.AsUInt = Ppe.AsUInt;
			return (Ppe_1G.PageFrameNumber << PDPTE_1GB_64_PAGE_FRAME_NUMBER_BIT) + (VirtualAddress & 0x3FFFFFFF);
		}
		PDE_64 Pde;
		ReadPhysicalAddress((Ppe.PageFrameNumber << PDPTE_64_PAGE_FRAME_NUMBER_BIT) + 8 * PdeIndex, &Pde, sizeof(PVOID), &SizeRead);
		if (!Pde.Present) {
			return 0;
		}
		if (Pde.LargePage) {
			PDE_2MB_64 Pde_2M;	// 2MB large page
			Pde_2M.AsUInt = Pde.AsUInt;
			return (Pde_2M.PageFrameNumber << PDE_2MB_64_PAGE_FRAME_NUMBER_BIT) + (VirtualAddress & 0x1FFFFF);
		}
		PTE_64 Pte;
		ReadPhysicalAddress((Pde.PageFrameNumber << PDE_64_PAGE_FRAME_NUMBER_BIT) + 8 * PteIndex, &Pte, sizeof(PVOID), &SizeRead);
		if (!Pte.Present) {
			return 0;
		}
		return (Pte.PageFrameNumber << PTE_64_PAGE_FRAME_NUMBER_BIT) + PageOffset;
	}
	*/

	//调用之前一定要检查Buffer的Pte->Present,缺页的Buffer会因为IRQL过高直接蓝
	NTSTATUS ReadPhysicalAddress(ULONG64 PhysicalAddress, PVOID Buffer, SIZE_T Size, SIZE_T* SizeRead)
	{
		MM_COPY_ADDRESS Addr = { 0 };
		Addr.PhysicalAddress.QuadPart = PhysicalAddress;
		return MmCopyMemory(Buffer, Addr, Size, MM_COPY_MEMORY_PHYSICAL, SizeRead);
	}

	NTSTATUS WritePhysicalAddress(ULONG64 PhysicalAddress, PVOID Buffer, SIZE_T Size, SIZE_T* SizeWritten)
	{
		PHYSICAL_ADDRESS Addr = { 0 };
		Addr.QuadPart = PhysicalAddress;
		PVOID MemoryMapped = MmMapIoSpaceEx(Addr, Size, PAGE_READWRITE);
		if (!MemoryMapped) {
			return STATUS_UNSUCCESSFUL;
		}
		memcpy(MemoryMapped, Buffer, Size);
		*SizeWritten = Size;
		MmUnmapIoSpace(MemoryMapped, Size);
		return STATUS_SUCCESS;
	}

	NTSTATUS ReadProcessMemoryByPhysical(HANDLE ProcessId, PVOID VitualAddress, PVOID Buffer, SIZE_T Size)
	{
		NTSTATUS status;
		PEPROCESS Process;

		if (VitualAddress > MmHighestUserAddress || Buffer > MmHighestUserAddress
			|| (ULONG64)VitualAddress + Size > (ULONG64)MmHighestUserAddress
			|| (ULONG64)Buffer + Size > (ULONG64)MmHighestUserAddress) {
			return STATUS_INVALID_PARAMETER;
		}
		status = PsLookupProcessByProcessId(ProcessId, &Process);
		if (!NT_SUCCESS(status)) {
			return status;
		}
		ObDereferenceObject(Process);

		ULONG64 DirectoryTableBase = GetDirectoryTableBase(Process);
		SIZE_T LeftToMove = Size;	//剩余读取字节数
		SIZE_T CurrentReadOffset = 0;

		PVOID TmpBuffer = ExAllocatePool(NonPagedPool, Size);	//从非分页池搞点永远不会缺页的内存出来
		if (!TmpBuffer) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		while (LeftToMove > 0)
		{
			ULONG64 PhysicalAddress = TranslateAddress(DirectoryTableBase, (ULONG64)VitualAddress + CurrentReadOffset);
			if (!PhysicalAddress) {
				//return STATUS_PARTIAL_COPY;
				break;
			}
			SIZE_T AmountToMove = min(PAGE_SIZE - (PhysicalAddress & 0xFFF), LeftToMove);	//需要读取的字节数
			SIZE_T SizeRead;	//实际读取的字节数
			
			PVOID ReadBuffer = (PVOID)((ULONG64)TmpBuffer + CurrentReadOffset);
			status = ReadPhysicalAddress(PhysicalAddress, ReadBuffer, AmountToMove, &SizeRead);
			if (!NT_SUCCESS(status) || SizeRead < AmountToMove) {
				break;
			}
			LeftToMove -= AmountToMove;
			CurrentReadOffset += AmountToMove;
		}
		//调用这个，是因为它会自动处理页异常，地址无效的时候也不会蓝，MmCopyMemory去Copy虚拟地址碰到缺页的时候会因为IRQL过高无法换页
		SIZE_T SizeCopied;
		status = MmCopyVirtualMemory(PsGetCurrentProcess(), TmpBuffer, PsGetCurrentProcess(), Buffer, Size, KernelMode, &SizeCopied);
		ExFreePool(TmpBuffer);
		if (!NT_SUCCESS(status) || CurrentReadOffset != Size) {
			return STATUS_UNSUCCESSFUL;
		}
		return STATUS_SUCCESS;
	}

	NTSTATUS WriteProcessMemoryByPhysical(HANDLE ProcessId, PVOID VitualAddress, PVOID Buffer, SIZE_T Size)
	{
		NTSTATUS status;
		PEPROCESS Process;

		if (VitualAddress > MmHighestUserAddress || Buffer > MmHighestUserAddress
			|| (ULONG64)VitualAddress + Size > (ULONG64)MmHighestUserAddress
			|| (ULONG64)Buffer + Size > (ULONG64)MmHighestUserAddress) {
			return STATUS_INVALID_PARAMETER;
		}
		status = PsLookupProcessByProcessId(ProcessId, &Process);
		if (!NT_SUCCESS(status)) {
			return status;
		}
		ObDereferenceObject(Process);

		ULONG64 DirectoryTableBase = GetDirectoryTableBase(Process);
		SIZE_T LeftToMove = Size;	//剩余读取字节数
		SIZE_T CurrentWriteOffset = 0;

		PVOID TmpBuffer = ExAllocatePool(NonPagedPool, Size);
		if (!TmpBuffer) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		SIZE_T SizeCopied;
		status = MmCopyVirtualMemory(PsGetCurrentProcess(), Buffer, PsGetCurrentProcess(), TmpBuffer, Size, KernelMode, &SizeCopied);
		if (!NT_SUCCESS(status)) {
			ExFreePool(TmpBuffer);
			return status;
		}
		while (LeftToMove > 0)
		{
			ULONG64 PhysicalAddress = TranslateAddress(DirectoryTableBase, (ULONG64)VitualAddress + CurrentWriteOffset);
			if (!PhysicalAddress) {
				break;
			}
			SIZE_T AmountToMove = min(PAGE_SIZE - (PhysicalAddress & 0xFFF), LeftToMove);	//需要读取的字节数
			SIZE_T SizeRead;	//实际读取的字节数

			PVOID WriteBuffer = (PVOID)((ULONG64)TmpBuffer + CurrentWriteOffset);
			status = WritePhysicalAddress(PhysicalAddress, WriteBuffer, AmountToMove, &SizeRead);
			if (!NT_SUCCESS(status) || SizeRead < AmountToMove) {
				break;
			}
			LeftToMove -= AmountToMove;
			CurrentWriteOffset += AmountToMove;
		}
		ExFreePool(TmpBuffer);
		if (CurrentWriteOffset != Size) {
			return STATUS_UNSUCCESSFUL;
		}
		return STATUS_SUCCESS;
	}

	VOID InitializePageBase()
	{
		ULONG64 dirbase = __readcr3();
		PHYSICAL_ADDRESS phAddr = { 0 };
		ULONG64 slot = 0;
		ULONG_PTR pfn = dirbase >> 12;

		phAddr.QuadPart = pfn << PAGE_SHIFT;
		PHARDWARE_PTE pml4 = (PHARDWARE_PTE)MmGetVirtualForPhysical(phAddr);

		while (pml4[slot].PageFrameNumber != pfn) slot++;

		PTE_BASE = (slot << 39) + 0xFFFF000000000000;
		PDE_BASE = PTE_BASE + (slot << 30);
		PPE_BASE = PDE_BASE + (slot << 21);
		PXE_BASE = PPE_BASE + (slot << 12);
		return;
	}
}
