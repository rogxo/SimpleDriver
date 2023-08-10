#include "kbdmou.h"
#include <Windows.h>
#include "ntdll.h"


bool Request(PREQUEST req) {
	typedef struct _SYSTEM_FIRMWARE_TABLE_INFORMATION {
		ULONG		ProviderSignature;
		ULONG		Action;
		ULONG		TableID;
		ULONG		TableBufferLength;
		PVOID		TableBuffer;
	} SYSTEM_FIRMWARE_TABLE_INFORMATION, * PSYSTEM_FIRMWARE_TABLE_INFORMATION;

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


KbdMou::KbdMou()
{
	this->device = CreateFileA(R"(\\.\PciControl)", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

KbdMou::~KbdMou()
{
	if (this->device) {
		CloseHandle(this->device);
	}
}

bool KbdMou::send_mouse_input(PMOUSE_INPUT_DATA input_data)
{
	REQUEST req = { OPERATION::MouseInput, input_data };
	//bool status = DeviceIoControl(this->device, 0x9090990A, &req, sizeof(REQUEST), NULL, 0, NULL, NULL);
	bool status = Request(&req);
	return status;
}

bool KbdMou::send_keyboard_input(PKEYBOARD_INPUT_DATA input_data)
{
	REQUEST req = { OPERATION::KeyboardInput, input_data };
	//bool status = DeviceIoControl(this->device, 0x9090990A, &req, sizeof(REQUEST), NULL, 0, NULL, NULL);
	bool status = Request(&req);
	return status;
}

bool KbdMou::mouse_move(int x, int y, int flag)
{
	MOUSE_INPUT_DATA input_data = { 0 };
	input_data.Flags = flag;
	switch (flag)
	{
	case MOUSE_MOVE_RELATIVE:
		input_data.LastX = x;
		input_data.LastY = y;
		break;
	case MOUSE_MOVE_ABSOLUTE:
		input_data.LastX = x * 0xffff / GetSystemMetrics(SM_CXSCREEN);
		input_data.LastY = y * 0xffff / GetSystemMetrics(SM_CYSCREEN);
		break;
	default:
		return false;
	}
	return send_mouse_input(&input_data);
}

bool KbdMou::left_button_down()
{
	MOUSE_INPUT_DATA input_data = { 0 };
	input_data.ButtonFlags = MOUSE_LEFT_BUTTON_DOWN;
	return send_mouse_input(&input_data);
}

bool KbdMou::left_button_up()
{
	MOUSE_INPUT_DATA input_data = { 0 };
	input_data.ButtonFlags = MOUSE_LEFT_BUTTON_UP;
	return send_mouse_input(&input_data);
}

bool KbdMou::left_button_press(int delay)
{
	left_button_down();
	Sleep(delay);
	left_button_up();
	return true;
}

bool KbdMou::right_button_down()
{
	MOUSE_INPUT_DATA input_data = { 0 };
	input_data.ButtonFlags = MOUSE_RIGHT_BUTTON_DOWN;
	return send_mouse_input(&input_data);
}

bool KbdMou::right_button_up()
{
	MOUSE_INPUT_DATA input_data = { 0 };
	input_data.ButtonFlags = MOUSE_RIGHT_BUTTON_UP;
	return send_mouse_input(&input_data);
}

bool KbdMou::right_button_press(int delay)
{
	right_button_down();
	Sleep(delay);
	right_button_up();
	return true;
}

bool KbdMou::key_down(int vk_code)
{
	KEYBOARD_INPUT_DATA input_data = { 0 };
	input_data.Flags = KEY_MAKE;
	input_data.MakeCode = MapVirtualKey(vk_code, MAPVK_VK_TO_VSC);
	return send_keyboard_input(&input_data);
}

bool KbdMou::key_up(int vk_code)
{
	KEYBOARD_INPUT_DATA input_data = { 0 };
	input_data.Flags = KEY_BREAK;
	input_data.MakeCode = MapVirtualKey(vk_code, MAPVK_VK_TO_VSC);
	return send_keyboard_input(&input_data);
}

bool KbdMou::key_press(int vk_code, int delay)
{
	key_down(vk_code);
	Sleep(delay);
	key_up(vk_code);
	return true;
}

