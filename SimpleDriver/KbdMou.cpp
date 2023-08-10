#include "KbdMou.h"
#include "Imports.h"
#include "Utils.h"
#include "skCrypter.h"
//#include "CallStack-Spoofer.h"


typedef VOID(*MouseClassServiceCallback_T)(
	IN PDEVICE_OBJECT DeviceObject,
	IN PMOUSE_INPUT_DATA InputDataStart,
	IN PMOUSE_INPUT_DATA InputDataEnd,
	IN OUT PULONG InputDataConsumed);

typedef VOID(*KeyboardClassServiceCallback_T)(
	IN PDEVICE_OBJECT DeviceObject,
	IN PKEYBOARD_INPUT_DATA InputDataStart,
	IN PKEYBOARD_INPUT_DATA InputDataEnd,
	IN OUT PULONG InputDataConsumed);

namespace KbdMouTrick
{
	EXTERN_C PVOID MouHid_JmpRax = NULL;
	EXTERN_C PVOID MouHid_AddRsp28hRet = NULL;
	EXTERN_C PVOID MouHid_CopAddress = NULL;
	EXTERN_C PVOID KbdHid_JmpRax = NULL;
	EXTERN_C PVOID KbdHid_AddRsp28hRet = NULL;
	EXTERN_C VOID AsmTrickMouseCall(
		IN PDEVICE_OBJECT DeviceObject,
		IN PMOUSE_INPUT_DATA InputDataStart,
		IN PMOUSE_INPUT_DATA InputDataEnd,
		IN OUT PULONG InputDataConsumed);
	EXTERN_C VOID AsmCopMouseCall(
		IN PDEVICE_OBJECT DeviceObject,
		IN PMOUSE_INPUT_DATA InputDataStart,
		IN PMOUSE_INPUT_DATA InputDataEnd,
		IN OUT PULONG InputDataConsumed);
	EXTERN_C VOID AsmTrickKeyboardCall(
		IN PDEVICE_OBJECT DeviceObject,
		IN PKEYBOARD_INPUT_DATA InputDataStart,
		IN PKEYBOARD_INPUT_DATA InputDataEnd,
		IN OUT PULONG InputDataConsumed);

	NTSTATUS InitializeKbdMouTrick()
	{
		PVOID MouHidBase = Utils::GetModuleBase(skCrypt("mouhid.sys"));
		if (!MouHidBase) {
			return STATUS_NOT_FOUND;
		}
		MouHid_AddRsp28hRet = (PVOID)Utils::FindPatternImage(MouHidBase, skCrypt(".text"), skCrypt("48 83 C4 28 C3"));
		MouHid_JmpRax = (PVOID)Utils::FindPatternImage(MouHidBase, skCrypt(".text"), skCrypt("FF E0"));
		if (!MouHid_JmpRax) {
			MouHid_JmpRax = (PVOID)Utils::FindPatternImage(MouHidBase, skCrypt(".text"), skCrypt("50 C3"));
		}
		if (!MouHid_JmpRax) {
			MouHid_JmpRax = (PVOID)Utils::FindPatternImage(MouHidBase, skCrypt(".text"), skCrypt("CC CC CC CC CC"));
			USHORT Data = 0xE0FF;
			Utils::WriteToReadOnly(MouHid_JmpRax, &Data, 2);
		}
		MouHid_CopAddress = (PVOID)(Utils::FindPatternImage(MouHidBase, skCrypt(".text"), skCrypt("48 8B 07 B2 ? 48 8B ? ? 4C")) - 0x4B);

		KdPrint(("[SimpleDriver] MouHidBase:%p\n", MouHidBase));
		KdPrint(("[SimpleDriver] MouHid_JmpRax:%p\n", KbdMouTrick::MouHid_JmpRax));
		KdPrint(("[SimpleDriver] MouHid_AddRsp28hRet:%p\n", KbdMouTrick::MouHid_AddRsp28hRet));
		KdPrint(("[SimpleDriver] MouHid_CopAddress:%p\n", KbdMouTrick::MouHid_CopAddress));


		PVOID KbdHidBase = Utils::GetModuleBase(skCrypt("kbdhid.sys"));
		if (!KbdHidBase) {
			return STATUS_NOT_FOUND;
		}
		KbdHid_AddRsp28hRet = (PVOID)Utils::FindPatternImage(KbdHidBase, skCrypt(".text"), skCrypt("48 83 C4 28 C3"));
		KbdHid_JmpRax = (PVOID)Utils::FindPatternImage(KbdHidBase, skCrypt(".text"), skCrypt("FF E0"));
		if (!KbdHid_JmpRax) {
			KbdHid_JmpRax = (PVOID)Utils::FindPatternImage(KbdHidBase, skCrypt(".text"), skCrypt("50 C3"));
		}
		if (!KbdHid_JmpRax) {
			KbdHid_JmpRax = (PVOID)Utils::FindPatternImage(KbdHidBase, skCrypt(".text"), skCrypt("CC CC CC CC CC"));
			USHORT Data = 0xE0FF;
			Utils::WriteToReadOnly(KbdHid_JmpRax, &Data, 2);
		}
		
		//KdPrint(("[SimpleDriver] KbdHidBase:%p\n", KbdHidBase));
		//KdPrint(("[SimpleDriver] KbdHid_JmpRax:%p\n", KbdMouTrick::KbdHid_JmpRax));
		//KdPrint(("[SimpleDriver] KbdHid_AddRsp28hRet:%p\n", KbdMouTrick::KbdHid_AddRsp28hRet));

		return STATUS_SUCCESS;
	}
}

