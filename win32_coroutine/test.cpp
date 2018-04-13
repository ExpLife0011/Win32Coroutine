#pragma once

#include "stdafx.h"
#include "win32_coroutine.h"

int main()
{

	HMODULE SelfBase = PeGetExeImageBase();

	PIMAGE_IMPORT_DESCRIPTOR Import = PeGetModuleImportEntry(SelfBase, "Kernel32.dll");
	ULONG Index = PeGetNameImportIndex(SelfBase, Import, "CreateFileW");
	if (Index == -1)
		return 1;

	PVOID Entry = ((PUCHAR)SelfBase + Import->FirstThunk + Index * sizeof(PVOID));

	System_CreateFileW = (Routine_CreateFileW)*(PVOID*)Entry;
	HookCallInIat((PVOID*)Entry, Coroutine_CreateFileW);

	Index = PeGetNameImportIndex(SelfBase, Import, "ReadFile");
	if (Index == -1)
		return 1;

	Entry = PeMapIdtIndexToIatEntry(SelfBase, Import, Index);
	System_ReadFile = (Routine_ReadFile)*(PVOID*)Entry;

	HookCallInIat((PVOID*)Entry, Coroutine_ReadFile);

	HANDLE CoHandle = CoCreateCoroutine(FALSE);

	getchar();
	return 0;
}