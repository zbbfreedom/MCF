// Copyleft 2013, LH_Mouse. All wrongs reserved.

#include "PreCompiled.hpp"

namespace MCFBuild {
	extern PROJECT LoadProject(
		const std::wstring &wcsProjFile,
		const std::wstring &wcsConfig,
		std::map<std::wstring, std::wstring> &&mapOverridingVars,
		const std::wstring &wcsOutputPath,
		bool bVerbose
	);
	extern FOLDER_TREE GetSourceTree(
		const std::wstring &wcsPath,
		const PROJECT &Project,
		bool bVerbose
	);
	extern BUILD_JOBS CountBuildJobs(
		const std::wstring &wcsSrcRoot,
		const FOLDER_TREE &SrcTree,
		const std::wstring &wcsDstRoot,
		const PROJECT &Project,
		unsigned long ulProcessCount,
		bool bRebuildAll,
		bool bVerbose
	);
	extern void CompileAndLink(
		const BUILD_JOBS &BuildJobs,
		const PROJECT &Project,
		unsigned long ulProcessCount,
		bool bVerbose
	);

	void DoBuild(
		const std::wstring &wcsProjFile,
		const std::wstring &wcsConfig,
		const std::wstring &wcsSrcRoot,
		const std::wstring &wcsDstRoot,
		const std::wstring &wcsOutputPath,
		unsigned long ulProcessCount,
		const std::map<std::wstring, std::wstring> &mapVars,
		bool bVerbose,
		bool bRebuildAll
	){
		Output(L"正在配置构建环境...");
		std::map<std::wstring, std::wstring> mapOverridingVars(mapVars);
		mapOverridingVars[L"_CONFIG"]	= L'\"' + wcsConfig + L'\"';
		mapOverridingVars[L"_SRCDIR"]	= L'\"' + wcsSrcRoot + L'\"';
		mapOverridingVars[L"_OBJDIR"]	= L'\"' + wcsDstRoot + L'\"';
		const auto Project = LoadProject(wcsProjFile, wcsConfig, std::move(mapOverridingVars), wcsOutputPath, bVerbose);

		Output(L"----------------------------------------");

		Output(L"正在分析源文件目录树...");
		const auto SourceTree = GetSourceTree(wcsSrcRoot, Project, bVerbose);

		Output(L"----------------------------------------");

		Output(L"正在统计构建任务...");
		const auto BuildJobs = CountBuildJobs(wcsSrcRoot, SourceTree, wcsDstRoot, Project, ulProcessCount, bRebuildAll, bVerbose);

		Output(L"----------------------------------------");

		Output(L"开始构建...");
		CompileAndLink(BuildJobs, Project, ulProcessCount, bVerbose);
	}
}
