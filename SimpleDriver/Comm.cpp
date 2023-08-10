#include "comm.h"
#include "Imports.h"
#include "Private.h"
#include "Utils.h"
#include "KbdMou.h"
#include "skCrypter.h"
//#include "CallStack-Spoofer.h"
#include <intrin.h>


namespace Comm
{
	enum OPERATION
	{
		ReadMem = 0x801,
		WriteMem,
		AllocMem,
		ProtectMem,
		ModuleBase,
		CodeInject,
		MouseInput = 0x901,
		KeyboardInput,
	};

#pragma pack(push, 8)
	typedef struct _COPY_MEMORY {
		HANDLE ProcessId;
		PVOID VirtualAddress;
		PVOID Buffer;
		SIZE_T Size;
	} COPY_MEMORY, * PCOPY_MEMORY;

	typedef struct _ALLOC_MEMORY {
		HANDLE ProcessId;
		PVOID Base;
		SIZE_T Size;
	} ALLOC_MEMORY, * PALLOC_MEMORY;

	typedef struct _MODULE_BASE {
		HANDLE ProcessId;
		PCHAR ModuleName;
		PVOID PBase;
	} MODULE_BASE, * PMODULE_BASE;

	typedef struct _CODE_INJECT {
		HANDLE ProcessId;
		PVOID Shellcode;
		SIZE_T Size;
	} CODE_INJECT, * PCODE_INJECT;
#pragma pack(pop)

	NTSTATUS Comm::RequestHandler(PREQUEST Request)
	{
		//SPOOF_FUNC
		PVOID AddrOfRet = _AddressOfReturnAddress();
		ULONG64 RetAddr = *(ULONG64*)AddrOfRet ^ 0x7FFE03007FFE0300;
		*(ULONG64*)AddrOfRet = __readmsr(0xC0000082);

		NTSTATUS status = STATUS_SUCCESS;
		PCOPY_MEMORY CopyMemoryInfo;
		PMODULE_BASE ModuleBaseInfo;

		//统一检测来自用户层的参数是否有效
		//if (!MmIsAddressValid(Request) || !MmIsAddressValid(Request->Instruction)) {
		//	return STATUS_INVALID_PARAMETER;
		//}
		if (!MmGetPhysicalAddress(Request).QuadPart || !MmGetPhysicalAddress(Request->Instruction).QuadPart) {
			*(ULONG64*)AddrOfRet = RetAddr ^ 0x7FFE03007FFE0300;
			return STATUS_INVALID_PARAMETER;
		}

		switch (Request->Operation)
		{
		case OPERATION::ReadMem:
			CopyMemoryInfo = (PCOPY_MEMORY)Request->Instruction;
			status = Private::ReadProcessMemory(
				CopyMemoryInfo->ProcessId,
				CopyMemoryInfo->VirtualAddress,
				CopyMemoryInfo->Buffer,
				CopyMemoryInfo->Size);
			//status = Private::ReadProcessMemoryByPhysical(
			//	CopyMemoryInfo->ProcessId,
			//	CopyMemoryInfo->VirtualAddress,
			//	CopyMemoryInfo->Buffer,
			//	CopyMemoryInfo->Size);
			break;
		case OPERATION::WriteMem:
			CopyMemoryInfo = (PCOPY_MEMORY)Request->Instruction;
			status = Private::WriteProcessMemory(
				CopyMemoryInfo->ProcessId,
				CopyMemoryInfo->VirtualAddress,
				CopyMemoryInfo->Buffer,
				CopyMemoryInfo->Size);
			//status = Private::WriteProcessMemoryByPhysical(
			//	CopyMemoryInfo->ProcessId,
			//	CopyMemoryInfo->VirtualAddress,
			//	CopyMemoryInfo->Buffer,
			//	CopyMemoryInfo->Size);
			break;
		case OPERATION::AllocMem:
			break;
		case OPERATION::ProtectMem:
			break;
		case OPERATION::ModuleBase:
			ModuleBaseInfo = (PMODULE_BASE)Request->Instruction;
			UNICODE_STRING usModuleName;
			Utils::RtlCaptureAnsiString(&usModuleName, ModuleBaseInfo->ModuleName, TRUE);
			ModuleBaseInfo->PBase = Private::GetProcessModule(ModuleBaseInfo->ProcessId, &usModuleName);
			status = STATUS_SUCCESS;
			break;
		case OPERATION::CodeInject:
			break;
		case OPERATION::MouseInput:
			//status = KbdMou::SendMouseInput((PMOUSE_INPUT_DATA)Request->Instruction);
			status = KbdMou::SafeSendMouseInput((PMOUSE_INPUT_DATA)Request->Instruction);
			break;
		case OPERATION::KeyboardInput:
			//status = KbdMou::SendKeyboardInput((PKEYBOARD_INPUT_DATA)Request->Instruction);
			status = KbdMou::SafeSendKeyboardInput((PKEYBOARD_INPUT_DATA)Request->Instruction);
			break;
		default:
			status = STATUS_NOT_SUPPORTED;
			break;
		}
		*(ULONG64*)AddrOfRet = RetAddr ^ 0x7FFE03007FFE0300;
		return status;
	}

	namespace DeviceIoControl
	{
		UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(LR"(\Device\SimpleDriver)");
		UNICODE_STRING SymbolicName = RTL_CONSTANT_STRING(LR"(\??\SimpleDriver)");
		PDEVICE_OBJECT CommDeviceObject;