namespace KbdMou
{
	// ¼üÊó³éÏóÀà
	UNICODE_STRING KbdClassDriverName = RTL_CONSTANT_STRING(L"\\Driver\\kbdclass");
	UNICODE_STRING MouClassDriverName = RTL_CONSTANT_STRING(L"\\Driver\\mouclass");
	// PS/2-8042
	UNICODE_STRING i8042prtDriverName = RTL_CONSTANT_STRING(L"\\Driver\\i8042prt");
	// USB-Hid
	UNICODE_STRING MouHIDDriverName = RTL_CONSTANT_STRING(L"\\Driver\\mouhid");
	UNICODE_STRING KbdHIDDriverName = RTL_CONSTANT_STRING(L"\\Driver\\kbdhid");

	PDEVICE_OBJECT MouDeviceObject = NULL;
	EXTERN_C MouseClassServiceCallback_T MouseClassServiceCallback = NULL;
	PDEVICE_OBJECT KbdDeviceObject = NULL;
	EXTERN_C KeyboardClassServiceCallback_T KeyboardClassServiceCallback = NULL;

	NTSTATUS InitializeMouse()
	{
		NTSTATUS status;
		PDRIVER_OBJECT MouClassDriverObject;
		PDRIVER_OBJECT MouHIDDriverObject;

		status = ObReferenceObjectByName(&MouClassDriverName, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&MouClassDriverObject);
		if (!NT_SUCCESS(status)) {
			return status;
		}
		status = ObReferenceObjectByName(&MouHIDDriverName, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&MouHIDDriverObject);
		if (!NT_SUCCESS(status))
		{
			if (MouHIDDriverObject) {
				ObfDereferenceObject(MouClassDriverObject);
			}
			return status;
		}

		PDEVICE_OBJECT MouHIDDeviceObject = MouHIDDriverObject->DeviceObject;

		while (MouHIDDeviceObject && !MouseClassServiceCallback)
		{
			PDEVICE_OBJECT MouClassDeviceObject = MouClassDriverObject->DeviceObject;
			while (MouClassDeviceObject && !MouseClassServiceCallback)
			{
				if (!MouClassDeviceObject->NextDevice && !MouDeviceObject) {
					MouDeviceObject = MouClassDeviceObject;
				}
				PULONG64 MouHIDDeviceExtensionStart = (PULONG64)MouHIDDeviceObject->DeviceExtension;
				ULONG64 MouHIDDeviceExtensionSize = ((ULONG64)MouHIDDeviceObject->DeviceObjectExtension - (ULONG64)MouHIDDeviceObject->DeviceExtension) / 4;

				for (ULONG64 i = 0; i < MouHIDDeviceExtensionSize; i++)
				{
					if (MouHIDDeviceExtensionStart[i] == (ULONG64)MouClassDeviceObject && MouHIDDeviceExtensionStart[i + 1] > (ULONG64)MouClassDriverObject->DriverStart)
					{
						MouseClassServiceCallback = (MouseClassServiceCallback_T)(MouHIDDeviceExtensionStart[i + 1]);
						break;
					}
				}
				MouClassDeviceObject = MouClassDeviceObject->NextDevice;
			}
			MouHIDDeviceObject = MouHIDDeviceObject->AttachedDevice;
		}
		if (!MouDeviceObject)
		{
			PDEVICE_OBJECT TmpDeviceObject = MouClassDriverObject->DeviceObject;
			while (TmpDeviceObject)
			{
				if (!TmpDeviceObject->NextDevice)
				{
					MouDeviceObject = TmpDeviceObject;
					break;
				}
				TmpDeviceObject = TmpDeviceObject->NextDevice;
			}
		}

		KdPrint(("[SimpleDriver] MouDeviceObject:%p\n", MouDeviceObject));
		KdPrint(("[SimpleDriver] MouseClassServiceCallback:%p\n", MouseClassServiceCallback));

		ObfDereferenceObject(MouClassDriverObject);
		ObfDereferenceObject(MouHIDDriverObject);
		return STATUS_SUCCESS;
	}

