
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
 * 获取序号导入对应的实际函数名称
 * @param	Module		模块基址
 * @param	Import		导入目录
 * @param	RoutineName	导入函数
 */
PCHAR
PeGetOrdinalImportName(
	HMODULE Module,
	PIMAGE_IMPORT_DESCRIPTOR Import,
	DWORD Ordinal
) {

	HMODULE BaseAddress = GetModuleHandleA((PSTR)Module + Import->Name);
	if (BaseAddress == NULL)
		return NULL;

	PIMAGE_EXPORT_DIRECTORY Export = PeGetModuleExportDirectory(BaseAddress);
	if (Ordinal > Export->NumberOfFunctions)
		return NULL;
	
	PDWORD NameTable = (PDWORD)((PUCHAR)BaseAddress + Export->AddressOfNames);
	PUSHORT OrdinalTable = (PUSHORT)((PUCHAR)BaseAddress + Export->AddressOfNameOrdinals);

	for (int i = 0;i < Export->NumberOfNames;i++) {
		if (OrdinalTable[i] == Ordinal - Export->Base)
			return (PCHAR)BaseAddress + NameTable[i];
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

	PCHAR Name;
	ULONG Index = 0;
	PIMAGE_THUNK_DATA TrunkData = (PIMAGE_THUNK_DATA)((PUCHAR)Module + Import->OriginalFirstThunk);
	while (TrunkData->u1.AddressOfData != 0) {

#ifdef _WIN64
		if (TrunkData->u1.AddressOfData & 0x8000000000000000) {
			Name = PeGetOrdinalImportName(Module, Import, TrunkData->u1.AddressOfData & (0x8000000000000000 - 1));
		}
#else
		if (TrunkData->u1.AddressOfData & 0x80000000) {
			Name = PeGetOrdinalImportName(Module, Import, TrunkData->u1.AddressOfData & (0x8000000 - 1));
		}
#endif
		else {
			PIMAGE_IMPORT_BY_NAME ImportByName = (PIMAGE_IMPORT_BY_NAME)((PUCHAR)Module + TrunkData->u1.AddressOfData);
			Name = ImportByName->Name;
		}
		if (strcmp(RoutineName, Name) == 0)
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
 * 对文件IO函数进行hook
 * 包括：
 *	CreateFileW
 *	ReadFile
 *	WriteFile
 *	DeviceIoControl
 */
BOOLEAN
CoSetupFileIoHook(
	PWSTR ModuleName
) {

	HMODULE ImageBase = GetModuleHandleW(ModuleName);

	PIMAGE_IMPORT_DESCRIPTOR Import = PeGetModuleImportEntry(ImageBase, "Kernel32.dll");
	if (Import == NULL)
		return FALSE;

	//挂钩CreateFileW，如果没有这个，下面两个也不用考虑了
	System_CreateFileW = (Routine_CreateFileW)HookSingleCall(ImageBase, Import, "CreateFileW", Coroutine_CreateFileW);
	if (System_CreateFileW) {

		//挂钩ReadFile
		System_ReadFile = (Routine_ReadFile)HookSingleCall(ImageBase, Import, "ReadFile", Coroutine_ReadFile);
		
		//挂钩WriteFile
		System_WriteFile = (Routine_WriteFile)HookSingleCall(ImageBase, Import, "WriteFile", Coroutine_WriteFile);

		//挂钩DeviceIoControl
		System_DeviceIoControl = (Routine_DeviceIoControl)HookSingleCall(ImageBase, Import, "DeviceIoControl", Coroutine_DeviceIoControl);
	}


	return TRUE;
}

/**
 * 对socket函数进行hook
 * 包括：
 *	socket
 *	accept
 *	send
 *	recv
 */
BOOLEAN
CoSetupNetIoHook(
	PWSTR ModuleName
) {

	HMODULE ImageBase = GetModuleHandleW(ModuleName);

	PIMAGE_IMPORT_DESCRIPTOR Import = PeGetModuleImportEntry(ImageBase, "Ws2_32.dll");
	if (Import == NULL)
		return FALSE;

	//挂钩socket
	System_socket = (Routine_socket)HookSingleCall(ImageBase, Import, "socket", Coroutine_socket);
	if (System_socket) {

		//挂钩accept
		System_accept = (Routine_accept)HookSingleCall(ImageBase, Import, "accept", Coroutine_accept);

		//挂钩send
		System_send = (Routine_send)HookSingleCall(ImageBase, Import, "send", Coroutine_send);

		//挂钩recv
		System_recv = (Routine_recv)HookSingleCall(ImageBase, Import, "recv", Coroutine_recv);

	}

	return TRUE;
}