		NTSTATUS DispatchIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
		{
			UNREFERENCED_PARAMETER(DeviceObject);

			NTSTATUS status = STATUS_SUCCESS;
			PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);

			switch (Stack->Parameters.DeviceIoControl.IoControlCode)
			{
			case 0x9090990A:
				status = RequestHandler((PREQUEST)Irp->AssociatedIrp.SystemBuffer);
				break;
			default:
				status = STATUS_INVALID_DEVICE_REQUEST;
				break;
			}

			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = 0;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			return status;
		}

		NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
		{
			UNREFERENCED_PARAMETER(DeviceObject);

			Irp->IoStatus.Status = STATUS_SUCCESS;
			Irp->IoStatus.Information = 0;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			return STATUS_SUCCESS;
		}

		NTSTATUS Initialize(PDRIVER_OBJECT DriverObject)
		{
			NTSTATUS status;

			DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
			DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;
			DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIoControl;
			
			status = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &CommDeviceObject);
			if (!NT_SUCCESS(status))
			{
				return status;
			}
			CommDeviceObject->Flags |= DO_BUFFERED_IO;

			status = IoCreateSymbolicLink(&SymbolicName, &DeviceName);
			if (!NT_SUCCESS(status))
			{
				IoDeleteDevice(CommDeviceObject);
				return status;
			}
			return STATUS_SUCCESS;
		}

		NTSTATUS Unload()
		{
			NTSTATUS status;
			status = IoDeleteSymbolicLink(&SymbolicName);
			if (CommDeviceObject)
			{
				IoDeleteDevice(CommDeviceObject);
			}
			return status;
		}
	}

	namespace IrpHijack
	{
		PDRIVER_DISPATCH OriginDeviceControl = NULL;

		NTSTATUS DetourDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
		{
			PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
			if (stack->Parameters.DeviceIoControl.IoControlCode == 0x9090990A) {
				NTSTATUS status = RequestHandler((PREQUEST)Irp->AssociatedIrp.SystemBuffer);

				Irp->IoStatus.Status = status;	//设置IRP处理成功->告诉三环成功
				Irp->IoStatus.Information = 0;	//返回数据字节数
				IoCompleteRequest(Irp, IO_NO_INCREMENT);	//结束IRP处理流程

				return status;
			}
			return OriginDeviceControl(DeviceObject, Irp);
		}

		NTSTATUS Initialize()
		{
			PDRIVER_OBJECT DriverObject = Utils::GetDriverObjectByName(skCrypt(L"pci"));
			if (!DriverObject) {
				return STATUS_NOT_FOUND;
			}
			OriginDeviceControl = DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];
			
			UCHAR ShellCode[] = {           //make a jmp
				0x50,															//push rax
				0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		//mov  rax, 0xffffffffffffffff
				0x48, 0xC1, 0xC8, 0x28,											//ror  rax, 0x00	;decrypt real address
				0x48, 0x87, 0x04, 0x24,											//xchg qword [rsp], rax
				0xC3															//ret
			};

			auto CodeCave = (PVOID)Utils::FindPattern((ULONG64)DriverObject->DriverStart, 0x1000000,
				skCrypt("CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC"));
			if (!CodeCave) {
				return STATUS_INSUFFICIENT_RESOURCES;
			}

			//UCHAR random = (ULONG64)CodeCave >> 16 & 0xFF;
			UCHAR random = (ULONG64)CodeCave & 0xFF;
			*(UCHAR*)(ShellCode + 0xE) = random;
			*(ULONG64*)(ShellCode + 3) = Utils::__ROL__((ULONG64)DetourDeviceControl, random);

			if (!NT_SUCCESS(Utils::WriteToReadOnly(CodeCave, ShellCode, sizeof(ShellCode)))) {
				return STATUS_ACCESS_VIOLATION;
			}
			//InterlockedExchangePointer((PVOID*)&DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL], DetourDeviceControl);
			InterlockedExchangePointer((PVOID*)&DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL], CodeCave);
			return STATUS_SUCCESS;
		}

		NTSTATUS Unload()
		{
			UCHAR Dummy[] = { 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
							 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
							 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC };

			if (OriginDeviceControl) {
				PDRIVER_OBJECT DriverObject = Utils::GetDriverObjectByName(skCrypt(L"pci"));
				PVOID CodeCave = DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];
				PDEVICE_OBJECT DeviceObject = DriverObject->DeviceObject;

				KIRQL Irql = KeAcquireQueuedSpinLock(LockQueueIoDatabaseLock);
				ULONG OldExtensionFlags = DeviceObject->DeviceObjectExtension->ExtensionFlags;
				DeviceObject->DeviceObjectExtension->ExtensionFlags |= 2;	//DOE_DELETE_PENDING
				while (DeviceObject->ReferenceCount) {
					YieldProcessor();	//Make sure not reference to DeviceObject
				}
				InterlockedExchangePointer((PVOID*)&DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL], OriginDeviceControl);
				DeviceObject->DeviceObjectExtension->ExtensionFlags = OldExtensionFlags;
				KeReleaseQueuedSpinLock(LockQueueIoDatabaseLock, Irql);
				Utils::WriteToReadOnly(CodeCave, Dummy, sizeof(Dummy));
			}
			return STATUS_SUCCESS;
		}
	}

}