	NTSTATUS InitializeKeyBoard()
	{
		NTSTATUS status;
		PDRIVER_OBJECT KbdClassDriverObject;
		PDRIVER_OBJECT KbdHIDDriverObject;

		status = ObReferenceObjectByName(&KbdClassDriverName, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&KbdClassDriverObject);
		if (!NT_SUCCESS(status)) {
			return status;
		}
		status = ObReferenceObjectByName(&KbdHIDDriverName, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&KbdHIDDriverObject);
		if (!NT_SUCCESS(status))
		{
			if (KbdHIDDriverObject) {
				ObfDereferenceObject(KbdClassDriverObject);
			}
			return status;
		}

		PDEVICE_OBJECT KbdHIDDeviceObject = KbdHIDDriverObject->DeviceObject;

		while (KbdHIDDeviceObject && !KeyboardClassServiceCallback)
		{
			PDEVICE_OBJECT KbdClassDeviceObject = KbdClassDriverObject->DeviceObject;
			while (KbdClassDeviceObject && !KeyboardClassServiceCallback)
			{
				if (!KbdClassDeviceObject->NextDevice && !KbdDeviceObject) {
					KbdDeviceObject = KbdClassDeviceObject;
				}
				PULONG64 KbdHIDDeviceExtensionStart = (PULONG64)KbdHIDDeviceObject->DeviceExtension;
				ULONG64 KbdHIDDeviceExtensionSize = ((ULONG64)KbdHIDDeviceObject->DeviceObjectExtension - (ULONG64)KbdHIDDeviceObject->DeviceExtension) / 4;

				for (ULONG64 i = 0; i < KbdHIDDeviceExtensionSize; i++)
				{
					if (KbdHIDDeviceExtensionStart[i] == (ULONG64)KbdClassDeviceObject && KbdHIDDeviceExtensionStart[i + 1] > (ULONG64)KbdClassDriverObject->DriverStart)
					{
						KeyboardClassServiceCallback = (KeyboardClassServiceCallback_T)(KbdHIDDeviceExtensionStart[i + 1]);
						break;
					}
				}
				KbdClassDeviceObject = KbdClassDeviceObject->NextDevice;
			}
			KbdHIDDeviceObject = KbdHIDDeviceObject->AttachedDevice;
		}
		if (!KbdDeviceObject)
		{
			PDEVICE_OBJECT TmpDeviceObject = KbdClassDriverObject->DeviceObject;
			while (TmpDeviceObject)
			{
				if (!TmpDeviceObject->NextDevice)
				{
					KbdDeviceObject = TmpDeviceObject;
					break;
				}
				TmpDeviceObject = TmpDeviceObject->NextDevice;
			}
		}

		KdPrint(("[SimpleDriver] KbdDeviceObject:%p\n", KbdDeviceObject));
		KdPrint(("[SimpleDriver] KeyboardClassServiceCallback:%p\n", KeyboardClassServiceCallback));

		ObfDereferenceObject(KbdClassDriverObject);
		ObfDereferenceObject(KbdHIDDriverObject);
		return STATUS_SUCCESS;
	}

	NTSTATUS InitializeKbdMou()
	{
		NTSTATUS status;
		status = KbdMou::InitializeMouse();
		if (!NT_SUCCESS(status)) {
			KdPrint(("[SimpleDriver] InitializeMouse failed status:%X\n", status));
		}
		status = KbdMou::InitializeKeyBoard();
		if (!NT_SUCCESS(status)) {
			KdPrint(("[SimpleDriver] InitializeKeyBoard failed status:%X\n", status));
		}
		status = KbdMouTrick::InitializeKbdMouTrick();
		if (!NT_SUCCESS(status)) {
			KdPrint(("[SimpleDriver] InitializeKbdMouTrick failed status:%X\n", status));
		}
		return status;
	}

