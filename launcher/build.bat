@echo off
echo ===================================================
echo Compiling Astra Client Launcher (Native C++)
echo ===================================================
echo.

where g++ >nul 2>nul
if %errorlevel%==0 (
    echo [Compiler] Found GCC/g++ compiler!
    echo [Build] Compiling launcher.cpp...
    g++ -std=c++20 launcher.cpp -o AstraLauncher.exe -lws2_32 -lcrypt32
    if %errorlevel%==0 (
        echo [Success] Successfully built AstraLauncher.exe!
    ) else (
        echo [Error] Compile failed. Make sure you have WinSock and Crypto libraries.
    )
    goto end
)

where cl >nul 2>nul
if %errorlevel%==0 (
    echo [Compiler] Found MSVC/cl compiler!
    echo [Build] Compiling launcher.cpp...
    cl /std:c++20 /EHsc launcher.cpp /FeAstraLauncher.exe ws2_32.lib crypt32.lib
    if %errorlevel%==0 (
        echo [Success] Successfully built AstraLauncher.exe!
    ) else (
        echo [Error] Compile failed.
    )
    goto end
)

echo [Error] Neither GCC (g++) nor MSVC (cl) was found on your Windows PATH.
echo Please install MinGW-w64 (GCC) or Visual Studio Build Tools to compile.

:end
pause
