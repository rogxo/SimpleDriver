/*++

Copyright (c) 2020-2025, Rog. All rights reserved.

Author:
	Rog

License:
	MIT

--*/
#include "UmComm.h"
#include "ntdll.h"


namespace UsermodeComm {

	namespace DeviceIoControl {
		HANDLE hDevice;

		bool Initialize() {
			hDevice = CreateFileA(R"(\\.\SimpleDriver)", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (!hDevice || hDevice == INVALID_HANDLE_VALUE) {
				return false;
			}
			return true;
		}
		bool Request(PREQUEST req) {
			return ::DeviceIoControl(hDevice, 0x9090990A, req, sizeof(REQUEST), NULL, 0, NULL, NULL);
		}
		bool Unload() {
			return CloseHandle(hDevice);
		}
	}

	namespace IrpHijack {
		HANDLE hDevice;

		bool Initialize() {
			hDevice = CreateFileA(R"(\\.\PciControl)", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (!hDevice || hDevice == INVALID_HANDLE_VALUE) {
				return false;
			}
			return true;
		}
		bool Request(PREQUEST req) {
			return ::DeviceIoControl(hDevice, 0x9090990A, req, sizeof(REQUEST), NULL, 0, NULL, NULL);
		}
		bool Unload() {
			return CloseHandle(hDevice);
		}
	}

	namespace SystemFirmwateTable
	{
		typedef struct _SYSTEM_FIRMWARE_TABLE_INFORMATION {
			ULONG		ProviderSignature;
			ULONG		Action;
			ULONG		TableID;
			ULONG		TableBufferLength;
			PVOID		TableBuffer;
		} SYSTEM_FIRMWARE_TABLE_INFORMATION, * PSYSTEM_FIRMWARE_TABLE_INFORMATION;

		bool Request(PREQUEST req) {
			SYSTEM_FIRMWARE_TABLE_INFORMATION SystemInformation;
			SystemInformation.ProviderSignature = 'CEAT';
			SystemInformation.Action = 0x9090990A;
			SystemInformation.TableID = 0;
			SystemInformation.TableBufferLength = sizeof(REQUEST);
			SystemInformation.TableBuffer = (PVOID)req;
			ULONG ReturnLength;
			NTSTATUS status = ZwQuerySystemInformation(SystemFirmwareTableInformation, &SystemInformation, sizeof(SYSTEM_FIRMWARE_TABLE_INFORMATION), &ReturnLength);
			return NT_SUCCESS(status);
		}
	}
}



