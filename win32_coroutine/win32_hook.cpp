
#include "win32_hook.h"
#include "win32_sysroutine.h"

/**
 * ��ȡ����ӳ���ַ
 */
HMODULE
PeGetExeImageBase(
) {

	return GetModuleHandleW(NULL);
}

/**
 * ��ȡģ������Ŀ¼ 
 * @param	Module		ģ���ַ
 * @param	Directory	Ŀ¼����
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
 * ��ȡģ�鵼��Ŀ¼
 * @param	Module		ģ���ַ
 */
PIMAGE_IMPORT_DESCRIPTOR
PeGetModuleImportDirectory(
	HMODULE Module
) {

	return (PIMAGE_IMPORT_DESCRIPTOR)PeGetModuleDataDirectory(Module, IMAGE_DIRECTORY_ENTRY_IMPORT);
}

/**
 * ��ȡģ�鵼��Ŀ¼
 * @param	Module		ģ���ַ
 */
PIMAGE_EXPORT_DIRECTORY
PeGetModuleExportDirectory(
	HMODULE Module
) {

	return (PIMAGE_EXPORT_DIRECTORY)PeGetModuleDataDirectory(Module, IMAGE_DIRECTORY_ENTRY_EXPORT);
}

/**
 * ��ȡģ�鵼����
 * @param	Module		ģ���ַ
 * @param	ModuleName	ģ������
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
 * ��ȡ��ŵ����Ӧ��ʵ�ʺ�������
 * @param	Module		ģ���ַ
 * @param	Import		����Ŀ¼
 * @param	RoutineName	���뺯��
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
 * ��ȡ���Ƶ����ڵ�����е�����
 * @param	Module		ģ���ַ
 * @param	Import		����Ŀ¼
 * @param	RoutineName	���뺯��
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
 * ��ȡ���Ƶ���������Ӧ�ĵ����ַ���
 * @param	Module		ģ���ַ
 * @param	Import		����Ŀ¼
 * @param	IdtIndex	��������
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
 * �޸ĵ����ַ�����ָ��
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
 * Hookһ������
 * @param	Module			�������ڵ�ģ���ַ
 * @param	Import			����ģ��Ŀ¼��
 * @param	RoutineName		���뺯����
 * @param	NewRoutine		�µĺ�����ַ
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

	//�ҹ�CreateFileW
	Index = PeGetNameImportIndex(Module, Import, RoutineName);
	if (Index == -1)
		return NULL;

	Entry = PeMapIdtIndexToIatEntry(Module, Import, Index);

	return HookCallInIat((PVOID*)Entry, NewRoutine);
}

/**
 * ���ļ�IO��������hook
 * ������
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

	//�ҹ�CreateFileW�����û���������������Ҳ���ÿ�����
	System_CreateFileW = (Routine_CreateFileW)HookSingleCall(ImageBase, Import, "CreateFileW", Coroutine_CreateFileW);
	if (System_CreateFileW) {

		//�ҹ�ReadFile
		System_ReadFile = (Routine_ReadFile)HookSingleCall(ImageBase, Import, "ReadFile", Coroutine_ReadFile);
		
		//�ҹ�WriteFile
		System_WriteFile = (Routine_WriteFile)HookSingleCall(ImageBase, Import, "WriteFile", Coroutine_WriteFile);

		//�ҹ�DeviceIoControl
		System_DeviceIoControl = (Routine_DeviceIoControl)HookSingleCall(ImageBase, Import, "DeviceIoControl", Coroutine_DeviceIoControl);
	}


	return TRUE;
}

/**
 * ��socket��������hook
 * ������
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

	//�ҹ�socket
	System_socket = (Routine_socket)HookSingleCall(ImageBase, Import, "socket", Coroutine_socket);
	if (System_socket) {

		//�ҹ�accept
		System_accept = (Routine_accept)HookSingleCall(ImageBase, Import, "accept", Coroutine_accept);

		//�ҹ�send
		System_send = (Routine_send)HookSingleCall(ImageBase, Import, "send", Coroutine_send);

		//�ҹ�recv
		System_recv = (Routine_recv)HookSingleCall(ImageBase, Import, "recv", Coroutine_recv);

	}

	return TRUE;
}