	NTSTATUS SendMouseInput(PMOUSE_INPUT_DATA MouseInputData)
	{
		ULONG Consumed;
		KIRQL OldIrql;
		MOUSE_INPUT_DATA CopyData = *MouseInputData;

		OldIrql = KfRaiseIrql(DISPATCH_LEVEL);
		MouseClassServiceCallback(MouDeviceObject, &CopyData, &CopyData + 1, &Consumed);
		//__writecr8(OldIrql);
		KeLowerIrql(OldIrql);

		//LARGE_INTEGER Interval = {};
		//Interval.QuadPart = -10000LL;
		//KeDelayExecutionThread(KernelMode, TRUE, &Interval);

		if (Consumed == 0) {
			return STATUS_UNSUCCESSFUL;
		}
		return STATUS_SUCCESS;
	}

	NTSTATUS SendKeyboardInput(PKEYBOARD_INPUT_DATA KeyBoardInputData)
	{
		ULONG Consumed;
		KIRQL OldIrql;
		KEYBOARD_INPUT_DATA CopyData = *KeyBoardInputData;

		OldIrql = KfRaiseIrql(DISPATCH_LEVEL);
		KeyboardClassServiceCallback(KbdDeviceObject, &CopyData, &CopyData + 1, &Consumed);
		KeLowerIrql(OldIrql);

		LARGE_INTEGER Interval = {};
		Interval.QuadPart = -10000LL;
		KeDelayExecutionThread(KernelMode, TRUE, &Interval);

		if (Consumed == 0) {
			return STATUS_UNSUCCESSFUL;
		}
		return STATUS_SUCCESS;
	}

	NTSTATUS SafeSendMouseInput(PMOUSE_INPUT_DATA MouseInputData)
	{
		ULONG Consumed;
		//KIRQL OldIrql;
		MOUSE_INPUT_DATA CopyData = *MouseInputData;

		//OldIrql = KfRaiseIrql(DISPATCH_LEVEL);
		//KbdMouTrick::AsmTrickMouseCall(MouDeviceObject, &CopyData, &CopyData + 1, &Consumed);
		//KeLowerIrql(OldIrql);
		
		KbdMouTrick::AsmCopMouseCall(MouDeviceObject, &CopyData, &CopyData + 1, &Consumed);

		//LARGE_INTEGER Interval = {};
		//Interval.QuadPart = -10000LL;
		//KeDelayExecutionThread(KernelMode, TRUE, &Interval);

		if (Consumed == 0) {
			return STATUS_UNSUCCESSFUL;
		}
		return STATUS_SUCCESS;
	}

	NTSTATUS SafeSendKeyboardInput(PKEYBOARD_INPUT_DATA KeyBoardInputData)
	{
		ULONG Consumed;
		KIRQL OldIrql;
		KEYBOARD_INPUT_DATA CopyData = *KeyBoardInputData;

		OldIrql = KfRaiseIrql(DISPATCH_LEVEL);
		KbdMouTrick::AsmTrickKeyboardCall(KbdDeviceObject, &CopyData, &CopyData + 1, &Consumed);
		KeLowerIrql(OldIrql);

		LARGE_INTEGER Interval = {};
		Interval.QuadPart = -10000LL;
		KeDelayExecutionThread(KernelMode, TRUE, &Interval);

		if (Consumed == 0) {
			return STATUS_UNSUCCESSFUL;
		}
		return STATUS_SUCCESS;
	}

	void TestMouse()
	{
		KbdMou::InitializeMouse();

		MOUSE_INPUT_DATA InputData;
		memset(&InputData, 0, sizeof(InputData));

		Utils::Sleep(1000);

		for (size_t i = 0; i < 5; i++)
		{
			InputData.ButtonFlags = MOUSE_LEFT_BUTTON_DOWN;
			KbdMou::SendMouseInput(&InputData);
			Utils::Sleep(500);

			InputData.LastX = 100;
			InputData.LastY = 0;
			InputData.Flags = MOUSE_MOVE_RELATIVE;

			InputData.ButtonFlags = MOUSE_LEFT_BUTTON_UP;
			KbdMou::SendMouseInput(&InputData);

			InputData.LastX = -100;
			InputData.LastY = 0;
			InputData.Flags = MOUSE_MOVE_RELATIVE;

			Utils::Sleep(1000);
		}
	}

	void TestKeyboard()
	{
		KbdMou::InitializeKeyBoard();

		KEYBOARD_INPUT_DATA InputData;
		memset(&InputData, 0, sizeof(InputData));

		Utils::Sleep(1000);

		for (size_t i = 0; i < 5; i++)
		{
			//InputData
			InputData.MakeCode = 0x1E;	//'A'

			InputData.Flags = KEY_MAKE;
			KbdMou::SendKeyboardInput(&InputData);

			Utils::Sleep(80);

			InputData.Flags = KEY_BREAK;
			KbdMou::SendKeyboardInput(&InputData);

			Utils::Sleep(1000);
		}
	}

};
