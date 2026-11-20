@echo off
chcp 65001 >nul
title KHOI DONG HE THONG QUAN TRAC THUY VAN USV - AQUA SENTINEL 4G & LORA

echo =======================================================================
echo    HE THONG QUAN TRAC THUY VAN TU HANH USV BUOY - CHEMSTATION AI
echo    (KET NOI DUAL-LINK: 4G LTE VIETTEL & LORA LONG-RANGE TRAM BO GCS)
echo =======================================================================
echo.

cd /d "%~dp0"

echo [1/3] Dang kiem tra moi truong Python va Serial...
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [LOI] Khong tim thay Python tren he thong. Vui long cai dat Python 3.10+.
    pause
    exit /b 1
)

echo [2/3] Dang khoi dong Tram Bo GCS Web Server tren cong COM tu dong...
start /b python gcs_web_server.py --no-browser COM11

timeout /t 2 /nobreak >nul

echo [3/3] Dang mo Giao Dien Dashboard Aqua-Sentinel tren trinh duyet...
start http://localhost:8080/sentinel

echo.
echo =======================================================================
echo  [OK] HE THONG DA SAN SANG HOAT DONG!
echo  - Dashboard Aqua-Sentinel HUD:  http://localhost:8080/sentinel
echo  - Ban Do 3D WebGL Truc Quan:    http://localhost:8080/3d
echo  - Dashboard Mobile Cloud:       http://localhost:8080/cloud
echo  - Co So Du Lieu Google Sheets: https://docs.google.com/spreadsheets/d/188ibulljtyfxsrM7BwbESRmD0rsrUSVqY60F3cL4FII
echo =======================================================================
echo  (Nhan Ctrl+C de dung he thong khi ket thuc phien lam viec)
echo.

REM Giu cua so console de theo doi log
cmd /k
