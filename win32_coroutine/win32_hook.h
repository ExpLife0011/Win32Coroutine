#pragma once

#include <Windows.h>

/**
 * 获取进程映像基址
 */
HMODULE
PeGetExeImageBase(
);

/**
 * 获取模块数据目录 
 * @param	Module		模块基址
 * @param	Directory	目录类型
 */
PVOID
PeGetModuleDataDirectory(
	HMODULE Module,
	ULONG Directory
);

/**
 * 获取模块导入目录
 * @param	Module		模块基址
 */
PIMAGE_IMPORT_DESCRIPTOR
PeGetModuleImportDirectory(
	HMODULE Module
);

/**
 * 获取模块导出目录
 * @param	Module		模块基址
 */
PIMAGE_EXPORT_DIRECTORY
PeGetModuleExportDirectory(
	HMODULE Module
);

/**
 * 获取模块导入项
 * @param	Module		模块基址
 * @param	ModuleName	模块名称
 */
PIMAGE_IMPORT_DESCRIPTOR
PeGetModuleImportEntry(
	HMODULE Module,
	PSTR ModuleName
);

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
);

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
);

/** 
 * 修改导入地址表项的指针
 */
PVOID
HookCallInIat(
	PVOID* OldRoutineEntry,
	PVOID NewRoutine
);

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
);

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
);

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
);