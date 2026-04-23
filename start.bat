@echo off
echo [1/3] Compiling main.cpp...
g++ main.cpp -o main.exe
if errorlevel 1 (
    echo Compilation failed!
    pause
    exit /b 1
)

echo [2/3] Starting dashboard server at http://localhost:8080/dashboard.html
start "" cmd /k "python serve.py"
timeout /t 1 >nul

echo [3/3] Opening dashboard in browser...
start "" http://localhost:8080/dashboard.html
timeout /t 1 >nul

echo.
echo Dashboard is live. Now run the simulation:
echo.
main.exe
