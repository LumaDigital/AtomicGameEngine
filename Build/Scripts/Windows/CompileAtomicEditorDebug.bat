call "%VS140COMNTOOLS%..\..\VC\bin\amd64\vcvars64.bat"
cmake ..\\..\\..\\ -DATOMIC_DEV_BUILD=1 -DPROJECT_PROPERTIES_CXX:STRING="/Zi" -DPROJECT_PROPERTIES_LINKER:STRING="/INCREMENTAL:NO /OPT:REF /OPT:ICF /DEBUG" -G "Visual Studio 14 2015 Win64"
msbuild /m /p:Configuration=Release /p:Platform=x64 Source\AtomicTool\GenerateScriptBindings.vcxproj
msbuild /m Atomic.sln /t:AtomicEditor /t:AtomicPlayer /p:Configuration=Release /p:Platform=x64
