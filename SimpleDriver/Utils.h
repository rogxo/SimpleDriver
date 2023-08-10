#pragma once
#include <ntifs.h>

namespace Utils
{
#pragma warning(disable:4127)
	template<class T> T __ROL__(T value, int count)
	{
		const unsigned int nbits = sizeof(T) * 8;
		if (count > 0)
		{
			count %= nbits;
			T high = value >> (nbits - count);
			if (T(-1) < 0) // This will be a signed val.
				high &= ~((T(-1) << count));
			value <<= count;
			value |= high;
		}
		else
		{
			count = -count % nbits;
			T low = value << (nbits - count);
			value >>= count;
			value |= low;
		}
		return value;
	}

	PVOID GetModuleBase(PCHAR szModuleName);
	BOOLEAN RtlCaptureAnsiString(PUNICODE_STRING DestinationString, PCSZ SourceString, BOOLEAN AllocateDestinationString);
	PDRIVER_OBJECT GetDriverObjectByName(PWCHAR DriverName);
	ULONG64 FindPattern(ULONG64 base, SIZE_T size, PCHAR pattern);
	ULONG64 GetImageSectionByName(PVOID ImageBase, PCHAR SectionName, SIZE_T* SizeOut);
	ULONG64 FindPatternImage(PVOID ImageBase, PCHAR SectionName, PCHAR Pattern);
	ULONG64 FindPatternImage(PCHAR ModuleName, PCHAR SectionName, PCHAR Pattern);
	NTSTATUS WriteToReadOnly(PVOID destination, PVOID buffer, ULONG size);

	VOID Sleep(ULONG Milliseconds);
};

