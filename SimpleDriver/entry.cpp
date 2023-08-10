#include <ntifs.h>
#include "Comm.h"
#include "Private.h"
#include "Utils.h"
#include "Imports.h"
#include "MpKslDrv.h"
//#include "Sysmon.h"
#include "KbdMou.h"
#include "skCrypter.h"


EXTERN_C NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status = STATUS_SUCCESS;
	KdPrint((skCrypt("[SimpleDriver] DriverEntry\n")));

	if (DriverObject)
	{
		DriverObject->DriverUnload = [](PDRIVER_OBJECT DriverObject)->VOID {
			UNREFERENCED_PARAMETER(DriverObject);
			KdPrint((skCrypt("[SimpleDriver] DriverUnload\n")));

			//Comm::DeviceIoControl::Unload();
			Comm::IrpHijack::Unload();
		};
		//status = Comm::DeviceIoControl::Initialize(DriverObject);
	}
	status = KbdMou::InitializeKbdMou();
	if (!NT_SUCCESS(status)) {
		KdPrint((skCrypt("[SimpleDriver] InitializeKbdMou failed status:%X\n"), status));
	}
	status = Comm::IrpHijack::Initialize();

	//KbdMou::TestMouse();
	//KbdMou::TestKeyboard();

	return status;
}
 