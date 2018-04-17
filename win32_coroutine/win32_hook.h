#pragma once

#include <Windows.h>

/**
 * ��ȡ����ӳ���ַ
 */
HMODULE
PeGetExeImageBase(
);

/**
 * ��ȡģ������Ŀ¼ 
 * @param	Module		ģ���ַ
 * @param	Directory	Ŀ¼����
 */
PVOID
PeGetModuleDataDirectory(
	HMODULE Module,
	ULONG Directory
);

/**
 * ��ȡģ�鵼��Ŀ¼
 * @param	Module		ģ���ַ
 */
PIMAGE_IMPORT_DESCRIPTOR
PeGetModuleImportDirectory(
	HMODULE Module
);

/**
 * ��ȡģ�鵼��Ŀ¼
 * @param	Module		ģ���ַ
 */
PIMAGE_EXPORT_DIRECTORY
PeGetModuleExportDirectory(
	HMODULE Module
);

/**
 * ��ȡģ�鵼����
 * @param	Module		ģ���ַ
 * @param	ModuleName	ģ������
 */
PIMAGE_IMPORT_DESCRIPTOR
PeGetModuleImportEntry(
	HMODULE Module,
	PSTR ModuleName
);

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
);

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
);

/** 
 * �޸ĵ����ַ�����ָ��
 */
PVOID
HookCallInIat(
	PVOID* OldRoutineEntry,
	PVOID NewRoutine
);

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
);

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
);

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
);