#include <ntifs.h>


namespace Comm {

#pragma pack(push, 8)
	typedef struct _REQUEST {
		ULONG Operation;
		PVOID Instruction;
	} REQUEST, * PREQUEST;
#pragma pack(pop)

	NTSTATUS RequestHandler(PREQUEST Request);

	namespace DeviceIoControl {
		NTSTATUS DispatchIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
		NTSTATUS Initialize(PDRIVER_OBJECT DriverObject);
		NTSTATUS Unload();
	}

	namespace IrpHijack {
		NTSTATUS DetourDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
		NTSTATUS Initialize();
		NTSTATUS Unload();
	}
};