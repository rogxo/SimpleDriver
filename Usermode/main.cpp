#include <iostream>
#include <Windows.h>
#include <string>
#include <tuple>
#include <vector>
#include <format>
#include "UmComm.h"
#include "KbdMouDef.h"
#include "kbdmou.h"


uint64_t val = 0x123456789abcdef;


void test_read() {
	uint64_t buffer = 0;

	UsermodeComm::COPY_MEMORY cm;
	cm.ProcessId = (HANDLE)GetCurrentProcessId();
	cm.VirtualAddress = &val;
	cm.Buffer = &buffer;
	cm.Size = 0x8;
	//cm.Size = 0x40000000;	//1G

	UsermodeComm::REQUEST req;
	req.Operation = UsermodeComm::OPERATION::ReadMem;
	req.Instruction = &cm;

	//UsermodeComm::IrpHijack::Initialize();
	//UsermodeComm::DeviceIoControl::Initialize();
	UsermodeComm::SystemFirmwateTable::Request(&req);

	uint64_t time = GetTickCount64();
	size_t i;
	for (i = 0; i < 1000000; i++)
	{
		//UsermodeComm::IrpHijack::Request(&req);
		//UsermodeComm::DeviceIoControl::Request(&req);
		UsermodeComm::SystemFirmwateTable::Request(&req);
	}
	//UsermodeComm::IrpHijack::Unload();
	//UsermodeComm::DeviceIoControl::Unload();
	UsermodeComm::SystemFirmwateTable::Request(&req);

	std::cout << std::format("result:{:X} read {} times cost:{:.2f}s", buffer, i, (GetTickCount64() - time) / 1000.f) << std::endl;
}

void test_write() {
	uint64_t buffer = 0xDEADC0DE;

	UsermodeComm::COPY_MEMORY cm;
	cm.ProcessId = (HANDLE)GetCurrentProcessId();
	cm.VirtualAddress = &val;
	cm.Buffer = &buffer;
	cm.Size = 0x8;
	//cm.Size = 0x40000000;	//1G

	UsermodeComm::REQUEST req;
	req.Operation = UsermodeComm::OPERATION::WriteMem;
	req.Instruction = &cm;

	//UsermodeComm::IrpHijack::Initialize();
	//UsermodeComm::DeviceIoControl::Initialize();

	uint64_t time = GetTickCount64();
	size_t i;
	for (i = 0; i < 1000000; i++)
	{
		//UsermodeComm::IrpHijack::Request(&req);
		//UsermodeComm::DeviceIoControl::Request(&req);
		UsermodeComm::SystemFirmwateTable::Request(&req);
	}
	//UsermodeComm::IrpHijack::Unload();
	//UsermodeComm::DeviceIoControl::Unload();

	std::cout << std::format("result:{:X} write {} times cost:{:.2f}s", val, i, (GetTickCount64() - time) / 1000.f) << std::endl;
}

void test_module() {
	
	UsermodeComm::MODULE_BASE mb;
	mb.ProcessId = (HANDLE)3544;
	mb.ModuleName = (char*)"explorer.exe";

	UsermodeComm::REQUEST req;
	req.Operation = UsermodeComm::OPERATION::ModuleBase;
	req.Instruction = &mb;

	//UsermodeComm::IrpHijack::Initialize();
	//UsermodeComm::IrpHijack::Request(&req);
	//UsermodeComm::IrpHijack::Unload();

	UsermodeComm::SystemFirmwateTable::Request(&req);

	std::cout << std::format("module base:{:X}", (uint64_t)mb.PBase) << std::endl;
}

void test_mouse() {
	
	UsermodeComm::MOUSE_INPUT_DATA InputData;
	UsermodeComm::REQUEST req;

	UsermodeComm::IrpHijack::Initialize();

	req.Operation = UsermodeComm::OPERATION::MouseInput;
	req.Instruction = &InputData;

	for (size_t i = 0; i < 10; i++)
	{
		InputData.ButtonFlags = MOUSE_LEFT_BUTTON_DOWN;
		UsermodeComm::IrpHijack::Request(&req);
		//UsermodeComm::SystemFirmwateTable::Request(&req);
		Sleep(500);

		InputData.LastX = 100;
		InputData.LastY = 0;
		InputData.Flags = MOUSE_MOVE_RELATIVE;

		InputData.ButtonFlags = MOUSE_LEFT_BUTTON_UP;
		UsermodeComm::IrpHijack::Request(&req);
		//UsermodeComm::SystemFirmwateTable::Request(&req);

		InputData.LastX = -100;
		InputData.LastY = 0;
		InputData.Flags = MOUSE_MOVE_RELATIVE;

		Sleep(1000);
	}

	//UsermodeComm::IrpHijack::Unload();
}

int main()
{
	//test_read();
	//test_write();
	//test_module();
	
	//test_mouse();

	Sleep(1000);

	KbdMou km;

	km.left_button_down();
	for (size_t i = 0; i < 5; i++)
	{
		km.mouse_move(100, 0, MOUSE_MOVE_RELATIVE);
		Sleep(500);

		km.mouse_move(-100, 0, MOUSE_MOVE_RELATIVE);
		Sleep(500);
	}
	km.left_button_up();

	//for (size_t i = 0; i < 5; i++)
	//{
	//	km.key_press('A', 80);
	//	Sleep(500);
	//}


	system("pause");

	return 0;
}
