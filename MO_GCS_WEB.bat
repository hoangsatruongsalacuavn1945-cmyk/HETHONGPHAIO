@echo off
chcp 65001 >nul
title CHEMSTATION AI — GCS WEB DASHBOARD & BAN DO 3D
echo ====================================================================
echo   KHOI DONG TRAM DIEU KHIEN GCS WEB DASHBOARD & BAN DO 3D
echo ====================================================================
echo.
echo [*] Dang khoi dong Web Server cuc bo tai http://localhost:8080 ...
echo [*] Tu dong ket noi cong USB COM12 doc du lieu LoRa thoi gian thuc...
echo.
python gcs_web_server.py
pause
