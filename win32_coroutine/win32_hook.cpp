
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
 * ��Win32API����hook
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

	//�ҹ�CreateFileW
	Index = PeGetNameImportIndex(SelfBase, Import, "CreateFileW");
	if (Index == -1)
		return FALSE;

	Entry = PeMapIdtIndexToIatEntry(SelfBase, Import, Index);

	System_CreateFileW = (Routine_CreateFileW)HookCallInIat((PVOID*)Entry, Coroutine_CreateFileW);
	
	
	//�ҹ�ReadFile
	Index = PeGetNameImportIndex(SelfBase, Import, "ReadFile");
	if (Index == -1)
		return FALSE;

	Entry = PeMapIdtIndexToIatEntry(SelfBase, Import, Index);

	System_ReadFile = (Routine_ReadFile)HookCallInIat((PVOID*)Entry, Coroutine_ReadFile);

	return TRUE;
}