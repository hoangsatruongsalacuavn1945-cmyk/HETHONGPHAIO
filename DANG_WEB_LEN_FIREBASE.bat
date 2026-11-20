@echo off
title DANG TAI WEBSITE LEN GOOGLE FIREBASE HOSTING
echo =======================================================================
echo    DANG TAI HE THONG QUAN TRAC USV LEN GOOGLE FIREBASE CLOUD
echo    Du an: USV-Aqua-Sentinel
echo =======================================================================
echo.
cd /d "%~dp0"

echo [1/3] Dang dong bo toan bo giao dien HTML vao thu muc public...
if not exist public mkdir public
copy /Y aqua_sentinel_dashboard.html public\index.html >nul
copy /Y aqua_sentinel_dashboard.html public\sentinel.html >nul
copy /Y google_satellite_tracker.html public\map.html >nul
copy /Y water_map_3d.html public\water_map_3d.html >nul
if exist cloud_dashboard_mobile.html copy /Y cloud_dashboard_mobile.html public\cloud_dashboard_mobile.html >nul
if exist console_dashboard.html copy /Y console_dashboard.html public\console.html >nul

echo.
echo [2/3] Dang kiem tra dang nhap Google...
call firebase login

echo.
echo [3/3] Dang tai website len Google Hosting...
call firebase deploy --only hosting

echo.
echo =======================================================================
echo  [THANH CONG] Website da duoc cap nhat len Google Cloud thanh cong!
echo.
echo  Link Dashboard tong quan:
echo  https://usv-aqua-sentinel.web.app
echo.
echo  Link Ban do Ve tinh Google Maps:
echo  https://usv-aqua-sentinel.web.app/map
echo.
echo  Link Mo phong 3D WebGL:
echo  https://usv-aqua-sentinel.web.app/3d
echo =======================================================================
pause
