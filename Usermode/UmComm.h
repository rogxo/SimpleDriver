/*++

Copyright (c) 2020-2025, Rog. All rights reserved.

Author:
	Rog

License:
	MIT

--*/
#include <Windows.h>

namespace UsermodeComm {

#pragma pack(push, 8)
	typedef struct _REQUEST {
		ULONG Operation;
		PVOID Instruction;
	} REQUEST, * PREQUEST;

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

	namespace DeviceIoControl
	{
		bool Initialize();
		bool Request(PREQUEST req);
		bool Unload();
	}

	namespace IrpHijack
	{
		bool Initialize();
		bool Request(PREQUEST req);
		bool Unload();
	}

	namespace SystemFirmwateTable
	{
		bool Initialize();
		bool Request(PREQUEST req);
		bool Unload();
	}

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

#pragma warning(disable:4201)
	typedef struct _KEYBOARD_INPUT_DATA {
		USHORT UnitId;
		USHORT MakeCode;
		USHORT Flags;
		USHORT Reserved;
		ULONG ExtraInformation;
	} KEYBOARD_INPUT_DATA, * PKEYBOARD_INPUT_DATA;

	typedef struct _MOUSE_INPUT_DATA {
		USHORT UnitId;
		USHORT Flags;
		union {
			ULONG Buttons;
			struct {
				USHORT ButtonFlags;
				USHORT ButtonData;
			};
		};
		ULONG  RawButtons;
		LONG   LastX;
		LONG   LastY;
		ULONG  ExtraInformation;
	} MOUSE_INPUT_DATA, * PMOUSE_INPUT_DATA;
}