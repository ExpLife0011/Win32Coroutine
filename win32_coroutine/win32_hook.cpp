
#include "win32_hook.h"
#include "win32_sysroutine.h"

/**
 * 获取进程映像基址
 */
HMODULE
PeGetExeImageBase(
) {

	return GetModuleHandleW(NULL);
}

/**
 * 获取模块数据目录 
 * @param	Module		模块基址
 * @param	Directory	目录类型
 */
PVOID
PeGetModuleDataDirectory(
	HMODULE Module,
	ULONG Directory
) {

	PIMAGE_DOS_HEADER DosHeader = (PIMAGE_DOS_HEADER)Module;
	if (DosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		return NULL;

	PIMAGE_NT_HEADERS NtHeader = (PIMAGE_NT_HEADERS)((PUCHAR)Module + DosHeader->e_lfanew);
	if (NtHeader->Signature != IMAGE_NT_SIGNATURE)
		return NULL;

	return (PUCHAR)Module + NtHeader->OptionalHeader.DataDirectory[Directory].VirtualAddress;
}

/**
 * 获取模块导入目录
 * @param	Module		模块基址
 */
PIMAGE_IMPORT_DESCRIPTOR
PeGetModuleImportDirectory(
	HMODULE Module
) {

	return (PIMAGE_IMPORT_DESCRIPTOR)PeGetModuleDataDirectory(Module, IMAGE_DIRECTORY_ENTRY_IMPORT);
}

/**
 * 获取模块导出目录
 * @param	Module		模块基址
 */
PIMAGE_EXPORT_DIRECTORY
PeGetModuleExportDirectory(
	HMODULE Module
) {

	return (PIMAGE_EXPORT_DIRECTORY)PeGetModuleDataDirectory(Module, IMAGE_DIRECTORY_ENTRY_EXPORT);
}

/**
 * 获取模块导入项
 * @param	Module		模块基址
 * @param	ModuleName	模块名称
 */
PIMAGE_IMPORT_DESCRIPTOR
PeGetModuleImportEntry(
	HMODULE Module,
	PSTR ModuleName
) {

	PIMAGE_IMPORT_DESCRIPTOR ImportArray = (PIMAGE_IMPORT_DESCRIPTOR)PeGetModuleImportDirectory(Module);
	if (ImportArray == NULL)
		return NULL;

	while (ImportArray->Name != NULL) {

		if (stricmp(ModuleName, (char*)Module + ImportArray->Name) == 0)
			return ImportArray;

		ImportArray++;
	}

	return NULL;
}

/**
 * 获取名称导入在导入表中的索引
 * @param	Module		模块基址
 * @param	Import		导入目录
 * @param	RoutineName	导入函数
 */
ULONG
PeGetNameImportIndex(
	HMODULE Module,
	PIMAGE_IMPORT_DESCRIPTOR Import,
	PSTR RoutineName
) {

	ULONG Index = 0;
	PIMAGE_THUNK_DATA TrunkData = (PIMAGE_THUNK_DATA)((PUCHAR)Module + Import->OriginalFirstThunk);
	while (TrunkData->u1.AddressOfData != 0) {

		if (((LONG)TrunkData->u1.AddressOfData) < 0) {
			Index++;
			continue;
		}

		PIMAGE_IMPORT_BY_NAME ImportByName = (PIMAGE_IMPORT_BY_NAME)((PUCHAR)Module + TrunkData->u1.AddressOfData);
		if (strcmp(RoutineName, ImportByName->Name) == 0)
			return Index;

		Index++;
		TrunkData++;
	}

	return -1;
}

/**
 * 获取名称导入索引对应的导入地址入口
 * @param	Module		模块基址
 * @param	Import		导入目录
 * @param	IdtIndex	导入索引
 */
PVOID*
PeMapIdtIndexToIatEntry(
	HMODULE Module,
	PIMAGE_IMPORT_DESCRIPTOR Import,
	ULONG IdtIndex
) {

	return (PVOID*)((PUCHAR)Module + Import->FirstThunk + IdtIndex * sizeof(PVOID));
}

/** 
 * 修改导入地址表项的指针
 */
PVOID
HookCallInIat(
	PVOID* OldRoutineEntry, 
	PVOID NewRoutine
) {

	DWORD Pro;
	PVOID OldRoutine;

	if (!VirtualProtect(OldRoutineEntry, sizeof(ULONG_PTR), PAGE_EXECUTE_READWRITE, &Pro))
		return FALSE;

	OldRoutine = InterlockedCompareExchangePointer(OldRoutineEntry, NewRoutine, *(PVOID*)OldRoutineEntry);

	VirtualProtect(OldRoutineEntry, sizeof(ULONG_PTR), Pro, &Pro);

	return OldRoutine;
}

/**
 * Hook一个调用
 * @param	Module			调用所在的模块基址
 * @param	Import			导入模块目录项
 * @param	RoutineName		导入函数名
 * @param	NewRoutine		新的函数地址
 */
PVOID
HookSingleCall(
	HMODULE Module,
	PIMAGE_IMPORT_DESCRIPTOR Import,
	PSTR RoutineName,
	PVOID NewRoutine
) {

	PVOID Entry;
	ULONG Index;

	//挂钩CreateFileW
	Index = PeGetNameImportIndex(Module, Import, RoutineName);
	if (Index == -1)
		return NULL;

	Entry = PeMapIdtIndexToIatEntry(Module, Import, Index);

	return HookCallInIat((PVOID*)Entry, NewRoutine);
}

/**
 * 对Win32API进行hook
 */
BOOLEAN
CoSetupWin32ApiHook(
) {

	PVOID Entry;
	ULONG Index;
	HMODULE SelfBase = PeGetExeImageBase();

	PIMAGE_IMPORT_DESCRIPTOR Import = PeGetModuleImportEntry(SelfBase, "Kernel32.dll");
	if (Import == NULL)
		return FALSE;

	//挂钩CreateFileW，如果没有这个，下面两个也不用考虑了
	System_CreateFileW = (Routine_CreateFileW)HookSingleCall(SelfBase, Import, "CreateFileW", Coroutine_CreateFileW);
	if (System_CreateFileW) {

		//挂钩ReadFile
		System_ReadFile = (Routine_ReadFile)HookSingleCall(SelfBase, Import, "ReadFile", Coroutine_ReadFile);
		
		//挂钩WriteFile
		System_WriteFile = (Routine_WriteFile)HookSingleCall(SelfBase, Import, "WriteFile", Coroutine_WriteFile);

		//挂钩DeviceIoControl
		System_DeviceIoControl = (Routine_DeviceIoControl)HookSingleCall(SelfBase, Import, "DeviceIoControl", Coroutine_DeviceIoControl);
	}

	return TRUE;
}